#ifndef UTIL_H
#define UTIL_H
#include "banking.h"
#include "common.h"
#include "ipc.h"
#include "ipc_local.h"
#include "pa2345.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);

  if (flags == -1) {
    return 1;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return 1;
  }

  return 0;
}
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

static int update_history(BalanceHistory *history, timestamp_t time,
                          balance_t new_balance) {
  if (history == NULL || time < 0 || time > MAX_T) {
    return 1;
  }

  /*
   * Balance before the new event.
   */
  balance_t previous_balance = new_balance;

  if (history->s_history_len > 0) {
    previous_balance = history->s_history[history->s_history_len - 1].s_balance;
  }

  /*
   * Fill all missing timestamps with the previous balance.
   *
   * Example:
   * history_len = 1, time = 3
   *
   * fill:
   * t=1 -> old balance
   * t=2 -> old balance
   */
  for (timestamp_t t = history->s_history_len; t < time; ++t) {
    history->s_history[t].s_time = t;
    history->s_history[t].s_balance = previous_balance;
    history->s_history[t].s_balance_pending_in = 0;
  }

  /*
   * At the event timestamp store the new balance.
   *
   * If another event happens at the same physical timestamp,
   * this simply overwrites that timestamp with the newest balance.
   */
  history->s_history[time].s_time = time;
  history->s_history[time].s_balance = new_balance;
  history->s_history[time].s_balance_pending_in = 0;

  if (history->s_history_len <= time) {
    history->s_history_len = (uint8_t)(time + 1);
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

static int handle_transfer(IpcContext *ctx, local_id idx, balance_t *balance,
                           BalanceHistory *history, int events_fd,
                           Message *msg) {
  if (msg->s_header.s_payload_len != sizeof(TransferOrder)) {
    return 1;
  }

  TransferOrder order;
  memcpy(&order, msg->s_payload, sizeof(order));

  local_id src = order.s_src;
  local_id dst = order.s_dst;
  balance_t amount = order.s_amount;

  if (idx == src) {
    /*
     * Source:
     * subtract money and forward the same TRANSFER to destination.
     */

    timestamp_t time = get_physical_time();
    *balance -= amount;

    if (update_history(history, time, *balance) != 0) {
      return 1;
    }

    char log_buffer[MAX_PAYLOAD_LEN];

    int len = snprintf(log_buffer, sizeof(log_buffer), log_transfer_out_fmt,
                       time, idx, amount, dst);

    if (len < 0 || len >= (int)sizeof(log_buffer)) {
      return 1;
    }

    if (log_event(events_fd, log_buffer) != 0) {
      return 1;
    }

    if (send(ctx, dst, msg) != 0) {
      return 1;
    }

    return 0;
  }

  if (idx == dst) {
    /*
     * Destination:
     * add money and send ACK to parent.
     */

    timestamp_t time = get_physical_time();
    *balance += amount;

    if (update_history(history, time, *balance) != 0) {
      return 1;
    }

    char log_buffer[MAX_PAYLOAD_LEN];

    int len = snprintf(log_buffer, sizeof(log_buffer), log_transfer_in_fmt,
                       time, idx, amount, src);

    if (len < 0 || len >= (int)sizeof(log_buffer)) {
      return 1;
    }

    if (log_event(events_fd, log_buffer) != 0) {
      return 1;
    }

    Message ack;
    memset(&ack, 0, sizeof(ack));

    ack.s_header.s_magic = MESSAGE_MAGIC;
    ack.s_header.s_payload_len = 0;
    ack.s_header.s_type = ACK;
    ack.s_header.s_local_time = get_physical_time();

    if (send(ctx, PARENT_ID, &ack) != 0) {
      return 1;
    }

    return 0;
  }

  /*
   * TRANSFER came to an unrelated process.
   */
  return 1;
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
 * Parent receives one message of the requested type
 * from every child.
 */
static int parent_receive_all(IpcContext *ctx, MessageType expected_type) {
  for (int i = 1; i < ctx->process_count; i++) {

    Message message;

    if (receive_any(ctx, &message) != 0) {

      perror("receive parent");
      return 1;
    }

    if (message.s_header.s_type != expected_type) {
      return 1;
    }
  }

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
static int child_work(int idx, int N, element pipes[N][N], int events_fd,
                      balance_t balance) {

  IpcContext ctx;

  ctx.self_id = (local_id)idx;
  ctx.process_count = N;
  ctx.pipes = &pipes[0][0];

  BalanceHistory history;
  memset(&history, 0, sizeof(history));
  history.s_id = (local_id)idx;

  history.s_history[0].s_time = 0;
  history.s_history[0].s_balance = balance;
  history.s_history[0].s_balance_pending_in = 0;
  history.s_history_len = 1;

  /*
   * ============================================================
   * STARTED
   * ============================================================
   */

  Message message;
  memset(&message, 0, sizeof(message));

  timestamp_t time = get_physical_time();

  for (timestamp_t t = 0; t <= time; t++) {
    history.s_history[t].s_balance = balance;
    history.s_history[t].s_time = t;
    history.s_history[t].s_balance_pending_in = 0;
  }

  int len = snprintf(message.s_payload, MAX_PAYLOAD_LEN, log_started_fmt, time,
                     idx, (int)getpid(), (int)getppid(), balance);
  message.s_header.s_local_time = time;

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

  for (int from = 1; from < N; ++from) {
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

  time = get_physical_time();
  len = snprintf(log_buffer, sizeof(log_buffer), log_received_all_started_fmt,
                 time, idx);
  message.s_header.s_local_time = time;

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
   */

  for (;;) {
    Message msg;

    if (receive_any(&ctx, &msg) != 0) {
      perror("receive_any");
      return 1;
    }

    if (msg.s_header.s_type == STOP) {
      break;
    }

    if (msg.s_header.s_type == TRANSFER) {
      if (handle_transfer(&ctx, (local_id)idx, &balance, &history, events_fd,
                          &msg) != 0) {
        return 1;
      }
    }
  }

  /*
   * ============================================================
   * DONE
   * ============================================================
   */

  memset(&message, 0, sizeof(message));

  time = get_physical_time();
  len = snprintf(message.s_payload, MAX_PAYLOAD_LEN, log_done_fmt, time, idx,
                 balance);
  message.s_header.s_local_time = time;

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

  if (send_multicast(&ctx, &message) != 0) {
    perror("send_multicast DONE");
    return 1;
  }

  /*
   * ============================================================
   * RECEIVE DONE FROM ALL OTHER CHILDREN
   * ============================================================
   */

  int done_count = 0;

  while (done_count < N - 2) {
    Message received;

    if (receive_any(&ctx, &received) != 0) {
      perror("receive_any DONE");
      return 1;
    }

    if (received.s_header.s_type == DONE) {
      done_count++;
      continue;
    }

    if (received.s_header.s_type == TRANSFER) {
      if (handle_transfer(&ctx, (local_id)idx, &balance, &history, events_fd,
                          &received) != 0) {
        return 1;
      }

      continue;
    }
  }

  /*
   * Log:
   *
   * Process X received all DONE messages
   */

  time = get_physical_time();

  len = snprintf(log_buffer, sizeof(log_buffer), log_received_all_done_fmt,
                 time, idx);

  if (len < 0 || len >= (int)sizeof(log_buffer)) {
    return 1;
  }

  if (log_event(events_fd, log_buffer) != 0) {
    return 1;
  }

  /*
   * Extend history up to the current physical time.
   */
  timestamp_t final_time = get_physical_time();

  if (update_history(&history, final_time, balance) != 0) {
    return 1;
  }

  /*
   * Send only the used part of BalanceHistory.
   */
  size_t history_size = offsetof(BalanceHistory, s_history) +
                        history.s_history_len * sizeof(BalanceState);

  if (history_size > MAX_PAYLOAD_LEN) {
    return 1;
  }

  Message history_msg;
  memset(&history_msg, 0, sizeof(history_msg));

  memcpy(history_msg.s_payload, &history, history_size);

  history_msg.s_header.s_magic = MESSAGE_MAGIC;
  history_msg.s_header.s_payload_len = (uint16_t)history_size;
  history_msg.s_header.s_type = BALANCE_HISTORY;
  history_msg.s_header.s_local_time = final_time;

  if (send(&ctx, PARENT_ID, &history_msg) != 0) {
    return 1;
  }

  return 0;
}
#endif // !UTIL_H
