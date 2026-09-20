#include "banking.h"
#include "common.h"
#include "ipc_local.h"
#include "util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>

static int parent_collect_results(IpcContext *ctx, AllHistory *all_history) {
  if (ctx == NULL || all_history == NULL) {
    return 1;
  }

  memset(all_history, 0, sizeof(*all_history));

  int child_count = ctx->process_count - 1;

  int done_count = 0;
  int history_count = 0;

  while (done_count < child_count || history_count < child_count) {
    Message msg;

    if (receive_any(ctx, &msg) != 0) {
      perror("receive_any parent");
      return 1;
    }

    if (msg.s_header.s_type == DONE) {
      done_count++;
      continue;
    }

    if (msg.s_header.s_type == BALANCE_HISTORY) {
      /*
       * At minimum the message must contain:
       *
       * s_id
       * s_history_len
       */
      size_t history_header_size = offsetof(BalanceHistory, s_history);

      if (msg.s_header.s_payload_len < history_header_size) {
        return 1;
      }

      BalanceHistory history;
      memset(&history, 0, sizeof(history));

      memcpy(&history, msg.s_payload, msg.s_header.s_payload_len);

      /*
       * Child ids are 1 .. N-1.
       */
      if (history.s_id == PARENT_ID || history.s_id >= ctx->process_count) {
        return 1;
      }

      /*
       * Check that payload contains exactly as many
       * BalanceState entries as s_history_len says.
       */
      size_t expected_size = offsetof(BalanceHistory, s_history) +
                             history.s_history_len * sizeof(BalanceState);

      if (msg.s_header.s_payload_len != expected_size) {
        return 1;
      }

      /*
       * Store histories consecutively:
       *
       * P1 -> s_history[0]
       * P2 -> s_history[1]
       * ...
       */
      all_history->s_history[history.s_id - 1] = history;

      history_count++;
      continue;
    }

    /*
     * Parent should not receive anything else here.
     */
    return 1;
  }

  all_history->s_history_len = (uint8_t)child_count;

  return 0;
}

void transfer(void *parent_data, local_id src, local_id dst, balance_t amount) {
  IpcContext *ctx = parent_data;

  Message msg;
  memset(&msg, 0, sizeof(msg));

  TransferOrder order;
  order.s_src = src;
  order.s_dst = dst;
  order.s_amount = amount;

  /*
   * Put TransferOrder into message payload.
   */
  memcpy(msg.s_payload, &order, sizeof(order));

  msg.s_header.s_magic = MESSAGE_MAGIC;
  msg.s_header.s_payload_len = sizeof(order);
  msg.s_header.s_type = TRANSFER;
  msg.s_header.s_local_time = get_physical_time();

  /*
   * Parent sends TRANSFER to source process.
   */
  if (send(ctx, src, &msg) != 0) {
    return;
  }

  /*
   * Wait until destination confirms that
   * the transfer has been received.
   */
  for (;;) {
    Message ack;

    if (receive_any(ctx, &ack) != 0) {
      return;
    }

    if (ack.s_header.s_type == ACK) {
      break;
    }
  }
}

int main(int argc, char *argv[]) {

  char *end = NULL;
  long initial_budgets[10];

  /*
   * ============================================================
   * INPUT
   * ============================================================
   */

  if (argc < 3 || argc > 13 || strcmp(argv[1], "-p") != 0) {
    fprintf(stderr, "usage: %s -p <number> <budget> ...\n", argv[0]);
    return 1;
  }

  errno = 0;

  long parsed = strtol(argv[2], &end, 10);

  if (errno == ERANGE || end == argv[2] || *end != '\0' || parsed < 1 ||
      parsed > 10) {

    fprintf(stderr, "usage: %s -p <number from 1 to 10>\n", argv[0]);
    return 1;
  }
  if (argc != parsed + 3) {
    fprintf(stderr, "expected %ld balances\n", parsed);
    return 1;
  }

  for (int i = 0; i < parsed; i++) {
    errno = 0;
    end = NULL;

    long balance = strtol(argv[i + 3], &end, 10);

    if (errno == ERANGE || end == argv[i + 3] || *end != '\0') {
      fprintf(stderr, "invalid balance: %s\n", argv[i + 3]);
      return 1;
    }

    initial_budgets[i] = balance;
  }

  // for (int i = 0; i < parsed; i++) {
  //   printf("%ld\n", initial_budgets[i]);
  // }

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

      if (set_nonblocking(fd[0]) != 0 || set_nonblocking(fd[1]) != 0) {
        perror("fcntl O_NONBLOCK");
        close(fd[0]);
        close(fd[1]);
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
      return child_work(idx, N, pipes, events_fd, initial_budgets[idx - 1]);
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
   * BANK ROBBERY
   * ============================================================
   */

  bank_robbery(&parent_ctx, (local_id)X);

  /*
   * ============================================================
   * STOP
   * ============================================================
   *
   * No new transfers will be initiated after this point.
   */

  Message stop;
  memset(&stop, 0, sizeof(stop));

  stop.s_header.s_magic = MESSAGE_MAGIC;
  stop.s_header.s_payload_len = 0;
  stop.s_header.s_type = STOP;
  stop.s_header.s_local_time = get_physical_time();

  if (send_multicast(&parent_ctx, &stop) != 0) {
    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * RECEIVE DONE + BALANCE_HISTORY
   * ============================================================
   *
   * These messages may arrive interleaved, so they are collected
   * in one event loop.
   */

  AllHistory all_history;

  if (parent_collect_results(&parent_ctx, &all_history) != 0) {
    close(events_fd);
    return 1;
  }

  /*
   * ============================================================
   * PRINT BALANCE HISTORY
   * ============================================================
   */

  print_history(&all_history);

  /*
   * ============================================================
   * WAIT FOR ALL CHILDREN
   * ============================================================
   */

  for (int i = 0; i < X; ++i) {
    if (waitpid(children[i], NULL, 0) == -1) {
      perror("waitpid");
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
    return 1;
  }

  return 0;
}
