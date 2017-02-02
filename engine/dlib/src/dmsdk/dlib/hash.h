#ifndef DMSDK_HASH_H
#define DMSDK_HASH_H

#include <stdint.h>
#include "shared_library.h"

/**
* dmhash_t type declaration
*/
typedef uint64_t dmhash_t;


extern "C"
{

/**
 * Calculate 64-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 64-bit hash value from string
 * @param string String
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashString64(const char* string);

}

#endif // DMSDK_HASH_H
