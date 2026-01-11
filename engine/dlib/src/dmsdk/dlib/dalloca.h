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

/*# SDK Alloca API documentation
 * Alloca functions.
 *
 * @document
 * @name Alloca
 * @namespace dmMemory
 * @language C++
 */

#ifndef DMSDK_DALLOCA_H
#define DMSDK_DALLOCA_H

#include <stdint.h>

#if defined(DM_PLATFORM_VENDOR)
    #include "alloca_vendor.h"
#elif defined(_WIN32)
    #include <malloc.h>
    #if !defined(alloca)
        #define alloca(_SIZE) _alloca(_SIZE) // done in malloc.h if _CRT_INTERNAL_NONSTDC_NAMES is non-zero
    #endif
#else
    #include <alloca.h>
#endif

#define dmAlloca(SIZE) alloca(SIZE)

// TODO: Formalize this a bit more and move it into a separate file

// Intentionally undocumented!
typedef struct dmAllocator
{
    void* (*m_Alloc)(size_t size, void* user_data);
    void (*m_Free)(void* mem, void* user_data);
    void* m_UserData;
} dmAllocator;

void* dmMemAlloc(dmAllocator* allocator, size_t size);
void dmMemFree(dmAllocator* allocator, void* mem);

// Intentionally undocumented!
typedef struct dmFixedMemAllocator
{
    dmAllocator m_Allocator;
    uint8_t* m_Memory;
    size_t m_Used;
    size_t m_Capacity;
} dmFixedMemAllocator;

void dmFixedMemAllocatorInit(dmFixedMemAllocator* allocator, size_t size, void* mem);

#endif // DMSDK_DALLOCA_H
