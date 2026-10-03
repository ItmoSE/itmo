#include "ipc_local.h"
#include "pa2345.h"

#include <string.h>

static void empty_message(Message *message, MessageType type) {
  memset(message, 0, sizeof(*message));
  message->s_header.s_magic = MESSAGE_MAGIC;
  message->s_header.s_type = type;
}

static int reply_to(IpcContext *ctx, local_id destination) {
  Message reply;
  empty_message(&reply, CS_REPLY);
  return send(ctx, destination, &reply);
}

static int handle_message(IpcContext *ctx, const Message *message) {
  local_id sender = ctx->last_sender;

  switch (message->s_header.s_type) {
  case CS_REQUEST:
    ctx->request_active[(int)sender] = 1;
    ctx->request_time[(int)sender] = message->s_header.s_local_time;
    return reply_to(ctx, sender);
  case CS_REPLY:
    ctx->reply_received[(int)sender] = 1;
    return 0;
  case CS_RELEASE:
    ctx->request_active[(int)sender] = 0;
    return 0;
  case DONE:
    ctx->done_received[(int)sender] = 1;
    return 0;
  default:
    return 1;
  }
}

int mutex_handle_one(IpcContext *ctx, const Message *message) {
  return handle_message(ctx, message);
}

static int own_request_is_first(const IpcContext *ctx) {
  timestamp_t own_time = ctx->request_time[(int)ctx->self_id];
  for (int id = 1; id < ctx->process_count; ++id) {
    if (id == ctx->self_id || !ctx->request_active[id])
      continue;
    if (ctx->request_time[id] < own_time ||
        (ctx->request_time[id] == own_time && id < ctx->self_id))
      return 0;
  }
  return 1;
}

static int received_all_replies(const IpcContext *ctx) {
  for (int id = 1; id < ctx->process_count; ++id) {
    if (id != ctx->self_id && !ctx->reply_received[id])
      return 0;
  }
  return 1;
}

int request_cs(const void *self) {
  IpcContext *ctx = (IpcContext *)self;
  Message request;
  timestamp_t request_time;

  memset(ctx->reply_received, 0, sizeof(ctx->reply_received));
  empty_message(&request, CS_REQUEST);
  if (ipc_send_children(ctx, &request, &request_time))
    return 1;

  ctx->request_active[(int)ctx->self_id] = 1;
  ctx->request_time[(int)ctx->self_id] = request_time;

  while (!received_all_replies(ctx) || !own_request_is_first(ctx)) {
    Message incoming;
    if (receive_any(ctx, &incoming) || handle_message(ctx, &incoming))
      return 1;
  }
  return 0;
}

int release_cs(const void *self) {
  IpcContext *ctx = (IpcContext *)self;
  Message release;

  ctx->request_active[(int)ctx->self_id] = 0;
  empty_message(&release, CS_RELEASE);
  return ipc_send_children(ctx, &release, NULL);
}
