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
     * @note Internally dmHashString64 is used. The identical property is valid iff two distinct strings result in two distinct hash values.
     * @param pool String pool
     * @param string String to add to pool
     * @return Pointer to string added
     */
    const char* Add(HPool pool, const char* string);
}

#endif // DM_STRINGPOOL_H
