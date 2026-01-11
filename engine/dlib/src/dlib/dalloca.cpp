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

#include "dalloca.h"
#include <stdlib.h>
#include <assert.h>

void* dmMemAlloc(dmAllocator* allocator, size_t size)
{
    if (allocator->m_Alloc)
        return allocator->m_Alloc(size, allocator->m_UserData);
    return malloc(size);
}

void dmMemFree(dmAllocator* allocator, void* mem)
{
    if (allocator->m_Free)
        allocator->m_Free(mem, allocator->m_UserData);
    free(mem);
}

static void* dmFixedMemAlloc(size_t size, void* user_data)
{
    dmFixedMemAllocator* allocator = (dmFixedMemAllocator*)user_data;
    if ((size + allocator->m_Used) > allocator->m_Capacity)
        return 0;
    void* mem = (void*)(allocator->m_Memory + allocator->m_Used);
    allocator->m_Used += size;
    return mem;
}

static void dmFixedMemFree(void* mem, void* user_data)
{
    (void)mem;
    (void)user_data;
}

void dmFixedMemAllocatorInit(dmFixedMemAllocator* allocator, size_t size, void* mem)
{
    allocator->m_Allocator.m_Alloc = dmFixedMemAlloc;
    allocator->m_Allocator.m_Free = dmFixedMemFree;
    allocator->m_Allocator.m_UserData = allocator;
    allocator->m_Memory = (uint8_t*)mem;
    allocator->m_Capacity = size;
    allocator->m_Used = 0;
}
