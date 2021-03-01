// Copyright 2020 The Defold Foundation
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

#include <stdint.h>
#include "shared_library.h"

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

extern "C"
{


/*# calculate 64-bit hash value from buffer
 *
 * @name dmHashBuffer64
 * @param buffer [type:const void*] Buffer
 * @param buffer_len [type:uint32_t] Length of buffer
 * @return hash [type:uint64_t] hash value
 */
DM_DLLEXPORT uint64_t dmHashBuffer64(const void* buffer, uint32_t buffer_len);


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
 * @return [type:const char*] Original string value
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


}

#endif // DMSDK_HASH_H
