#ifndef DM_INDEX_POOL_H
#define DM_INDEX_POOL_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


/**
 * Index pool class.
 * The contained type must be a integer type.
 * All operations are O(1).
 */
template <typename T>
class dmIndexPool
{
    enum STATE_FLAGS
    {
        STATE_DEFAULT           = 0x0,
        STATE_USER_ALLOCATED    = 0x1
    };

public:
    /**
     * Creates an empty index pool.
     */
    dmIndexPool()
    {
        m_Pool = NULL;
        m_State = STATE_DEFAULT;
        m_Capacity = 0;
        m_Size = 0;
    }

    /**
     * Creates an index pool with user allocated memory.
     * @param user_allocated Pointer to user allocated continous data-block
     * @param size Number of valid elements.
     */
    dmIndexPool(T *user_allocated, T size)
    {
        m_Pool = user_allocated;
        m_Capacity = size;
        m_State = STATE_USER_ALLOCATED;
        m_Size = 0;
        for (T i=0; i < size; i++)
            m_Pool[i] = i;
    }

    /**
     * Destructor.
     * @note If user allocated, memory is not free'd
     */
    ~dmIndexPool()
    {
        if(!(m_State & STATE_USER_ALLOCATED))
        {
            if (m_Pool)
                free(m_Pool);
        }
    }

    /**
     * Pool size
     * @return Returns used number of elements of the pool
     */
    T Size() const
    {
        return m_Size;
    }

    /**
     * Total capacity
     * @return Returns total capacity of the index pool
     */
    T Capacity() const
    {
        return m_Capacity;
    }

    /**
     * Remaining valid elements count
     * @return Returns the number of remaining indices valid for use.
     */
    uint32_t Remaining() const
    {
        return m_Capacity - m_Size;
    }

    /**
     * Set index pool capacity.
     * @note New capacity must be larger or equal to current capacity
     * @param capacity Capacity.
     */
    void SetCapacity(T capacity)
    {
        assert(capacity >= m_Capacity);
        T* old_pool = m_Pool;
        uint32_t oldcapacity = m_Capacity;
        m_Pool = (T*) malloc(sizeof(T)*capacity);
        memcpy(m_Pool, old_pool, oldcapacity * sizeof(T));
        m_Capacity = capacity;
        for (T i = oldcapacity; i < capacity; i++)
            m_Pool[i] = i;
        free(old_pool);
    }

    /**
     * Clear the index pool.
     */
    void Clear()
    {
        m_Size = 0;
        for (T i=0; i < m_Capacity; i++)
            m_Pool[i] = i;
    }

    /**
     * Push a node into pool.
     * @note Index must have been retreived using Pop!
     * @param unique index to push back into pool.
     */
    void Push(T node)
    {
        assert(m_Pool);
        assert(m_Size != 0 && m_Size <= m_Capacity);
        m_Pool[--m_Size] = node;
    }

    /**
     * Pop a node from pool.
     * @return Returns a unique index from pool
     */
    T Pop()
    {
        assert(m_Pool);
        assert(m_Size < m_Capacity);
        return m_Pool[m_Size++];
    }

    /**
     * Iterate over all remaining entries in pool
     * @param call_back Call-back called for every remaining entry
     * @param context Context
     */
    template <typename CONTEXT>
    void IterateRemaining(void (*call_back)(CONTEXT *context, T index), CONTEXT* context)
    {
        assert(m_Pool);
        for (size_t i = m_Size; i < m_Capacity; ++i)
        {
            call_back(context, m_Pool[i]);
        }
    }

private:
    T*   m_Pool;
    T    m_Capacity;
    T    m_Size;
    uint16_t m_State : 1;
};

class dmIndexPool8  : public dmIndexPool<uint8_t> {};

class dmIndexPool16 : public dmIndexPool<uint16_t> {};

class dmIndexPool32 : public dmIndexPool<uint32_t> {};

#endif // DM_INDEX_POOL_H
