#include "align.h"
#include <stdlib.h>
#include <errno.h>

namespace dmAlign
{

    Result Malloc(void **memptr, unsigned alignment, unsigned size)
    {
        int error = 0;
#if defined(__GNUC__)
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

    void Free(void* memptr)
    {
#if defined(__GNUC__)
        free(memptr);
#elif defined(_MSC_VER)
        _aligned_free(memptr);
#endif
    }

}
