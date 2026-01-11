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

#include "block_allocator.h"

#include <assert.h>
#include <stdlib.h>

#include <dlib/align.h>

namespace dmBlockAllocator
{
    static const uint32_t BLOCK_SIZE                   = 16384;
    static const uint32_t BLOCK_ALLOCATION_THRESHOLD   = BLOCK_SIZE / 2;
    static const uint16_t MAX_BLOCK_COUNT              = 8;
    static const uint32_t BLOCK_ALLOCATION_ALIGNEMENT  = sizeof(uint16_t);

    struct BlockData
    {
        uint32_t m_AllocationCount;
        uint32_t m_LowWaterMark;
        uint32_t m_HighWaterMark;
    };

    struct Block
    {
        uint8_t m_Data[BLOCK_SIZE];
    };

    struct Context
    {
        BlockData m_BlockDatas[MAX_BLOCK_COUNT];
        Block* m_Blocks[MAX_BLOCK_COUNT];
        Block m_InitialBlock;
    };

    void* Allocate(HContext context, uint32_t size)
    {
        uint32_t allocation_size = DM_ALIGN(sizeof(uint16_t) + size, BLOCK_ALLOCATION_ALIGNEMENT);
        if (allocation_size > BLOCK_ALLOCATION_THRESHOLD)
        {
            uint16_t* res = (uint16_t*)malloc(sizeof(uint16_t) + size);
            *res          = MAX_BLOCK_COUNT;
            return &res[1];
        }
        uint16_t first_free = MAX_BLOCK_COUNT;
        for (uint16_t block_index = 0; block_index < MAX_BLOCK_COUNT; ++block_index)
        {
            Block* block = context->m_Blocks[block_index];
            if (block == 0x0)
            {
                first_free = (first_free == MAX_BLOCK_COUNT) ? block_index : first_free;
                continue;
            }
            BlockData* block_data = &context->m_BlockDatas[block_index];
            if (block_data->m_LowWaterMark >= allocation_size)
            {
                block_data->m_LowWaterMark -= allocation_size;
                block_data->m_AllocationCount++;
                uint16_t* ptr = (uint16_t*)&block->m_Data[block_data->m_LowWaterMark];
                *ptr          = block_index;
                return &ptr[1];
            }
            if (block_data->m_HighWaterMark + allocation_size <= BLOCK_SIZE)
            {
                block_data->m_AllocationCount++;
                uint16_t* ptr = (uint16_t*)&block->m_Data[block_data->m_HighWaterMark];
                block_data->m_HighWaterMark += allocation_size;
                *ptr = block_index;
                return &ptr[1];
            }
        }
        if (first_free != MAX_BLOCK_COUNT)
        {
            Block* block                          = new Block;
            BlockData* block_data                 = &context->m_BlockDatas[first_free];
            block_data->m_AllocationCount         = 1;
            block_data->m_LowWaterMark            = 0;
            block_data->m_HighWaterMark           = allocation_size;
            uint16_t* ptr                         = (uint16_t*)&block->m_Data[0];
            *ptr                                  = first_free;
            context->m_Blocks[first_free] = block;
            return &ptr[1];
        }
        uint16_t* res = (uint16_t*)malloc(sizeof(uint16_t) + size);
        *res          = MAX_BLOCK_COUNT;
        return &res[1];
    }

    void Free(HContext context, void* data, uint32_t size)
    {
        uint16_t* ptr = (uint16_t*)data;
        --ptr;
        uint16_t block_index = *ptr;
        if (block_index == MAX_BLOCK_COUNT)
        {
            free(ptr);
            return;
        }
        assert(block_index < MAX_BLOCK_COUNT);
        uint16_t allocation_size = DM_ALIGN(sizeof(uint16_t) + size, BLOCK_ALLOCATION_ALIGNEMENT);
        Block* block             = context->m_Blocks[block_index];
        assert(block != 0x0);
        BlockData* block_data = &context->m_BlockDatas[block_index];
        assert(block_data->m_AllocationCount > 0);

        block_data->m_AllocationCount--;
        if (0 == block_data->m_AllocationCount)
        {
            if (block_index != 0)
            {
                delete block;
                context->m_Blocks[block_index] = 0x0;
            }
            return;
        }
        if ((uint8_t*)ptr == &block->m_Data[block_data->m_LowWaterMark])
        {
            block_data->m_LowWaterMark += allocation_size;
        }
        else if ((uint8_t*)ptr == &block->m_Data[block_data->m_HighWaterMark - allocation_size])
        {
            block_data->m_HighWaterMark -= allocation_size;
        }
    }

    HContext CreateContext()
    {
        Context* context = (Context*)malloc(sizeof(Context));
        context->m_BlockDatas[0].m_AllocationCount = 0;
        context->m_BlockDatas[0].m_LowWaterMark    = 0;
        context->m_BlockDatas[0].m_HighWaterMark   = 0;
        context->m_Blocks[0]                       = &context->m_InitialBlock;
        for (uint16_t i = 1; i < MAX_BLOCK_COUNT; ++i)
        {
            context->m_Blocks[i] = 0x0;
        }
        return context;
    }

    void DeleteContext(HContext context)
    {
        if (!context)
        {
            return;
        }
        assert(context->m_BlockDatas[0].m_AllocationCount == 0);
        for (uint16_t i = 1; i < MAX_BLOCK_COUNT; ++i)
        {
            assert(context->m_Blocks[i] == 0x0);
        }
        free(context);
    }
}
