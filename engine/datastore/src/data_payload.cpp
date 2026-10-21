// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "data.h"

static const size_t      PAYLOAD_BLOCK_SIZE = 4096;
static const size_t      PAYLOAD_BLOCK_HEADER_SIZE = (sizeof(DSPayloadBlock) + 7) & ~(size_t)7;

static DSPayloadHeader** FreeLink(DSPayloadHeader* chunk)
{
    return (DSPayloadHeader**)(chunk + 1);
}

static void DeletePayloadBlock(DSPayloadBlock* block)
{
    if (block->m_Previous)
        block->m_Previous->m_Next = block->m_Next;
    else
        *block->m_Owner = block->m_Next;
    if (block->m_Next)
        block->m_Next->m_Previous = block->m_Previous;
    delete[] (uint8_t*)block;
}

static DSPayloadBlock* NewPayloadBlock(DSPayloadBlock** blocks, size_t size)
{
    if (*blocks && !(*blocks)->m_LiveSize)
        DeletePayloadBlock(*blocks);
    size_t          capacity = size > PAYLOAD_BLOCK_SIZE ? size : PAYLOAD_BLOCK_SIZE;
    DSPayloadBlock* block = (DSPayloadBlock*)new uint8_t[PAYLOAD_BLOCK_HEADER_SIZE + capacity];
    *block = { .m_Next = *blocks, .m_Owner = blocks, .m_Capacity = capacity };
    if (*blocks)
        (*blocks)->m_Previous = block;
    *blocks = block;
    return block;
}

void ReservePayloads(DSPayloadBlock** blocks, size_t size)
{
    size_t available = 0;
    for (DSPayloadBlock* block = *blocks; block; block = block->m_Next)
    {
        available += block->m_Capacity - block->m_LiveSize;
        if (available >= size)
            return;
    }
    NewPayloadBlock(blocks, size);
}

void* AllocatePayload(DSPayloadBlock** blocks, size_t size)
{
    size = (size + 7) & ~(size_t)7;
    size_t bytes = sizeof(DSPayloadHeader) + size;
    for (DSPayloadBlock* block = *blocks; block; block = block->m_Next)
    {
        // A freed chunk keeps its next link in the old payload, adding no live
        // per-value pointers beyond the allocation header.
        for (DSPayloadHeader** link = &block->m_Free; *link; link = FreeLink(*link))
        {
            DSPayloadHeader* chunk = *link;
            if (chunk->m_Size < size)
                continue;
            *link = *FreeLink(chunk);
            if (chunk->m_Size - size >= sizeof(DSPayloadHeader) + 8)
            {
                DSPayloadHeader* remainder = (DSPayloadHeader*)((uint8_t*)(chunk + 1) + size);
                *remainder = { .m_Block = block, .m_Size = chunk->m_Size - size - sizeof(DSPayloadHeader) };
                *FreeLink(remainder) = *link;
                *link = remainder;
                chunk->m_Size = size;
            }
            block->m_LiveSize += sizeof(DSPayloadHeader) + chunk->m_Size;
            return chunk + 1;
        }
        if (bytes <= block->m_Capacity - block->m_Size)
        {
            DSPayloadHeader* chunk = (DSPayloadHeader*)((uint8_t*)block + PAYLOAD_BLOCK_HEADER_SIZE + block->m_Size);
            *chunk = { .m_Block = block, .m_Size = size };
            block->m_Size += bytes;
            block->m_LiveSize += bytes;
            return chunk + 1;
        }
    }
    DSPayloadBlock*  block = NewPayloadBlock(blocks, bytes);
    DSPayloadHeader* chunk = (DSPayloadHeader*)((uint8_t*)block + PAYLOAD_BLOCK_HEADER_SIZE);
    *chunk = { .m_Block = block, .m_Size = size };
    block->m_Size = block->m_LiveSize = bytes;
    return chunk + 1;
}

void ReleasePayload(void* bytes)
{
    DSPayloadHeader* chunk = (DSPayloadHeader*)bytes - 1;
    DSPayloadBlock*  block = chunk->m_Block;
    block->m_LiveSize -= sizeof(DSPayloadHeader) + chunk->m_Size;
    if (block->m_LiveSize)
    {
        *FreeLink(chunk) = block->m_Free;
        block->m_Free = chunk;
    }
    else if (!block->m_Previous && block->m_Capacity <= PAYLOAD_BLOCK_SIZE)
    {
        block->m_Size = 0;
        block->m_Free = 0;
    }
    else
        DeletePayloadBlock(block);
}

void DeletePayloads(DSPayloadBlock** blocks)
{
    while (*blocks)
        DeletePayloadBlock(*blocks);
}
