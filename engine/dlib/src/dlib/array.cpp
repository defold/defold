
#include <string.h>
#include "array.h"


void dmArrayHelper::SetCapacity(uint32_t new_capacity, uint32_t type_size, uintptr_t* ptr_first, uintptr_t* ptr_last, uintptr_t* ptr_end)
{
    uintptr_t old_capacity = (*ptr_last - *ptr_first)/type_size;
    if(new_capacity == old_capacity)
        return;

    uint8_t *new_block;
    if(new_capacity)
    {
        new_block = new uint8_t[new_capacity*type_size];
        assert (new_block != 0 && "SetCapacity failed allocating memory");
    }
    else
        new_block = 0;

    uintptr_t old_size = (*ptr_end - *ptr_first)/type_size;
    uintptr_t new_size = old_size;
    new_size = new_size < new_capacity ? new_size : new_capacity;

    if(old_capacity)
    {
        memcpy(new_block, (void *)*ptr_first, (size_t)((size_t)type_size*(old_size < new_capacity ? old_size : new_capacity)));
        delete[] (uint8_t*) *ptr_first;
    }

    *ptr_first = (uintptr_t) new_block;
    *ptr_end   = (uintptr_t) (new_block + (new_size*type_size));
    *ptr_last  = (uintptr_t) (new_block + (new_capacity*type_size));
}

