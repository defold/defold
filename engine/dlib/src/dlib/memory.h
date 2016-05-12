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
     * @param memptr Pointer to a void* where the
     * @param alignment The alignment value, which must be an integer power of 2.
     * @param size Size of the requested memory allocation.
     */
    Result AlignedMalloc(void **memptr, unsigned int alignment,  unsigned int size);

    /**
     * Frees a block of memory that was allocated with dmMemory::AlignedMalloc
     * @param memptr A pointer to the memory block that was returned by dmMemory::AlignedMalloc
     */
    void AlignedFree(void* memptr);

}

#endif // DM_MEMORY_H
