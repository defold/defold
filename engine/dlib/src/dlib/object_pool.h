
#ifndef DM_OBJECT_POOL_H
#define DM_OBJECT_POOL_H

#include "array.h"

/**
 * Object pool data-structure with the following properties
 * - Mapping from logical index to physical index
 * - Logical index does not changes
 * - Allocated objects are contiguously laid out in memory
 *   Loop of m_Objects [0..Size()-1] times to iterate all objects
 * - Internal physical order is not preserved and a direct consequence of the
 *   contiguous property
 */
template <typename T>
struct dmObjectPool
{
    /// All objects [0..Size()]
    dmArray<T>        m_Objects;

    /// Internal
    struct Entry
    {
        uint32_t m_Physical;
        uint32_t m_Next;
    };

    /// Internal
    dmArray<Entry> m_Entries;
    /// Internal
    uint32_t       m_FirstFree;
    /// Internal
    dmArray<uint32_t> m_ToLogical;

    dmObjectPool()
    {
        m_FirstFree = 0xffffffff;
    }

    /**
     * Set capacity. New capacity must be >= old_capacity
     * @param capacity
     */
    void SetCapacity(uint32_t capacity)
    {
        assert(capacity >= m_Objects.Capacity());

        m_Entries.SetCapacity(capacity);
        m_Objects.SetCapacity(capacity);

        m_ToLogical.SetCapacity(capacity);
        m_ToLogical.SetSize(capacity);
    }

    /**
     * Allocate a new object
     * @return logical index
     */
    uint32_t Alloc()
    {
        uint32_t size = m_Objects.Size();
        Entry* e = 0;
        if (m_FirstFree != 0xffffffff) {
            e = &m_Entries[m_FirstFree];
            m_FirstFree = e->m_Next;
        } else {
            m_Entries.SetSize(size + 1);
            e = &m_Entries[size];
        }
        e->m_Next = 0xffffffff;
        e->m_Physical = size;
        m_Objects.SetSize(size + 1);

        uint32_t index = e - m_Entries.Begin();
        m_ToLogical[size] = index;

        return index;
    }

    /**
     * Total size
     * @return
     */
    uint32_t Size()
    {
        return m_Objects.Size();
    }

    /**
     * Is full?
     * @return
     */
    bool Full()
    {
        return m_Objects.Remaining() == 0;
    }

    /**
     *
     * @return
     */
    uint32_t Capacity()
    {
        return m_Objects.Capacity();
    }

    /**
     * Free object
     * @param id logical index
     * @param clear clear memory?
     */
    void Free(uint32_t id, bool clear)
    {
        uint32_t size = m_Objects.Size();
        Entry* e = &m_Entries[id];
        uint32_t last = m_ToLogical[size - 1];
        assert(e->m_Physical < size);

        T* o = &m_Objects[e->m_Physical];
        if (clear) {
            memset(o, 0, sizeof(T));
        }

        // Remap logical/physical
        m_Entries[last].m_Physical = e->m_Physical;
        m_ToLogical[e->m_Physical] = last;

        // Remove
        m_Objects.EraseSwap(e->m_Physical);

        // Put in free list
        e->m_Next = m_FirstFree;
        m_FirstFree = e - m_Entries.Begin();
    }

    /**
     * Get object from logical index
     * @param id
     * @return
     */
    T& Get(uint32_t index)
    {
        Entry* e = &m_Entries[index];
        T& o = m_Objects[e->m_Physical];
        return o;
    }

    /**
     * Set object from logical index
     * @param index
     * @param object
     */
    void Set(uint32_t index, T& object)
    {
        Entry* e = &m_Entries[index];
        m_Objects[e->m_Physical] = object;
    }

};

#endif /* DM_OBJECT_POOL_H */
