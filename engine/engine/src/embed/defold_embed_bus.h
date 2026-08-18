// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
//
// Shared host↔engine JSON message bus for Defold_Embed* (Android + iOS).

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DefoldEmbedMessageCallback)(void* user_data, const char* json_utf8);

/** Enqueue host→engine JSON (copied). Delivered via host.poll() on the Lua side. */
void DefoldEmbedBus_EnqueueInbound(const char* json_utf8);

/** Pop next inbound JSON into *out (malloc'd); caller frees. Returns 0 if empty. */
int DefoldEmbedBus_PollInbound(char** out);

/** Enqueue engine→host JSON (copied). Flushed in DefoldEmbedBus_FlushOutbound. */
void DefoldEmbedBus_EnqueueOutbound(const char* json_utf8);

void DefoldEmbedBus_SetCallback(DefoldEmbedMessageCallback cb, void* user_data);

/** Invoke host callback for each queued outbound message (Update thread). */
void DefoldEmbedBus_FlushOutbound(void);

/** Clear queues and callback (Destroy). */
void DefoldEmbedBus_Reset(void);

#ifdef __cplusplus
}
#endif
