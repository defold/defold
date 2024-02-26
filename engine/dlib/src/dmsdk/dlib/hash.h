// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_HASH_H
#define DMSDK_HASH_H

#include <assert.h>
#include <stdint.h>
#include "shared_library.h"
#include "dalloca.h" // dmFixedMemAllocator

#if defined(DM_PLATFORM_VENDOR)
    #include <dmsdk/dlib/hash_vendor.h>
#elif defined(__linux__) && !defined(__ANDROID__)
    #define DM_HASH_FMT "%016lx"
#else
    #define DM_HASH_FMT "%016llx"
#endif

#ifndef DM_HASH_FMT
        #error "DM_HASH_FMT was not defined!"
#endif

/*# SDK Hash API documentation
 * Hash functions.
 *
 * @document
 * @name Hash
 * @namespace dmHash
 * @path engine/dlib/src/dmsdk/dlib/hash.h
 */

/*# dmhash_t type definition
 *
 * ```cpp
 * typedef uint64_t dmhash_t
 * ```
 *
 * @typedef
 * @name dmhash_t
 *
 */
typedef uint64_t dmhash_t;

#ifdef __cplusplus
extern "C" {
#endif

/*#
 * Calculate 32-bit hash value from buffer
 * @name dmHashBuffer32
 * @param buffer [type:const void*] Buffer
 * @param buffer_len [type:uint32_t] Length of buffer
 * @return hash [type:uint32_t] hash value
 */
DM_DLLEXPORT uint32_t dmHashBuffer32(const void* buffer, uint32_t buffer_len);


/*# calculate 64-bit hash value from buffer
 *
 * @name dmHashBuffer64
 * @param buffer [type:const void*] Buffer
 * @param buffer_len [type:uint32_t] Length of buffer
 * @return hash [type:uint64_t] hash value
 */
DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);


/*#
 * Calculate 32-bit hash value from string
 * @name dmHashString32
 * @param string [type:const char*] Null terminated string
 * @return hash [type:uint32_t] hash value
 */
DM_DLLEXPORT uint32_t dmHashString32(const char* string);


/*# calculate 64-bit hash value from string
 *
 * @name dmHashString64
 * @param string [type:const char*] Null terminated string
 * @return hash [type:uint64_t] hash value
 */
DM_DLLEXPORT uint64_t dmHashString64(const char* string);


/*# get string value from hash
 *
 * Returns the original string used to produce a hash.
 * Always returns a null terminated string. Returns "<unknown>" if the original string wasn't found.
 * @name dmHashReverseSafe64
 * @param hash [type:uint64_t] hash value
 * @return [type:const char*] Original string value or "<unknown>" if it wasn't found.
 * @note Do not store this pointer
 */
DM_DLLEXPORT const char* dmHashReverseSafe64(uint64_t hash);

/*# get string value from hash
 *
 * Reverse hash lookup. Maps hash to original data. It is guaranteed that the returned
 * buffer is null-terminated. If the buffer contains a valid c-string
 * it can safely be used in printf and friends.
 *
 * @name dmHashReverseSafe64
 * @param hash [type:uint64_t] hash to lookup
 * @param length [type:uint32_t*] original data length. Optional argument and NULL-pointer is accepted.
 * @return [type:const char*] pointer to buffer. 0 if no reverse exists or if reverse lookup is disabled
 * @note Do not store this pointer
 */
DM_DLLEXPORT const void* dmHashReverse64(uint64_t hash, uint32_t* length);

/*# get string value from hash
 *
 * Returns the original string used to produce a hash.
 * Always returns a null terminated string. Returns "<unknown>" if the original string wasn't found.
 * @name dmHashReverseSafe32
 * @param hash [type:uint32_t] hash value
 * @return [type:const char*] Original string value or "<unknown>" if it wasn't found.
 * @note Do not store this pointer
 */
DM_DLLEXPORT const char* dmHashReverseSafe32(uint32_t hash);

/*# get string value from hash
 *
 * Reverse hash lookup. Maps hash to original data. It is guaranteed that the returned
 * buffer is null-terminated. If the buffer contains a valid c-string
 * it can safely be used in printf and friends.
 *
 * @name dmHashReverseSafe32
 * @param hash [type:uint32_t] hash to lookup
 * @param length [type:uint32_t*] original data length. Optional argument and NULL-pointer is accepted.
 * @return [type:const char*] pointer to buffer. 0 if no reverse exists or if reverse lookup is disabled
 * @note Do not store this pointer
 */
DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length);

#ifdef __cplusplus
} // extern "C"
#endif


#ifdef __cplusplus

/*#
 * Hash state used for 32-bit incremental hashing
 * @struct
 * @name HashState32
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


/*#
 * Hash state used for 64-bit incremental hashing
 * @struct
 * @name HashState64
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


/*#
 * Initialize hash-state for 32-bit incremental hashing
 * @name dmHashInit32
 * @param hash_state [type: HashState32*] Hash state
 * @param reverse_hash [type: bool] true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH
 */
DM_DLLEXPORT void dmHashInit32(HashState32* hash_state, bool reverse_hash);


/*#
 * Clone 32-bit incremental hash state
 * @name dmHashClone32
 * @param hash_state [type: HashState32*] Hash state
 * @param source_hash_state [type: HashState32*] Source hash state
 * @param reverse_hash [type: bool] true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH. Ignored if source state reverse hashing is disabled.
 */
