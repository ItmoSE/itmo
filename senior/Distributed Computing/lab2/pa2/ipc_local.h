/* ipc_local.h */

#ifndef IPC_LOCAL_H
#define IPC_LOCAL_H

#include "ipc.h"

typedef struct {
  int read_fd;
  int write_fd;
} element;

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
} IpcContext;

#endif
