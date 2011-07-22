#ifndef DM_HASH_H
#define DM_HASH_H

#include <stdint.h>
#include "shared_library.h"

typedef uint64_t dmhash_t;

struct dmReverseHashEntry
{
    void*    m_Value;
    uint32_t m_Length;

    inline dmReverseHashEntry() {}

    dmReverseHashEntry(void* value, uint32_t length)
    {
        m_Value = value;
        m_Length = length;
    }
};

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
    dmReverseHashEntry m_ReverseEntry;
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
    dmReverseHashEntry m_ReverseEntry;
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
DM_DLLEXPORT void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint32_t buffer_len);

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
 * Calculate 32-bit hash value from buffer. Special version of dmHashBuffer32 with reverse always disabled.
 * Used in situations when no memory allocations can occur, eg profiling scenarios.
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashBufferNoReverse32(const void* buffer, uint32_t buffer_len);

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

/**
 * Enable/disable support for reverse hash lookup
 * @param enable true for enable
 */
DM_DLLEXPORT void dmHashEnableReverseHash(bool enable);

/**
 * Reverse hash lookup. Maps hash to original data. It is guaranteed that the returned
 * buffer is null-terminated. If the buffer contains a valid c-string
 * it can safely be used in printf and friends.
 * @param hash hash to lookup
 * @param length original data length. Optional argument and NULL-pointer is accepted.
 * @return pointer to buffer. 0 if no reverse exists or if reverse lookup is disabled
 */
DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length);

/**
 * Reverse hash lookup. Maps hash to original data. It is guaranteed that the returned
 * buffer is null-terminated. If the buffer contains a valid c-string
 * it can safely be used in printf and friends.
 * @param hash hash to lookup
 * @param length original data length. Optional argument and NULL-pointer is accepted.
 * @return pointer to buffer. 0 if no reverse exists or if reverse lookup is disabled
 */
DM_DLLEXPORT const void* dmHashReverse64(uint64_t hash, uint32_t* length);

}

#endif // DM_HASH_H
