#include "banking.h"
#include "common.h"
#include "ipc_local.h"
#include "mutex_local.h"
#include "pa2345.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

static void make_message(Message *message, MessageType type,
                         const char *payload, int length) {
  memset(message, 0, sizeof(*message));
  message->s_header.s_magic = MESSAGE_MAGIC;
  message->s_header.s_type = type;
  message->s_header.s_payload_len = (uint16_t)length;
  if (length > 0)
    memcpy(message->s_payload, payload, (size_t)length);
}

static int wait_for_children(IpcContext *ctx, MessageType type) {
  for (int sender = 1; sender < ctx->process_count; ++sender) {
    Message message;
    if (receive(ctx, (local_id)sender, &message) ||
        message.s_header.s_type != (int16_t)type)
      return 1;
  }
  return 0;
}

static int child_main(local_id id, int count, element pipes[count][count],
                      int events_fd, int use_mutex) {
  IpcContext ctx;
  Message message;
  char text[MAX_PAYLOAD_LEN];

  memset(&ctx, 0, sizeof(ctx));
  ctx.self_id = id;
  ctx.process_count = count;
  ctx.last_sender = -1;
  ctx.pipes = &pipes[0][0];

  timestamp_t time = get_lamport_time() + 1;
  int length = snprintf(text, sizeof(text), log_started_fmt, time, id,
                        (int)getpid(), (int)getppid(), 0);
  if (length < 0 || length >= (int)sizeof(text) || log_event(events_fd, text))
    return 1;

  make_message(&message, STARTED, text, length);
  if (send_multicast(&ctx, &message))
    return 1;

  for (int sender = 1; sender < count; ++sender) {
    if (sender == id)
      continue;
    if (receive(&ctx, (local_id)sender, &message) ||
        message.s_header.s_type != STARTED)
      return 1;
  }

  length = snprintf(text, sizeof(text), log_received_all_started_fmt,
                    get_lamport_time(), id);
  if (length < 0 || length >= (int)sizeof(text) || log_event(events_fd, text))
    return 1;

  int iterations = id * 5;
  for (int iteration = 1; iteration <= iterations; ++iteration) {
    if (use_mutex && request_cs(&ctx))
      return 1;

    length = snprintf(text, sizeof(text), log_loop_operation_fmt, id,
                      iteration, iterations);
    if (length < 0 || length >= (int)sizeof(text))
      return 1;
    print(text);

    if (use_mutex && release_cs(&ctx))
      return 1;
  }

  time = get_lamport_time() + 1;
  length = snprintf(text, sizeof(text), log_done_fmt, time, id, 0);
  if (length < 0 || length >= (int)sizeof(text) || log_event(events_fd, text))
    return 1;

  make_message(&message, DONE, text, length);
  if (send_multicast(&ctx, &message))
    return 1;
  ctx.done_received[(int)id] = 1;

  for (;;) {
    int all_done = 1;
    for (int child = 1; child < count; ++child) {
      if (!ctx.done_received[child]) {
        all_done = 0;
        break;
      }
    }
    if (all_done)
      break;

    if (receive_any(&ctx, &message) || mutex_handle_one(&ctx, &message))
      return 1;
  }

  length = snprintf(text, sizeof(text), log_received_all_done_fmt,
                    get_lamport_time(), id);
  if (length < 0 || length >= (int)sizeof(text) || log_event(events_fd, text))
    return 1;

  return close(events_fd) == -1;
}

static int parse_arguments(int argc, char *argv[], int *children,
                           int *use_mutex) {
  int process_count_seen = 0;
  int mutex_seen = 0;

  if (argc != 3 && argc != 4)
    return 1;

  for (int index = 1; index < argc; ++index) {
    if (strcmp(argv[index], "--mutexl") == 0) {
      if (mutex_seen)
        return 1;
      mutex_seen = 1;
      continue;
    }

    if (strcmp(argv[index], "-p") == 0) {
      char *end;
      long value;

      if (process_count_seen || index + 1 >= argc)
        return 1;

      errno = 0;
      end = NULL;
      value = strtol(argv[++index], &end, 10);
      if (errno == ERANGE || end == argv[index] || *end != '\0' || value < 1 ||
          value > MAX_PROCESS_ID)
        return 1;

      *children = (int)value;
      process_count_seen = 1;
      continue;
    }

    return 1;
  }

  if (!process_count_seen || (argc == 4 && !mutex_seen) ||
      (argc == 3 && mutex_seen))
    return 1;

  *use_mutex = mutex_seen;
  return 0;
}

int main(int argc, char *argv[]) {
  int children_count;
  int use_mutex;
  if (parse_arguments(argc, argv, &children_count, &use_mutex)) {
    fprintf(stderr, "usage: %s -p <children> [--mutexl]\n", argv[0]);
    return 1;
  }

  int count = children_count + 1;
  int events_fd = open(events_log, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND,
                       0644);
  int pipes_fd = open(pipes_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (events_fd == -1 || pipes_fd == -1)
    return 1;

  element pipes[count][count];
  for (int from = 0; from < count; ++from) {
    for (int to = 0; to < count; ++to) {
      pipes[from][to].read_fd = -1;
      pipes[from][to].write_fd = -1;
      if (from == to)
        continue;

      int descriptors[2];
      if (pipe(descriptors) == -1 || set_nonblocking(descriptors[0]) ||
          set_nonblocking(descriptors[1]))
        return 1;
      pipes[from][to].read_fd = descriptors[0];
      pipes[from][to].write_fd = descriptors[1];

      char description[128];
      int length = snprintf(description, sizeof(description),
                            "P%d -> P%d: read_fd=%d write_fd=%d\n", from, to,
                            descriptors[0], descriptors[1]);
      if (length < 0 || length >= (int)sizeof(description) ||
          write_all(pipes_fd, description, (size_t)length))
        return 1;
    }
  }
  if (close(pipes_fd) == -1)
    return 1;

  pid_t children[children_count];
  for (int id = 1; id < count; ++id) {
    pid_t pid = fork();
    if (pid == -1)
      return 1;
    if (pid == 0) {
      if (configure_fds(id, count, pipes))
        return 1;
      return child_main((local_id)id, count, pipes, events_fd, use_mutex);
    }
    children[id - 1] = pid;
  }

  if (configure_fds(PARENT_ID, count, pipes))
    return 1;

  IpcContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.self_id = PARENT_ID;
  ctx.process_count = count;
  ctx.last_sender = -1;
  ctx.pipes = &pipes[0][0];

  if (wait_for_children(&ctx, STARTED) || wait_for_children(&ctx, DONE))
    return 1;

  for (int index = 0; index < children_count; ++index) {
    int status;
    if (waitpid(children[index], &status, 0) == -1 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0)
      return 1;
  }

  return close(events_fd) == -1;
}
