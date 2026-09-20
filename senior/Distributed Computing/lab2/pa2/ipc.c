#include "ipc.h"
#include "ipc_local.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
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

/*
 * Try to receive a message without blocking.
 *
 * return:
 *     0  message received
 *     1  no message available
 *    -1  error
 */
static int try_receive(IpcContext *ctx, local_id from, Message *msg) {
  element *channel = get_channel(ctx, from, ctx->self_id);

  int fd = channel->read_fd;

  if (fd < 0) {
    errno = EBADF;
    return -1;
  }

  /*
   * Return values:
   *
   *  0 - complete message received
   *  1 - no message available right now
   *  2 - pipe was closed cleanly
   * -1 - actual error
   */

  /*
   * Read MessageHeader.
   */
  size_t received = 0;
  char *header_ptr = (char *)&msg->s_header;

  while (received < sizeof(MessageHeader)) {
    ssize_t rc =
        read(fd, header_ptr + received, sizeof(MessageHeader) - received);

    if (rc > 0) {
      received += (size_t)rc;
      continue;
    }

    if (rc == 0) {
      /*
       * EOF before we started reading a new message:
       * writer closed its end normally.
       */
      if (received == 0) {
        return 2;
      }

      /*
       * EOF in the middle of a message.
       */
      errno = EPIPE;
      return -1;
    }

    /*
     * rc == -1
     */

    if (errno == EINTR) {
      continue;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      /*
       * Nothing from this process right now.
       */
      if (received == 0) {
        return 1;
      }

      /*
       * We have already consumed part of the header.
       * We cannot return now, otherwise the next call
       * would interpret the remaining bytes as a new header.
       */
      continue;
    }

    return -1;
  }

  /*
   * Validate header.
   */
  if (msg->s_header.s_magic != MESSAGE_MAGIC) {
    errno = EINVAL;
    return -1;
  }

  if (msg->s_header.s_payload_len > MAX_PAYLOAD_LEN) {
    errno = EMSGSIZE;
    return -1;
  }

  /*
   * Empty payload means the message is already complete.
   */
  if (msg->s_header.s_payload_len == 0) {
    return 0;
  }

  /*
   * Read payload.
   */
  received = 0;

  while (received < msg->s_header.s_payload_len) {
    ssize_t rc = read(fd, msg->s_payload + received,
                      msg->s_header.s_payload_len - received);

    if (rc > 0) {
      received += (size_t)rc;
      continue;
    }

    if (rc == 0) {
      /*
       * Header was received, but writer closed the pipe
       * before the complete payload arrived.
       */
      errno = EPIPE;
      return -1;
    }

    /*
     * rc == -1
     */

    if (errno == EINTR) {
      continue;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      /*
       * Part of this message has already been consumed,
       * so keep waiting for the rest.
       */
      continue;
    }

    return -1;
  }

  return 0;
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
    int rc = try_receive(ctx, from, msg);

    if (rc == 0) {
      return 0;
    }

    if (rc == 1) {
      /*
       * Nothing available from this process right now.
       * Keep trying.
       */
      continue;
    }

    if (rc == 2) {
      /*
       * Sender closed the pipe before sending
       * the message we are waiting for.
       */
      errno = EPIPE;
      return 1;
    }

    /*
     * rc == -1
     */
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
    for (int from = 0; from < ctx->process_count; ++from) {

      if (from == ctx->self_id)
        continue;

      element *channel = get_channel(ctx, (local_id)from, ctx->self_id);

      /*
       * The parent, for example, may not have some read descriptors.
       */
      if (channel->read_fd < 0)
        continue;

      int rc = try_receive(ctx, (local_id)from, msg);

      if (rc == 0)
        return 0;

      if (rc < 0)
        return 1;

      /*
       * rc == 1:
       * No message is currently available from this process.
       * Try the next one.
       */
    }
  }
}
