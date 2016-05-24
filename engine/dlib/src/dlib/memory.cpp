#include "memory.h"
#include <stdlib.h>
#include <errno.h>
#if defined(__ANDROID__)
#include <malloc.h>
#endif

namespace dmMemory
{

    Result AlignedMalloc(void **memptr, unsigned alignment, unsigned size)
    {
        int error = 0;
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
#endif
    }

}
