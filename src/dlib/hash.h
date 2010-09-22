#ifndef DM_HASH_H
#define DM_HASH_H

#include <stdint.h>
#include "shared_library.h"

extern "C"
{

/**
 * Hash state used for 32-bit incremental hashing
 */
struct HashState32
{
    uint32_t m_Hash;
    uint32_t m_Tail;
    uint32_t m_Count;
    uint32_t m_Size;
};

/**
 * Hash state used for 64-bit incremental hashing
 */
struct HashState64
{
    uint64_t m_Hash;
    uint64_t m_Tail;
    uint32_t m_Count;
    uint32_t m_Size;
};

/**
 * Initialize hash-state for 32-bit incremental hashing
 * @param hash_state Hash state
 */
DM_DLLEXPORT void dmHashInit32(HashState32* hash_state);

/**
 * Incremental hashing
 * @param hash_state Hash state
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer32(HashState32* hash_state, const void* buffer, uint32_t buffer_len);

/**
 * Finalize hashing
 * @param hash_state Hash state
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashFinal32(HashState32* hash_state);

/**
 * Initialize hash-state for 64-bit incremental hashing
 * @param hash_state Hash state
 */
DM_DLLEXPORT void dmHashInit64(HashState64* hash_state);

/**
 * Incremental hashing
 * @param hash_state Hash state
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint64_t buffer_len);

/**
 * Finalize hashing
 * @param hash_state Hash state
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashFinal64(HashState64* hash_state);

/**
 * Calculate 32-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashBuffer32(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 64-bit hash value from buffer
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);

/**
 * Calculate 32-bit hash value from string
 * @param string String
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashString32(const char* string);

/**
 * Calculate 64-bit hash value from string
 * @param string String
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashString64(const char* string);

}

#endif // DM_HASH_H
