#include <stddef.h>
#include "hash.h"

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);
void hashlittle2(const void *key, size_t length, uint32_t *pc, uint32_t *pb);

uint32_t dmHashBuffer32(const void* buffer, uint32_t buffer_len)
{
    return hashlittle(buffer, buffer_len, 0);
}

#include <stdio.h>
uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len)
{
    uint32_t h1 = 0;
    uint32_t h2 = 0;

    hashlittle2(buffer, buffer_len, &h1, &h2);
    return (uint64_t(h1) << 32) | uint64_t(h2);
}
