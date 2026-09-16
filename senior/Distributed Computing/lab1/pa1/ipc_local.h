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
   * Указатель на первый элемент матрицы:
   *
   * element pipes[N][N];
   *
   * pipes[from][to] хранится в памяти линейно.
   */
  element *pipes;
} IpcContext;

#endif
