// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0

#include "defold_embed_bus.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct MessageNode {
    char* json;
    MessageNode* next;
};

struct MessageQueue {
    MessageNode* head;
    MessageNode* tail;
};

static pthread_mutex_t g_Mutex = PTHREAD_MUTEX_INITIALIZER;
static MessageQueue g_Inbound = { 0, 0 };
static MessageQueue g_Outbound = { 0, 0 };
static DefoldEmbedMessageCallback g_Callback = 0;
static void* g_CallbackUserData = 0;

static char* DupJson(const char* json)
{
    if (!json)
        return 0;
    size_t n = strlen(json);
    char* copy = (char*)malloc(n + 1);
    if (!copy)
        return 0;
    memcpy(copy, json, n + 1);
    return copy;
}

static void Push(MessageQueue* q, char* json)
{
    if (!json)
        return;
    MessageNode* node = (MessageNode*)calloc(1, sizeof(MessageNode));
    if (!node)
    {
        free(json);
        return;
    }
    node->json = json;
    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;
    q->tail = node;
}

static char* Pop(MessageQueue* q)
{
    MessageNode* node = q->head;
    if (!node)
        return 0;
    q->head = node->next;
    if (!q->head)
        q->tail = 0;
    char* json = node->json;
    free(node);
    return json;
}

static void Clear(MessageQueue* q)
{
    while (char* json = Pop(q))
        free(json);
}

} // namespace

extern "C" void DefoldEmbedBus_EnqueueInbound(const char* json_utf8)
{
    char* copy = DupJson(json_utf8);
    if (!copy)
        return;
    pthread_mutex_lock(&g_Mutex);
    Push(&g_Inbound, copy);
    pthread_mutex_unlock(&g_Mutex);
}

extern "C" int DefoldEmbedBus_PollInbound(char** out)
{
    if (!out)
        return 0;
    *out = 0;
    pthread_mutex_lock(&g_Mutex);
    *out = Pop(&g_Inbound);
    pthread_mutex_unlock(&g_Mutex);
    return *out != 0;
}

extern "C" void DefoldEmbedBus_EnqueueOutbound(const char* json_utf8)
{
    char* copy = DupJson(json_utf8);
    if (!copy)
        return;
    pthread_mutex_lock(&g_Mutex);
    Push(&g_Outbound, copy);
    pthread_mutex_unlock(&g_Mutex);
}

extern "C" void DefoldEmbedBus_SetCallback(DefoldEmbedMessageCallback cb, void* user_data)
{
    pthread_mutex_lock(&g_Mutex);
    g_Callback = cb;
    g_CallbackUserData = user_data;
    pthread_mutex_unlock(&g_Mutex);
}

extern "C" void DefoldEmbedBus_FlushOutbound(void)
{
    DefoldEmbedMessageCallback cb = 0;
    void* user = 0;
    MessageQueue local = { 0, 0 };

    pthread_mutex_lock(&g_Mutex);
    cb = g_Callback;
    user = g_CallbackUserData;
    local = g_Outbound;
    g_Outbound.head = 0;
    g_Outbound.tail = 0;
    pthread_mutex_unlock(&g_Mutex);

    while (char* json = Pop(&local))
    {
        if (cb)
            cb(user, json);
        free(json);
    }
}

extern "C" void DefoldEmbedBus_Reset(void)
{
    pthread_mutex_lock(&g_Mutex);
    Clear(&g_Inbound);
    Clear(&g_Outbound);
    g_Callback = 0;
    g_CallbackUserData = 0;
    pthread_mutex_unlock(&g_Mutex);
}
