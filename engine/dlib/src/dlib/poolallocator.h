#ifndef DM_POOL_ALLOCATOR_H
#define DM_POOL_ALLOCATOR_H

#include <stdint.h>

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
