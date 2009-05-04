#include <stddef.h>
#include "hash.h"

uint32_t hashlittle( const void *key, size_t length, uint32_t initval);
void hashlittle2(const void *key, size_t length, uint32_t *pc, uint32_t *pb);

uint32_t HashBuffer32(const void* buffer, uint32_t buffer_len)
{
    return hashlittle(buffer, buffer_len, 0);
}

uint64_t HashBuffer64(const void* buffer, uint32_t buffer_len)
{
    uint32_t h1, h2;
    hashlittle2(buffer, buffer_len, &h1, &h2);
    return (uint64_t(h1) << 32) | uint64_t(h2);
}
