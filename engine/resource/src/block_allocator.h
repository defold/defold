#ifndef DM_BLOCK_ALLOCATOR_H
#define DM_BLOCK_ALLOCATOR_H

#include <stdint.h>

namespace dmBlockAllocator
{
    // A simple block allocator for scratch memory to use for small allocations instead
    // of pressuring malloc/free.
    //
    // It allocates blocks (or chunks) of memory via malloc and then allocates from
    // each block.
    //
    // It keeps track of the high and low range used in a block to try and reuse free
    // space but no real defragmentation is done.
    //
    // A block can only be released if it is deallocations are all from either the start
    // or the end if a free is done in the middle of a block it that leaves holes the
    // block will never reach a fully free state.
    //
    // One block is always allocated, but other blocks are dynamically allocated/freed.
    //
    // There is no thread syncronization so each context should only be used by one thread.
    //
    // If there is no space in the blocks or the allocation is deemed to big it will fall
    // back to malloc/free for the allocation.

    typedef struct Context* HContext;

    HContext CreateContext();
    void DeleteContext(HContext context);

    void* Allocate(HContext context, uint32_t size);
    void Free(HContext context, void* data, uint32_t size);

} // dmBlockAllocator

#endif
