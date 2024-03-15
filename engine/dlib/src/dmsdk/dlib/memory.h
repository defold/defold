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

#ifndef DMSDK_MEMORY_H
#define DMSDK_MEMORY_H

/*# SDK Memory API documentation
 * Memory allocation functions
 *
 * @document
 * @name Memory
 * @namespace dmMemory
 * @path engine/dlib/src/dmsdk/dlib/memory.h
 */

namespace dmMemory
{
    /*# aligned memory allocation result
     *
     * Aligned memory allocation result
     *
     * @enum
     * @name Result
     * @member dmMemory::RESULT_OK 0
     * @member dmMemory::RESULT_INVAL -1
     * @member dmMemory::RESULT_NOMEM -2
     */
    enum Result
    {
        RESULT_OK             = 0,
        RESULT_INVAL          = -1,
        RESULT_NOMEM          = -2,
    };

    /*#
     * Allocate size bytes of uninitialized storage whose alignment is specified by alignment.
     * @name AlignedMalloc
     * @param memptr [type: void**] Pointer to a void* where the allocated pointer address should be stored.
     * @param alignment [type: uint32_t] The alignment value, which must be an integer power of 2.
     * @param size [type: uint32_t] Size of the requested memory allocation.
     * @return result [type: Result] Returns RESULT_OK on success, RESULT_INVAL if alignment is not a power of 2 and RESULT_NOMEM if out of memory.
     */
    Result AlignedMalloc(void** memptr, unsigned int alignment,  unsigned int size);

    /*#
     * Frees a block of memory that was allocated with dmMemory::AlignedMalloc
     * @name AlignedFree
     * @param memptr [type: void*] A pointer to the memory block that was returned by dmMemory::AlignedMalloc
     */
    void AlignedFree(void* memptr);
}

#endif // DMSDK_MEMORY_H
