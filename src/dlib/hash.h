#ifndef DM_HASH_H
#define DM_HASH_H

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
uint32_t DM_DLLEXPORT dmHashBuffer32(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 64-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
uint64_t DM_DLLEXPORT dmHashBuffer64(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 32-bit hash value from string
 * @param string String
 * @return Hash value
 */
uint32_t DM_DLLEXPORT dmHashString32(const char* string);

/**
 * Calculate 64-bit hash value from string
 * @param string String
 * @return Hash value
 */
uint64_t DM_DLLEXPORT dmHashString64(const char* string);

}

#endif // DM_HASH_H
