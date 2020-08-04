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

#ifndef DM_MEMORY_H
#define DM_MEMORY_H

namespace dmMemory
{
    /**
     * Aligned memory allocation result
     */
    enum Result
    {
        RESULT_OK             = 0,    //!< RESULT_OK
        RESULT_INVAL          = -1,   //!< RESULT_INVAL
        RESULT_NOMEM          = -2,   //!< RESULT_NOMEM
    };

    /**
     * Allocate size bytes of uninitialized storage whose alignment is specified by alignment.
     * @param memptr Pointer to a void* where the allocated pointer address should be stored.
     * @param alignment The alignment value, which must be an integer power of 2.
     * @param size Size of the requested memory allocation.
     * @return Returns RESULT_OK on success, RESULT_INVAL if alignment is not a power of 2 and RESULT_NOMEM if out of memory.
     */
    Result AlignedMalloc(void **memptr, unsigned int alignment,  unsigned int size);

    /**
     * Frees a block of memory that was allocated with dmMemory::AlignedMalloc
     * @param memptr A pointer to the memory block that was returned by dmMemory::AlignedMalloc
     */
    void AlignedFree(void* memptr);

}

#endif // DM_MEMORY_H
