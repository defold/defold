#ifndef DM_ALIGN_H
#define DM_ALIGN_H

/**
 * Alignment macro. For compiler compatibility use the macro
 * between the type and the variable, e.g. int DM_ALIGNED(16) x;
 * @param a number of bytes to align to
 */
#define DM_ALIGNED(a)
#undef DM_ALIGNED

/**
 * Align a value to a boundary.
 * @param x value to align
 * @param a alignment boundary
 */
#define DM_ALIGN(x, a) (((uintptr_t) (x) + (a-1)) & ~(a-1))

#if defined(__GNUC__)
#define DM_ALIGNED(a) __attribute__ ((aligned (a)))
#elif defined(_MSC_VER)
#define DM_ALIGNED(a) __declspec(align(a))
#else
#error "Unsupported compiler"
#endif


namespace dmAlign
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
     * @param memptr Pointer to a void* where the
     * @param alignment The alignment value, which must be an integer power of 2.
     * @param size Size of the requested memory allocation.
     */
    Result Malloc(void **memptr, unsigned int alignment,  unsigned int size);

    /**
     * Frees a block of memory that was allocated with dmAlign::Malloc
     * @param memptr A pointer to the memory block that was returned by dmAlign::Malloc
     */
    void Free(void* memptr);

}

#endif // DM_ALIGN_H
