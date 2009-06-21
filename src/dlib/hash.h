#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include "shared_library.h"

extern "C"
{

/**
 * Calculate 32-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
uint32_t DLLEXPORT HashBuffer32(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 64-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
uint64_t DLLEXPORT HashBuffer64(const void* buffer, uint32_t buffer_len);

}

#endif // HASH_H
