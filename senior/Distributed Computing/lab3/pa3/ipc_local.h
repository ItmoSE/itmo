/* ipc_local.h */

#ifndef IPC_LOCAL_H
#define IPC_LOCAL_H

#include "ipc.h"

#include <stddef.h>

typedef struct {
  int read_fd;
  int write_fd;
} element;

typedef struct {
  Message message;
  size_t received;
  size_t expected;
  int header_complete;
} IpcReadState;

typedef struct {
  local_id self_id;
  int process_count;

  /*
   * Pointer to the first element of the matrix:
   *
   * element pipes[N][N];
   *
   * pipes[from][to] is stored linearly in memory.
   */
  element *pipes;
  IpcReadState read_states[MAX_PROCESS_ID + 1];
} IpcContext;

#endif
