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

#ifndef DM_POOL_ALLOCATOR_H
#define DM_POOL_ALLOCATOR_H

namespace dmPoolAllocator
{
    /**
     * Pool allocator handle. Allocated memory in pages internally. Individual allocations are not freeable.
     * Instead all memory is freed at once when the pool is destroyed.
     */
    typedef struct Pool* HPool;

    /**
     * Create a new pool
     * @param chunk_size chunk size
     * @return pool allocator handle
     */
    HPool New(uint32_t chunk_size);

    /**
     * Delete pool and free all allocated memory.
     * @param pool pool allocator handle
     */
    void Delete(HPool pool);

    /**
     * Allocate memory.
     * @note size must be <= page_size
     * @param pool pool allocator handle
     * @param size size
     * @return pointer to memory.
     */
    void* Alloc(HPool pool, uint32_t size);

    /**
     * Duplicate a string. Similar to strdup
     * @param pool pool allocator handle
     * @param string string to duplicate
     * @return duplicated string
     */
    char* Duplicate(HPool pool, const char* string);

}

#endif
