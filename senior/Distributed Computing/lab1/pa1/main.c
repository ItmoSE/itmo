#include "common.h"
#include "ipc.h"
#include "ipc_local.h"
#include "pa1.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * Write exactly len bytes.
 */
static int write_all(int fd, const char *buf, size_t len) {
  size_t written = 0;

  while (written < len) {
    ssize_t rc = write(fd, buf + written, len - written);

    if (rc > 0) {
      written += (size_t)rc;
      continue;
    }

    if (rc == -1 && errno == EINTR) {
      continue;
    }

    return 1;
  }

  return 0;
}

/*
 * Every event must be written both to stdout and events.log.
 */
static int log_event(int events_fd, const char *text) {
  size_t len = strlen(text);

  if (write_all(STDOUT_FILENO, text, len) != 0) {
    return 1;
  }

  if (write_all(events_fd, text, len) != 0) {
    return 1;
  }

  return 0;
}

/*
 * Close descriptor and mark it as unavailable in this process.
 */
static int close_one(int *fd) {
  if (*fd < 0) {
    return 0;
  }

  if (close(*fd) == -1) {
    return 1;
  }

  *fd = -1;
  return 0;
}

/*
 * For process Pidx:
 *
 *   pipes[idx][j] -> keep WRITE
 *   pipes[i][idx] -> keep READ
 *
 * All other pipe ends are closed.
 *
 * This rule is used for BOTH children and parent.
 */
static int configure_fds(int idx, int N, element pipes[N][N]) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {

      if (i == j) {
        continue;
      }

      if (idx == i) {
        /*
         * Pidx -> Pj
         *
         * Current process is sender.
         * Keep write_fd, close read_fd.
         */
        if (close_one(&pipes[i][j].read_fd) != 0) {
          perror("close");
          return 1;
        }

      } else if (idx == j) {
        /*
         * Pi -> Pidx
         *
         * Current process is receiver.
         * Keep read_fd, close write_fd.
         */
        if (close_one(&pipes[i][j].write_fd) != 0) {
          perror("close");
          return 1;
        }

      } else {
        /*
         * Current process does not participate
         * in channel Pi -> Pj.
         */
        if (close_one(&pipes[i][j].read_fd) != 0) {
          perror("close");
          return 1;
        }

        if (close_one(&pipes[i][j].write_fd) != 0) {
          perror("close");
          return 1;
        }
      }
    }
  }

  return 0;
}

/*
 * Work performed by a child process.
 *
 * configure_fds() has already been called before entering here.
 */
static int child_work(int idx, int N, element pipes[N][N], int events_fd) {

  IpcContext ctx;

  ctx.self_id = (local_id)idx;
  ctx.process_count = N;
  ctx.pipes = &pipes[0][0];

  /*
   * ============================================================
   * STARTED
   * ============================================================
   */

  Message message;
  memset(&message, 0, sizeof(message));

  int len = snprintf(message.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, idx,
                     (int)getpid(), (int)getppid());

  if (len < 0 || len >= MAX_PAYLOAD_LEN) {
    return 1;
  }

  /*
   * The same string goes to stdout/events.log
   * and becomes STARTED payload.
   */
  if (log_event(events_fd, message.s_payload) != 0) {
    return 1;
  }

  message.s_header.s_magic = MESSAGE_MAGIC;
  message.s_header.s_payload_len = (uint16_t)len;
  message.s_header.s_type = STARTED;
  message.s_header.s_local_time = 0;

  /*
   * STARTED -> all other processes,
   * including parent.
   */
  if (send_multicast(&ctx, &message) != 0) {
    perror("send_multicast STARTED");
    return 1;
  }

  /*
   * ============================================================
   * RECEIVE STARTED FROM ALL OTHER CHILDREN
   * ============================================================
   *
   * P0 does not send STARTED.
   */

  for (int from = 1; from < N; from++) {

    if (from == idx) {
      continue;
    }

    Message received;

    if (receive(&ctx, (local_id)from, &received) != 0) {

      perror("receive STARTED");
      return 1;
    }

    if (received.s_header.s_type != STARTED) {
      return 1;
    }
  }

  /*
   * Log:
   *
   * Process X received all STARTED messages
   */

  char log_buffer[MAX_PAYLOAD_LEN];

  len = snprintf(log_buffer, sizeof(log_buffer), log_received_all_started_fmt,
                 idx);

  if (len < 0 || len >= (int)sizeof(log_buffer)) {
    return 1;
  }

  if (log_event(events_fd, log_buffer) != 0) {
    return 1;
  }

  /*
   * ============================================================
   * USEFUL WORK
   * ============================================================
   *
   * PA1 has no useful work.
   */

  /*
   * ============================================================
   * DONE
   * ============================================================
   */

  memset(&message, 0, sizeof(message));

  len = snprintf(message.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, idx);

  if (len < 0 || len >= MAX_PAYLOAD_LEN) {
    return 1;
  }

  /*
   * The exact same DONE string is logged
   * and used as message payload.
   */
  if (log_event(events_fd, message.s_payload) != 0) {
    return 1;
  }

  message.s_header.s_magic = MESSAGE_MAGIC;
  message.s_header.s_payload_len = (uint16_t)len;
  message.s_header.s_type = DONE;
  message.s_header.s_local_time = 0;

  if (send_multicast(&ctx, &message) != 0) {
    perror("send_multicast DONE");
    return 1;
  }

  /*
   * ============================================================
   * RECEIVE DONE FROM ALL OTHER CHILDREN
   * ============================================================
   */

  for (int from = 1; from < N; from++) {

    if (from == idx) {
      continue;
    }

    Message received;

    if (receive(&ctx, (local_id)from, &received) != 0) {

      perror("receive DONE");
      return 1;
    }

    if (received.s_header.s_type != DONE) {
      return 1;
    }
  }

  /*
   * Log:
   *
   * Process X received all DONE messages
   */

  len =
      snprintf(log_buffer, sizeof(log_buffer), log_received_all_done_fmt, idx);

  if (len < 0 || len >= (int)sizeof(log_buffer)) {
    return 1;
  }

  if (log_event(events_fd, log_buffer) != 0) {
    return 1;
  }

  return 0;
}

