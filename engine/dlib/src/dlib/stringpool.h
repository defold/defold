// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_STRINGPOOL_H
#define DM_STRINGPOOL_H

namespace dmStringPool
{
    /**
     * String pool handle.
     */
    typedef struct Pool* HPool;

    /**
     * Create a new string pool. Internally strings are allocated in pages of 4k.
     * @return String pool
     */
    HPool New();

    /**
     * Delete string pool
     * @param pool Pool to delete
     */
    void Delete(HPool pool);

    /**
     * Add string to string pool. Similar functionality to String.intern() in java.
     * The string pool stores only a single copy of each distinct string. Therefore it is
     * guaranteed that two identical strings will have the same pointer value.
     * @note The identical property is valid if and only if two distinct strings result
     * in two distinct hash values.
     * 
     * @param pool String pool
     * @param string String to add to pool
     * @return Pointer to string added
     */
    const char* Add(HPool pool, const char* string, uint32_t string_length, uint32_t string_hash);
}

#endif // DM_STRINGPOOL_H