DM_DLLEXPORT void dmHashClone32(HashState32* hash_state, const HashState32* source_hash_state, bool reverse_hash);

/*#
 * Incremental hashing
 * @name dmHashUpdateBuffer32
 * @param hash_state [type: HashState32*] Hash state
 * @param buffer [type:const void*] Buffer
 * @param buffer_len [type:uint32_t] Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer32(HashState32* hash_state, const void* buffer, uint32_t buffer_len);

/*#
 * Finalize incremental hashing and release associated resources
 * @name dmHashFinal32
 * @param hash_state [type: HashState32*] Hash state
 * @return hash [type: uint32_t] the hash value
 */
DM_DLLEXPORT uint32_t dmHashFinal32(HashState32* hash_state);

/*#
 * Release incremental hashing resources
 * Used to release assocciated resources for intermediate incremental hash states.
 * @name dmHashRelease32
 * @param hash_state [type: HashState32*] Hash state
 */
DM_DLLEXPORT void dmHashRelease32(HashState32* hash_state);

/*#
 * Initialize hash-state for 64-bit incremental hashing
 * @name dmHashInit64
 * @param hash_state [type: HashState64*] Hash state
 * @param reverse_hash true to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH
 */
DM_DLLEXPORT void dmHashInit64(HashState64* hash_state, bool reverse_hash);

/*#
 * Clone 64-bit incremental hash state
 * @name dmHashClone64
 * @param hash_state [type: HashState64*] Hash state
 * @param source_hash_state [type: HashState64*] Source hash state
 * @param reverse_hash true [type: bool] to enable reverse hashing of buffers up to ::DMHASH_MAX_REVERSE_LENGTH. Ignored if source state reverse hashing is disabled.
 */
DM_DLLEXPORT void dmHashClone64(HashState64* hash_state, const HashState64* source_hash_state, bool reverse_hash);

/*#
 * Incremental hashing
 * @name dmHashUpdateBuffer64
 * @param hash_state [type: HashState64*] Hash state
 * @param buffer [type:const void*] Buffer
 * @param buffer_len [type:uint32_t] Length of buffer
 */
DM_DLLEXPORT void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint32_t buffer_len);

/*#
 * Finalize incremental hashing and release associated resources
 * @name dmHashFinal64
 * @param hash_state [type: HashState64*] Hash state
 * @return hash [type:uint64_t] The hash value
 */
DM_DLLEXPORT uint64_t dmHashFinal64(HashState64* hash_state);

/*#
 * Release incremental hashing resources
 * Used to release assocciated resources for intermediate incremental hash states.
 * @name dmHashRelease64
 * @param hash_state [type: HashState64*] Hash state
 */
DM_DLLEXPORT void dmHashRelease64(HashState64* hash_state);


/*#
 * Allocate stack memory context for safely reversing hash values into strings
 * @define
 * @name DM_HASH_REVERSE_MEM
 * @param name [type: symbol] The name of the dmAllocator struct
 * @param size [type: size_t] The max size of the stack allocated context
 */
#define DM_HASH_REVERSE_MEM(_NAME, _CAPACITY) \
    uint8_t _NAME ## _mem_ ## __LINE__ [_CAPACITY]; \
    dmFixedMemAllocator _NAME ## __LINE__; \
    dmFixedMemAllocatorInit(& _NAME ## __LINE__, _CAPACITY, _NAME ## _mem_ ## __LINE__); \
    dmAllocator& _NAME = (_NAME ## __LINE__).m_Allocator;


/*# get string value from hash
 *
 * Returns the original string used to produce a hash.
 * @name dmHashReverseSafe64Alloc
 * @param allocator [type:dmAllocator*] The reverse hash allocator
 * @param hash [type:uint64_t] hash value
 * @return [type:const char*] Original string value or "<unknown:value>" if it wasn't found,
 *                            or "<unknown>" if the allocator failed to allocate more memory.
 *                            Always returns a null terminated string.
 * @note This function is thread safe
 * @note The pointer is valid during the scope of the allocator
 *
 * @examples
 *
 * Get the string representaiton of a hash value
 *
 * ```cpp
 * DM_HASH_REVERSE_MEM(hash_ctx, 128);
 * const char* reverse = (const char*) dmHashReverseSafe64Alloc(&hash_ctx, hash);
 * ```
 */
DM_DLLEXPORT const char* dmHashReverseSafe64Alloc(dmAllocator* allocator, uint64_t hash);


/*# get string value from hash
 *
 * Returns the original string used to produce a hash.
 *
 * @name dmHashReverseSafe32Alloc
 * @param allocator [type:dmAllocator*] The reverse hash allocator
 * @param hash [type:uint32_t] hash value
 * @return [type:const char*] Original string value or "<unknown:value>" if it wasn't found,
 *                            or "<unknown>" if the allocator failed to allocate more memory.
 *                            Always returns a null terminated string.
 * @note This function is thread safe
 * @note The pointer is valid during the scope of the allocator
 *
 * @examples
 *
 * Get the string representaiton of a hash value
 *
 * ```cpp
 * DM_HASH_REVERSE_MEM(hash_ctx, 128);
 * const char* reverse = (const char*) dmHashReverseSafe32Alloc(&hash_ctx, hash);
 * ```
 */
DM_DLLEXPORT const char* dmHashReverseSafe32Alloc(dmAllocator* allocator, uint32_t hash);

#endif // __cplusplus

#endif // DMSDK_HASH_H