/*
 * Parent receives one message of the requested type
 * from every child.
 */
static int parent_receive_all(IpcContext *ctx, MessageType expected_type) {
  for (int from = 1; from < ctx->process_count; from++) {

    Message message;

    if (receive(ctx, (local_id)from, &message) != 0) {

      perror("receive parent");
      return 1;
    }

    if (message.s_header.s_type != expected_type) {
      return 1;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  char *end = NULL;

  /*
   * ============================================================
   * INPUT
   * ============================================================
   */

  if (argc != 3 || strcmp(argv[1], "-p") != 0) {
    fprintf(stderr, "usage: %s -p <number>\n", argv[0]);
    return 1;
  }

  errno = 0;

  long parsed = strtol(argv[2], &end, 10);

  if (errno == ERANGE || end == argv[2] || *end != '\0' || parsed < 1 ||
      parsed > 10) {

    fprintf(stderr, "usage: %s -p <number from 1 to 10>\n", argv[0]);
    return 1;
  }

  /*
   * -p X means X CHILD processes.
   *
   * Total process count:
   *
   * P0 + X children.
   */
  int X = (int)parsed;
  int N = X + 1;

  /*
   * ============================================================
   * OPEN LOG FILES BEFORE FORK
   * ============================================================
   */

  int events_fd =
      open(events_log, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);

  if (events_fd == -1) {
    perror("open events.log");
    return 1;
  }

  int pipes_fd = open(pipes_log, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  if (pipes_fd == -1) {
    perror("open pipes.log");
    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * CREATE ALL N*(N-1) PIPES
   * ============================================================
   */

  element pipes[N][N];

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {

      if (i == j) {
        pipes[i][j].read_fd = -1;
        pipes[i][j].write_fd = -1;
        continue;
      }

      int fd[2];

      if (pipe(fd) == -1) {
        perror("pipe");
        close(events_fd);
        close(pipes_fd);
        return 1;
      }

      pipes[i][j].read_fd = fd[0];
      pipes[i][j].write_fd = fd[1];

      /*
       * Save topology into pipes.log.
       */
      char buffer[128];

      int len =
          snprintf(buffer, sizeof(buffer),
                   "P%d -> P%d: read_fd=%d write_fd=%d\n", i, j, fd[0], fd[1]);

      if (len < 0 || len >= (int)sizeof(buffer)) {
        close(events_fd);
        close(pipes_fd);
        return 1;
      }

      if (write_all(pipes_fd, buffer, (size_t)len) != 0) {

        perror("write pipes.log");
        close(events_fd);
        close(pipes_fd);
        return 1;
      }
    }
  }

  /*
   * pipes.log itself does not need to be inherited
   * by children.
   */
  if (close(pipes_fd) == -1) {
    perror("close pipes.log");
    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * CREATE X CHILDREN
   * ============================================================
   */

  pid_t children[X];

  for (int idx = 1; idx < N; idx++) {

    pid_t pid = fork();

    if (pid == -1) {
      perror("fork");
      close(events_fd);
      return 1;
    }

    if (pid == 0) {
      /*
       * CHILD
       *
       * idx becomes this child's local_id.
       */

      if (configure_fds(idx, N, pipes) != 0) {
        return 1;
      }

      /*
       * return is important:
       *
       * the child must NOT return to the fork loop
       * and create more children.
       */
      return child_work(idx, N, pipes, events_fd);
    }

    /*
     * Only parent gets here.
     */
    children[idx - 1] = pid;
  }

  /*
   * ============================================================
   * PARENT P0
   * ============================================================
   *
   * All children have already been created.
   *
   * Now parent can close all duplicate / unnecessary
   * pipe ends.
   */

  if (configure_fds(PARENT_ID, N, pipes) != 0) {
    close(events_fd);
    return 1;
  }

  /*
   * Parent keeps a complete topology:
   *
   * P0 -> Px : write ends
   * Px -> P0 : read ends
   *
   * Even though PA1 says P0 does not actually send
   * STARTED/DONE messages.
   */

  IpcContext parent_ctx;

  parent_ctx.self_id = PARENT_ID;
  parent_ctx.process_count = N;
  parent_ctx.pipes = &pipes[0][0];

  /*
   * ============================================================
   * RECEIVE ALL STARTED
   * ============================================================
   */

  if (parent_receive_all(&parent_ctx, STARTED) != 0) {

    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * RECEIVE ALL DONE
   * ============================================================
   */

  if (parent_receive_all(&parent_ctx, DONE) != 0) {

    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * WAIT FOR CHILD PROCESSES
   * ============================================================
   */

  for (int i = 0; i < X; i++) {

    int status;

    if (waitpid(children[i], &status, 0) == -1) {

      perror("waitpid");
      close(events_fd);
      return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {

      close(events_fd);
      return 1;
    }
  }

  /*
   * ============================================================
   * CLEANUP
   * ============================================================
   */

  if (close(events_fd) == -1) {
    perror("close events.log");
    return 1;
  }

  return 0;
}
