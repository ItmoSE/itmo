#ifndef UTIL_H
#define UTIL_H

#include "common.h"
#include "ipc_local.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

static int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  return flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1;
}

static int write_all(int fd, const char *buffer, size_t length) {
  size_t written = 0;
  while (written < length) {
    ssize_t result = write(fd, buffer + written, length - written);
    if (result > 0) {
      written += (size_t)result;
    } else if (result == -1 && errno == EINTR) {
      continue;
    } else {
      return 1;
    }
  }
  return 0;
}

static int log_event(int events_fd, const char *text) {
  size_t length = strlen(text);
  return write_all(STDOUT_FILENO, text, length) ||
         write_all(events_fd, text, length);
}

static int close_one(int *fd) {
  if (*fd < 0)
    return 0;
  if (close(*fd) == -1)
    return 1;
  *fd = -1;
  return 0;
}

static int configure_fds(int self_id, int count,
                         element pipes[count][count]) {
  for (int from = 0; from < count; ++from) {
    for (int to = 0; to < count; ++to) {
      if (from == to)
        continue;
      if (self_id == from) {
        if (close_one(&pipes[from][to].read_fd))
          return 1;
      } else if (self_id == to) {
        if (close_one(&pipes[from][to].write_fd))
          return 1;
      } else if (close_one(&pipes[from][to].read_fd) ||
                 close_one(&pipes[from][to].write_fd)) {
        return 1;
      }
    }
  }
  return 0;
}

#endif
