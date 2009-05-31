
#include <string.h>
#include <stdlib.h>
#include "array.h"
#include "iterator.h"


void ArrayHelper::SetCapacity(uint32_t new_capacity, uint32_t type_size, uint32_t* ptr_first, uint32_t* ptr_last, uint32_t* ptr_end)
{
    uint32_t old_capacity = (*ptr_last - *ptr_first)/type_size;
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

    uint32_t old_size = (*ptr_end - *ptr_first)/type_size;
    uint32_t new_size = old_size;
    new_size = new_size < new_capacity ? new_size : new_capacity;

    if(old_capacity)
    {
        memcpy(new_block, (void *) *ptr_first , type_size*(old_size < new_capacity ? old_size : new_capacity));
        delete[] (uint8_t*) *ptr_first;
    }

    *ptr_first = (uint32_t) new_block;
    *ptr_end   = (uint32_t) (new_block + (new_size*type_size));
    *ptr_last  = (uint32_t) (new_block + (new_capacity*type_size));
}

