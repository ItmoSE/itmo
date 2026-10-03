#include "ipc.h"
#include "ipc_local.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

/*
 * The element pipes[N][N] matrix is stored contiguously in memory.
 *
 * pipes[from][to]
 *
 * is located at the following offset in the linear representation:
 *
 *     from * N + to
 */
static element *get_channel(IpcContext *ctx, local_id from, local_id to) {
  return &ctx->pipes[(size_t)from * (size_t)ctx->process_count + (size_t)to];
}

static int valid_context(const IpcContext *ctx) {
  if (ctx == NULL || ctx->pipes == NULL)
    return 0;

  if (ctx->process_count <= 0 || ctx->process_count > MAX_PROCESS_ID + 1)
    return 0;

  if (ctx->self_id < 0 || ctx->self_id >= ctx->process_count)
    return 0;

  return 1;
}

/*
 * Read exactly len bytes in blocking mode.
 *
 * read() may return fewer bytes than requested, so a single
 * read(fd, buf, len) call is not sufficient.
 */
// static int read_exact(int fd, void *buf, size_t len) {
//   size_t received = 0;
//   char *ptr = buf;
//
//   while (received < len) {
//     ssize_t rc = read(fd, ptr + received, len - received);
//
//     if (rc > 0) {
//       received += (size_t)rc;
//       continue;
//     }
//
//     if (rc == 0) {
//       /*
//        * All write ends of this pipe are closed.
//        */
//       errno = EPIPE;
//       return 1;
//     }
//
//     if (errno == EINTR)
//       continue;
//
//     return 1;
//   }
//
//   return 0;
// }

int send(void *self, local_id dst, const Message *msg) {
  IpcContext *ctx = self;

  if (!valid_context(ctx) || msg == NULL) {
    errno = EINVAL;
    return 1;
  }

  if (dst < 0 || dst >= ctx->process_count || dst == ctx->self_id) {
    errno = EINVAL;
    return 1;
  }

  if (msg->s_header.s_magic != MESSAGE_MAGIC) {
    errno = EINVAL;
    return 1;
  }

  if (msg->s_header.s_payload_len > MAX_PAYLOAD_LEN) {
    errno = EMSGSIZE;
    return 1;
  }

  element *channel = get_channel(ctx, ctx->self_id, dst);

  if (channel->write_fd < 0) {
    errno = EBADF;
    return 1;
  }

  int fd = channel->write_fd;

  size_t message_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;

  const char *ptr = (const char *)msg;
  size_t written = 0;

  /*
   * fd is already O_NONBLOCK.
   *
   * write() may:
   *   - write all bytes;
   *   - write only some bytes;
   *   - return EAGAIN if the pipe is currently full.
   */
  while (written < message_len) {
    ssize_t rc = write(fd, ptr + written, message_len - written);

    if (rc > 0) {
      written += (size_t)rc;
      continue;
    }

    if (rc == -1 && errno == EINTR) {
      continue;
    }

    if (rc == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      /*
       * Pipe is full right now.
       * Since poll/select are forbidden, retry.
       */
      continue;
    }

    /*
     * Any other result is an actual error.
     */
    if (rc == 0) {
      errno = EIO;
    }

    return 1;
  }

  return 0;
}
int send_multicast(void *self, const Message *msg) {
  IpcContext *ctx = self;

  if (!valid_context(ctx) || msg == NULL) {
    errno = EINVAL;
    return 1;
  }

  for (int dst = 0; dst < ctx->process_count; ++dst) {
    if (dst == ctx->self_id)
      continue;

    if (send(ctx, (local_id)dst, msg) != 0) {
      return 1;
    }
  }

  return 0;
}

typedef enum {
  RECEIVE_ATTEMPT_ERROR = -1,
  RECEIVE_ATTEMPT_COMPLETE = 0,
  RECEIVE_ATTEMPT_EMPTY = 1,
  RECEIVE_ATTEMPT_CLOSED = 2
} ReceiveAttemptResult;

/*
 * Try to receive a message without blocking.
 *
 * RECEIVE_ATTEMPT_COMPLETE - complete message received
 * RECEIVE_ATTEMPT_EMPTY    - no message available right now
 * RECEIVE_ATTEMPT_CLOSED   - pipe was closed before a new message
 * RECEIVE_ATTEMPT_ERROR    - actual error or truncated message
 */
