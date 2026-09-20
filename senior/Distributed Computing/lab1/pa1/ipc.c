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
static int read_exact(int fd, void *buf, size_t len) {
  size_t received = 0;
  char *ptr = buf;

  while (received < len) {
    ssize_t rc = read(fd, ptr + received, len - received);

    if (rc > 0) {
      received += (size_t)rc;
      continue;
    }

    if (rc == 0) {
      /*
       * All write ends of this pipe are closed.
       */
      errno = EPIPE;
      return 1;
    }

    if (errno == EINTR)
      continue;

    return 1;
  }

  return 0;
}

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

  size_t message_len = sizeof(MessageHeader) + msg->s_header.s_payload_len;

  /*
   * Message is laid out in memory as follows:
   *
   * +----------------+
   * | MessageHeader  |
   * +----------------+
   * | payload        |
   * +----------------+
   *
   * This allows the required part of Message to be sent with one write().
   *
   * Do not send the entire MAX_PAYLOAD_LEN buffer.
   */
  for (;;) {
    ssize_t rc = write(channel->write_fd, msg, message_len);

    if (rc == -1 && errno == EINTR)
      continue;

    if (rc == -1)
      return 1;

    if ((size_t)rc != message_len) {
      errno = EIO;
      return 1;
    }

    return 0;
  }
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

  /*
   * Read the fixed-size header first.
   */
  if (read_exact(channel->read_fd, &msg->s_header, sizeof(MessageHeader)) !=
      0) {
    return 1;
  }

  /*
   * The payload size is now known.
   */
  if (msg->s_header.s_magic != MESSAGE_MAGIC) {
    errno = EINVAL;
    return 1;
  }

  if (msg->s_header.s_payload_len > MAX_PAYLOAD_LEN) {
    errno = EMSGSIZE;
    return 1;
  }

  if (msg->s_header.s_payload_len == 0)
    return 0;

  if (read_exact(channel->read_fd, msg->s_payload,
                 msg->s_header.s_payload_len) != 0) {
    return 1;
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
   * Save the current fd flags.
   */
  int old_flags = fcntl(fd, F_GETFL);

  if (old_flags == -1)
    return -1;

  int changed = (old_flags & O_NONBLOCK) == 0;

  if (changed) {
    if (fcntl(fd, F_SETFL, old_flags | O_NONBLOCK) == -1) {
      return -1;
    }
  }

  int result = -1;
  int saved_errno = 0;

  /*
   * Try to read the header first.
   *
   * If the pipe is empty, a non-blocking read returns EAGAIN.
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
      saved_errno = EPIPE;
      result = -1;
      goto restore_flags;
    }

    if (errno == EINTR)
      continue;

    if (errno == EAGAIN || errno == EWOULDBLOCK) {

      if (received == 0) {
        /*
         * No message is currently available from this process.
         */
        result = 1;
        goto restore_flags;
      }

      /*
       * Message reading has already started.
       * Wait for the remaining bytes.
       */
      continue;
    }

    saved_errno = errno;
    result = -1;
    goto restore_flags;
  }

  if (msg->s_header.s_magic != MESSAGE_MAGIC) {
    saved_errno = EINVAL;
    result = -1;
    goto restore_flags;
  }

  if (msg->s_header.s_payload_len > MAX_PAYLOAD_LEN) {
    saved_errno = EMSGSIZE;
    result = -1;
    goto restore_flags;
  }

  received = 0;

  while (received < msg->s_header.s_payload_len) {

    ssize_t rc = read(fd, msg->s_payload + received,
                      msg->s_header.s_payload_len - received);

    if (rc > 0) {
      received += (size_t)rc;
      continue;
    }

    if (rc == 0) {
      saved_errno = EPIPE;
      result = -1;
      goto restore_flags;
    }

    if (errno == EINTR)
      continue;

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      continue;
    }

    saved_errno = errno;
    result = -1;
    goto restore_flags;
  }

  result = 0;

restore_flags:

  if (changed) {
    if (fcntl(fd, F_SETFL, old_flags) == -1) {

      if (result >= 0) {
        saved_errno = errno;
        result = -1;
      }
    }
  }

  if (saved_errno != 0)
    errno = saved_errno;

  return result;
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
