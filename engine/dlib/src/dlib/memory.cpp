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

#include "memory.h"
#include "dalloca.h"
#include <stdlib.h>
#include <errno.h>
#if defined(__ANDROID__) || defined(_MSC_VER)
#include <malloc.h>
#endif

namespace dmMemory
{

    Result AlignedMalloc(void **memptr, unsigned alignment, unsigned size)
    {
        int error = 0;

        if (alignment == 0 || alignment % 2 != 0) {
            return RESULT_INVAL;
        }

#if defined(__ANDROID__)
        *(memptr) = memalign(alignment, size);
        if (*(memptr) == 0) {
            error = errno;
        }
#elif defined(__GNUC__)
        error = posix_memalign(memptr, alignment, size);
#elif defined(_MSC_VER)
        *(memptr) = _aligned_malloc(size, alignment);
        if (*(memptr) == 0) {
            error = errno;
        }
#else
        #error "dmMemory::AlignedMalloc not implemented for this platform."
#endif

        if (error == EINVAL) {
            return RESULT_INVAL;
        } else if (error == ENOMEM) {
            return RESULT_NOMEM;
        }

        return RESULT_OK;
    }

    void AlignedFree(void* memptr)
    {
#if defined(__GNUC__)
        free(memptr);
#elif defined(_MSC_VER)
        _aligned_free(memptr);
#else
        #error "dmMemory::AlignedFree not implemented for this platform."
#endif
    }
}