static ReceiveAttemptResult try_receive(IpcContext *ctx, local_id from,
                                        Message *msg) {
  element *channel = get_channel(ctx, from, ctx->self_id);

  int fd = channel->read_fd;

  if (fd < 0) {
    errno = EBADF;
    return RECEIVE_ATTEMPT_ERROR;
  }

  IpcReadState *state = &ctx->read_states[(int)from];

  for (;;) {
    char *destination;
    size_t remaining;

    if (!state->header_complete) {
      destination = (char *)&state->message.s_header + state->received;
      remaining = sizeof(MessageHeader) - state->received;
    } else {
      destination = state->message.s_payload + state->received;
      remaining = state->expected - state->received;
    }

    ssize_t rc = read(fd, destination, remaining);

    if (rc > 0) {
      state->received += (size_t)rc;

      if (state->received <
          (state->header_complete ? state->expected : sizeof(MessageHeader))) {
        continue;
      }

      if (!state->header_complete) {
        if (state->message.s_header.s_magic != MESSAGE_MAGIC) {
          errno = EINVAL;
          return RECEIVE_ATTEMPT_ERROR;
        }

        if (state->message.s_header.s_payload_len > MAX_PAYLOAD_LEN) {
          errno = EMSGSIZE;
          return RECEIVE_ATTEMPT_ERROR;
        }

        state->header_complete = 1;
        state->received = 0;
        state->expected = state->message.s_header.s_payload_len;

        if (state->expected != 0) {
          continue;
        }
      }

      *msg = state->message;
      memset(state, 0, sizeof(*state));
      return RECEIVE_ATTEMPT_COMPLETE;
    }

    if (rc == 0) {
      if (state->received == 0 && !state->header_complete)
        return RECEIVE_ATTEMPT_CLOSED;

      errno = EPIPE;
      return RECEIVE_ATTEMPT_ERROR;
    }

    if (errno == EINTR)
      continue;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return RECEIVE_ATTEMPT_EMPTY;

    return RECEIVE_ATTEMPT_ERROR;
  }
}

int receive(void *self, local_id from, Message *msg) {
  IpcContext *ctx = self;

  if (!valid_context(ctx) || msg == NULL) {
    errno = EINVAL;
    return 1;
  }

  if (from < 0 || from >= ctx->process_count || from == ctx->self_id) {
    errno = EINVAL;
    return 1;
  }

  element *channel = get_channel(ctx, from, ctx->self_id);

  if (channel->read_fd < 0) {
    errno = EBADF;
    return 1;
  }

  for (;;) {
    ReceiveAttemptResult rc = try_receive(ctx, from, msg);

    if (rc == RECEIVE_ATTEMPT_COMPLETE) {
      return 0;
    }

    if (rc == RECEIVE_ATTEMPT_EMPTY) {
      /*
       * Nothing available from this process right now.
       * Keep trying.
       */
      continue;
    }

    if (rc == RECEIVE_ATTEMPT_CLOSED) {
      /*
       * Sender closed the pipe before sending
       * the message we are waiting for.
       */
      errno = EPIPE;
      return 1;
    }

    /* RECEIVE_ATTEMPT_ERROR. */
    return 1;
  }
}

int receive_any(void *self, Message *msg) {
  IpcContext *ctx = self;

  if (!valid_context(ctx) || msg == NULL) {
    errno = EINVAL;
    return 1;
  }

  for (;;) {
    int open_channels = 0;

    for (int from = 0; from < ctx->process_count; ++from) {

      if (from == ctx->self_id)
        continue;

      element *channel = get_channel(ctx, (local_id)from, ctx->self_id);

      /*
       * The parent, for example, may not have some read descriptors.
       */
      if (channel->read_fd < 0)
        continue;

      ++open_channels;

      ReceiveAttemptResult rc = try_receive(ctx, (local_id)from, msg);

      if (rc == RECEIVE_ATTEMPT_COMPLETE)
        return 0;

      if (rc == RECEIVE_ATTEMPT_ERROR)
        return 1;

      if (rc == RECEIVE_ATTEMPT_CLOSED) {
        close(channel->read_fd);
        channel->read_fd = -1;
        continue;
      }

      /*
       * RECEIVE_ATTEMPT_EMPTY:
       * No message is currently available from this process.
       * Try the next one.
       */
    }

    if (open_channels == 0) {
      errno = EPIPE;
      return 1;
    }
  }
}
