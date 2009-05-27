
#include <string.h>
#include <stdlib.h>
#include "array.h"
#include "iterator.h"


uint8_t* ArrayHelper::Alloc(uint32_t size)
{
#ifdef ARRAY_BUFFER_OVERRUN_CHECK
    uint8_t *p = new uint8_t[size + 8];
    *((uint32_t *)p) = 0xB00B;
    *((uint32_t *)(p + size + 4)) = 0xBABE;
    return p + 4;
#else
    return new uint8_t[size];
#endif
}


void ArrayHelper::Free(uint8_t *p)
{
#ifdef ARRAY_BUFFER_OVERRUN_CHECK
    p -= 4;
#endif
    delete[] p;
}


#ifdef ARRAY_BUFFER_OVERRUN_CHECK
int ArrayHelper::Check(void **array, uint32_t user_allocated)
{
    uint32_t *begin = (uint32_t *) array[0];
    uint32_t *last = (uint32_t *) array[2];
    if (begin && !user_allocated)
    {
        int t = *(begin-1) == 0xB00B && *last == 0xBABE;
        return t;
    }
    else
    {
        return 1;
    }
}
#endif


// TODO: Should use intptr instead of uint32_t
void ArrayHelper::SetCapacity(void **_array, uint32_t type_size, uint32_t new_capacity)
{
    uint32_t begin = (uint32_t) _array[0];
    uint32_t end = (uint32_t) _array[1];

    if (new_capacity == 0)
    {
        if (begin)
        {
            ArrayHelper::Free((uint8_t *)begin);
        }
        _array[0] = (void *) 0;
        _array[1] = (void *) 0;
        _array[2] = (void *) 0;
        return;
    }

    uint32_t current_size = (end-begin) / type_size;
    uint32_t new_size = current_size > new_capacity ?  new_capacity : current_size;
    void* tmp = (void *) ArrayHelper::Alloc(new_capacity * type_size);
    if (begin)
    {
        memcpy(tmp, (void *) begin, new_size * type_size);
        ArrayHelper::Free((uint8_t *)begin);
    }

    uint32_t new_begin = (uint32_t) tmp;
    uint32_t new_end = new_begin + new_size * type_size;
    uint32_t new_last = new_begin + new_capacity * type_size;
    _array[0] = (void *) new_begin;
    _array[1] = (void *) new_end;
    _array[2] = (void *) new_last;
}
