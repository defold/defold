#ifndef DM_HASH_H
#define DM_HASH_H

#include <dmsdk/dlib/hash.h>

/**
 * Max length for reverse hashing entries. Buffer length larger than this will not be stored for reverse hashing.
 */
const uint32_t DMHASH_MAX_REVERSE_LENGTH = 1024U;

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
    uint32_t m_ReverseHashEntryIndex;

    HashState32() {}
private:
    // Restrict copy-construction etc.
    HashState32(const HashState32&) {}
    void operator =(const HashState32&) {}
    void operator ==(const HashState32&) {}
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
    uint32_t m_ReverseHashEntryIndex;

    HashState64() {}
private:
    // Restrict copy-construction etc.
    HashState64(const HashState64&) {}
    void operator =(const HashState64&) {}
    void operator ==(const HashState64&) {}
};


/**
 * Initialize hash-state for 32-bit incremental hashing
 * @param hash_state Hash state
 * @param reverse_hash true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH
 */
DM_DLLEXPORT void dmHashInit32(HashState32* hash_state, bool reverse_hash);


/**
 * Clone 32-bit incremental hash state
 * @param hash_state Hash state
 * @param source_hash_state Source hash state
 * @param reverse_hash true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH. Ignored if source state reverse hashing is disabled.
 */
DM_DLLEXPORT void dmHashClone32(HashState32* hash_state, const HashState32* source_hash_state, bool reverse_hash);


/**
 * Incremental hashing
 * @param hash_state Hash state
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer32(HashState32* hash_state, const void* buffer, uint32_t buffer_len);

/**
 * Finalize incremental hashing and release associated resources
 * @param hash_state Hash state
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashFinal32(HashState32* hash_state);

/**
 * Release incremental hashing resources
 * Used to release assocciated resources for intermediate incremental hash states.
 * @param hash_state Hash state
 */
DM_DLLEXPORT void dmHashRelease32(HashState32* hash_state);

/**
 * Initialize hash-state for 64-bit incremental hashing
 * @param hash_state Hash state
 * @param reverse_hash true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH
 */
DM_DLLEXPORT void dmHashInit64(HashState64* hash_state, bool reverse_hash);

/**
 * Clone 64-bit incremental hash state
 * @param hash_state Hash state
 * @param source_hash_state Source hash state
 * @param reverse_hash true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH. Ignored if source state reverse hashing is disabled.
 */
DM_DLLEXPORT void dmHashClone64(HashState64* hash_state, const HashState64* source_hash_state, bool reverse_hash);

/**
 * Incremental hashing
 * @param hash_state Hash state
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint32_t buffer_len);

/**
 * Finalize incremental hashing and release associated resources
 * @param hash_state Hash state
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashFinal64(HashState64* hash_state);

/**
 * Release incremental hashing resources
 * Used to release assocciated resources for intermediate incremental hash states.
 * @param hash_state Hash state
 */
DM_DLLEXPORT void dmHashRelease64(HashState64* hash_state);

/**
 * Calculate 64-bit hash value from buffer. Special version of dmHashBuffer64 with reverse always disabled.
 * Used in situations when no memory allocations can occur, eg profiling scenarios.
 * @param buffer Buffer
 * @param buffer_len Length of buffer
 * @return Hash value
 */
DM_DLLEXPORT uint64_t dmHashBufferNoReverse64(const void* buffer, uint32_t buffer_len);

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
 * Calculate 32-bit hash value from string
 * @param string String
 * @return Hash value
 */
DM_DLLEXPORT uint32_t dmHashString32(const char* string);

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
 * Reverse hash key entry removal.
 * @param hash hash key to erase
 */
DM_DLLEXPORT void dmHashReverseErase32(uint32_t hash);

/**
 * Reverse hash lookup. Maps hash to original data. It is guaranteed that the returned
 * buffer is null-terminated. If the buffer contains a valid c-string
 * it can safely be used in printf and friends.
 * @param hash hash to lookup
 * @param length original data length. Optional argument and NULL-pointer is accepted.
 * @return pointer to buffer. 0 if no reverse exists or if reverse lookup is disabled
 */
DM_DLLEXPORT const void* dmHashReverse64(uint64_t hash, uint32_t* length);

/**
 * Reverse hash key entry removal.
 * @param hash hash key to erase
 */
DM_DLLEXPORT void dmHashReverseErase64(uint64_t hash);

/** Returns the original string used to produce a hash.
 * Always returns a null terminated string. Returns "<unknown>" if the original string wasn't found.
 */
DM_DLLEXPORT const char* dmHashReverseSafe64(uint64_t hash);



}

#endif // DM_HASH_H
