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

static void LinkFreePage(HDSContext store, uint32_t index)
{
    DSRowPage& page = store->m_Pages[index];
    page.m_Prev = UINT32_MAX;
    page.m_Next = store->m_FreePage;
    if (page.m_Next != UINT32_MAX)
        store->m_Pages[page.m_Next].m_Prev = index;
    store->m_FreePage = index;
}

static void UnlinkFreePage(HDSContext store, uint32_t index)
{
    DSRowPage& page = store->m_Pages[index];
    if (page.m_Prev != UINT32_MAX)
        store->m_Pages[page.m_Prev].m_Next = page.m_Next;
    else
        store->m_FreePage = page.m_Next;
    if (page.m_Next != UINT32_MAX)
        store->m_Pages[page.m_Next].m_Prev = page.m_Prev;
}

static uint32_t NewPage(HDSContext store)
{
    uint32_t index = store->m_FreePageSlot;
    if (index == UINT32_MAX)
    {
        index = store->m_Pages.Size();
        Reserve(store->m_Pages, index + 1);
        store->m_Pages.SetSize(index + 1);
    }
    else
        store->m_FreePageSlot = store->m_Pages[index].m_Next;
    DSRowPage& page = store->m_Pages[index];
    page.m_Data = new uint8_t[store->m_PageSize];
    page.m_FreeChunk = 0;
    page.m_FreeCount = store->m_PageSize / store->m_RowChunkSize;
    for (uint32_t offset = 0; offset < store->m_PageSize; offset += store->m_RowChunkSize)
    {
        uint32_t next = offset + store->m_RowChunkSize;
        if (next == store->m_PageSize)
            next = UINT32_MAX;
        memcpy(page.m_Data + offset, &next, sizeof(next));
    }
    LinkFreePage(store, index);
    return index;
}

static DSRowChunk AllocateChunk(HDSContext store, size_t size)
{
    if (size > store->m_RowChunkSize)
        return DSRowChunk { .m_Data = new uint8_t[size], .m_Page = UINT32_MAX };
    uint32_t index = store->m_FreePage;
    if (index == UINT32_MAX)
        index = NewPage(store);
    else if (store->m_Pages[index].m_FreeCount == store->m_PageSize / store->m_RowChunkSize)
        --store->m_SparePageCount;
    DSRowPage& page = store->m_Pages[index];
    uint32_t   offset = page.m_FreeChunk;
    memcpy(&page.m_FreeChunk, page.m_Data + offset, sizeof(page.m_FreeChunk));
    if (!--page.m_FreeCount)
        UnlinkFreePage(store, index);
    return DSRowChunk { .m_Data = page.m_Data + offset, .m_Page = index, .m_Offset = offset };
}

static void ReleaseChunk(HDSContext store, const DSRowChunk& chunk)
{
    if (chunk.m_Page == UINT32_MAX)
    {
        delete[] chunk.m_Data;
        return;
    }
    DSRowPage& page = store->m_Pages[chunk.m_Page];
    if (!page.m_FreeCount)
        LinkFreePage(store, chunk.m_Page);
    memcpy(chunk.m_Data, &page.m_FreeChunk, sizeof(page.m_FreeChunk));
    page.m_FreeChunk = chunk.m_Offset;
    if (++page.m_FreeCount != store->m_PageSize / store->m_RowChunkSize)
        return;
    if (store->m_SparePageCount < store->m_MaxSparePages)
    {
        ++store->m_SparePageCount;
        return;
    }
    UnlinkFreePage(store, chunk.m_Page);
    delete[] page.m_Data;
    page.m_Data = 0;
    page.m_Next = store->m_FreePageSlot;
    store->m_FreePageSlot = chunk.m_Page;
}

void InitializeRowStorage(HDSContext store, DSTable* table)
{
    // Power-of-two row counts keep random lookup to a shift, mask and pointer load.
    uint64_t row_size = (uint64_t)table->m_RowStride * (table->m_RetainDefaults && !table->m_Pool ? 2 : 1);
    uint32_t shift = 0;
    if (row_size)
        while (shift < 30 && (row_size << (shift + 1)) <= store->m_RowChunkSize)
            ++shift;
    table->m_ChunkShift = shift;
    table->m_ChunkMask = (1u << shift) - 1;
    table->m_DefaultOffset = table->m_RetainDefaults && !table->m_Pool ? (1u << shift) * table->m_RowStride : 0;
}

void ReserveRowStorage(HDSContext store, DSTable* table, uint32_t num_rows)
{
    if (!table->m_RowStride)
        return;
    uint32_t num_chunks = (num_rows >> table->m_ChunkShift) + ((num_rows & table->m_ChunkMask) != 0);
    Reserve(table->m_Chunks, num_chunks);
    size_t size = ((size_t)table->m_RowStride << table->m_ChunkShift) + table->m_DefaultOffset;
    while (table->m_Chunks.Size() < num_chunks)
        table->m_Chunks.Push(AllocateChunk(store, size));
}

void TrimRowStorage(HDSContext store, DSTable* table, uint32_t num_rows)
{
    uint32_t num_chunks = (num_rows >> table->m_ChunkShift) + ((num_rows & table->m_ChunkMask) != 0);
    while (table->m_Chunks.Size() > num_chunks)
    {
        ReleaseChunk(store, table->m_Chunks.Back());
        table->m_Chunks.Pop();
    }
}

void DeleteRowPages(HDSContext store)
{
    for (uint32_t i = 0; i < store->m_Pages.Size(); ++i)
        delete[] store->m_Pages[i].m_Data;
}
