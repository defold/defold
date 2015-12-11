#include "array.h"
#include "log.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

namespace dmArrayUtil
{
    void SetCapacity(uint32_t capacity, uint32_t type_size, uintptr_t* first, uintptr_t* last, uintptr_t* end, bool use_sol_allocator)
    {
        uintptr_t old_capacity = (*last - *first) / type_size;
        // early-out for now change
        if (capacity == old_capacity)
            return;

        // new backing storage
        uint8_t *new_storage;
        if (capacity)
        {
            if (use_sol_allocator)
            {
                new_storage = (uint8_t *)runtime_alloc_array(type_size, capacity);
            }
            else
            {
                new_storage = (uint8_t *)malloc(type_size * capacity);
            }
            assert (new_storage != 0 && "SetCapacity could not allocate memory");
        }
        else
        {
            new_storage = 0;
        }

        uintptr_t old_size = (*end - *first) / type_size;
        // truncate size if the new capacity is less
        uintptr_t new_size = old_size < capacity ? old_size : capacity;

        // copy content if any
        if (old_capacity)
        {
            memcpy(new_storage, (void *)*first, (size_t)(type_size * (size_t)new_size));

            if (use_sol_allocator)
            {
                runtime_unpin((void *)*first);
            }
            else
            {
                free((void *)*first);
            }
        }

        *first = (uintptr_t)new_storage;
        *end   = (uintptr_t)(new_storage + (new_size * type_size));
        *last  = (uintptr_t)(new_storage + (capacity * type_size));
    }
}

