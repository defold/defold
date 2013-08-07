
#ifndef DM_ARRAY_H
#define DM_ARRAY_H
#include <stdint.h>
#include <assert.h>


/**
 * dmArray class helper functions. (private)
 */
class dmArrayHelper
{
    public:
	static void SetCapacity(uint32_t, uint32_t, uintptr_t*, uintptr_t*, uintptr_t*);
};


/**
 * Array class with basic bound-checking.
 * The contained type must be a value type and conform to memcpy-semantics.
 * Except for SetSize(.) and SetCapacity(.) all operations are O(1).
 */
template <typename T>
class dmArray
{
    enum STATE_FLAGS
    {
        STATE_DEFAULT           = 0x0,
        STATE_USER_ALLOCATED    = 0x1
    };

public:
    /**
     * Creates an empty array.
     */
    dmArray()
    {
        m_Front = 0;
        m_End = 0;
        m_Back = 0;
        m_State = STATE_DEFAULT;
    }

    /**
     * Creates an array with user allocated memory. User allocated arrays can not change capacity.
     * @param user_allocated Pointer to user allocated continous data-block
     * @param size Initial number of valid elements.
     * @param capacity Max capacity
     */
    dmArray(T *user_allocated, uint32_t size, uint32_t capacity)
    {
        assert(user_allocated != 0);
        assert(size  < capacity);
        m_Front = user_allocated;
        m_End = user_allocated + size;
        m_Back = user_allocated + capacity;
        m_State = STATE_USER_ALLOCATED;
    }

    /**
     * Destructor.
     * @note If user allocated, memory is not free'd
     */
    ~dmArray()
    {
        if (!(m_State & STATE_USER_ALLOCATED) && m_Front)
        {
            delete[] (uint8_t*) m_Front;
        }
    }


    /**
     * Return begin iterator
     * @return begin iterator
     */
    T* Begin()
    {
        return m_Front;
    }

    /**
     * Return end iterator
     * @return end iterator
     */
    T* End()
    {
        return m_End;
    }

    /**
     * First element
     * @return Reference to the first element
     */
    T& Front()
    {
        assert( Size() > 0 );
        return m_Front[0];
    }

    /**
     * First element (const)
     * @return Const reference to the first element
     */
    const T& Front() const
    {
        assert( Size() > 0 );
        return m_Front[0];
    }

    /**
     * Last element
     * @return Reference to the last element
     */
    T& Back()
    {
        assert( Size() > 0 );
        return m_End[-1];
    }

    /**
     * Last element (const)
     * @return Const reference to the last element
     */
    const T& Back() const
    {
        assert( Size() > 0 );
        return m_End[-1];
    }

    /**
     * Array size
     * @return Returns size of the array
     */
    uint32_t Size() const
    {
        return (uint32_t)(m_End - m_Front);
    }

    /**
     * Total capacity
     * @return Returns total capacity of the array
     */
    uint32_t Capacity() const
    {
        return (uint32_t)(m_Back - m_Front);
    }

    /**
     * Return true if the array is full.
     * @return Returns true if size of array has reached capacity.
     */
    bool Full() const
    {
        return m_End == m_Back;
    }

    /**
     * Return true if the array is empty.
     * @return Returns true if the array is empty
     */
    bool Empty() const
    {
        return m_End == m_Front;
    }

    /**
     * Returns the amount of space left in the array for use, in elements.
     * @return Returns space left in array, in elements.
     */
    uint32_t Remaining() const
    {
        return m_Back - m_End;
    }

    /**
     * Index operator
     * @return Returns reference to the element at offset i
     */
    T& operator[](uint32_t i)
    {
        assert (  i < Size() );
        return m_Front[i];
    }

    /**
     * Index operator (const)
     * @return Returns const reference to the element at offset i
     */
    const T& operator[](uint32_t i) const
    {
        assert ( i < Size() );
        return m_Front[i];
    }

    /**
     * Set capacity. If less than current size, the array will be truncated.
     * @note Changing the capacity will result in a new memory allocation.
     * @param new_capacity New array capacity
     */
    void SetCapacity(uint32_t new_capacity)
    {
        assert (!(m_State & STATE_USER_ALLOCATED) && "SetCapacity is an illegal operation on user allocated arrays");
		dmArrayHelper::SetCapacity(new_capacity, sizeof(T), (uintptr_t*)&m_Front, (uintptr_t*)&m_Back, (uintptr_t*)&m_End);
    }

    /**
     * Offset capacity. This is equivalent of calling SetCapacity(Capacity() + offset_capacity).
     * @note Offsetting capacity will result in a new memory allocation.
     * @param offset_capacity The amount to adjust capacity of array with.
     */
    void OffsetCapacity(int32_t offset_capacity)
    {
        SetCapacity((uint32_t)((int32_t)Capacity()) + offset_capacity);
    }

    /**
     * Set array size. The new size must be less than capacity
     * @param new_size New array size
     */
    void SetSize(uint32_t new_size)
    {
        assert( new_size <= Capacity() );
        m_End = m_Front + new_size;
    }

    /**
     * Removes the element at element index by replacing with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_index array index of element to remove
     */
    T& EraseSwap(uint32_t element_index)
    {
        assert ( element_index < Size() );
        m_Front[element_index] = *(m_End - 1);
        m_End--;
        assert ( m_End >= m_Front );
        return m_Front[element_index];
    }

    /**
     * Removes the element with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_ref element to remove
     */
    T& EraseSwapRef(T& element_ref)
    {
        assert ( &element_ref >= m_Front && &element_ref < m_End);
        element_ref = *(m_End - 1);
        m_End--;
        assert ( m_End >= m_Front );
        return element_ref;
    }

    /**
     * Adds a new element at the end of the array.
     * @note Push will fail if capacity is exceeded.
     * @param x Element to add
     */
    void Push(const T& x)
    {
        assert ( Capacity() - Size() > 0 );
        *m_End++ = x;
    }

    /**
     * Adds n new elements at the end of the array.
     * @param x Elements
     * @param count Elemen count
     * @note PushArray will fail if capacity is exceeded.
     */
    void PushArray(const T* x, uint32_t count)
    {
        assert ( Capacity() - Size() >= count );
        memcpy(m_End, x, sizeof(T) * count);
        m_End += count;
    }

    /**
     * Removes the last element in the array.
     * @note Pop will fail if size is 0.
     */
    void Pop()
    {
        assert ( Size() > 0 );
        m_End--;
    }

    /**
     * Swaps the content of two arrays.
     * @param rhs Array to swap with.
     */
    void Swap(dmArray<T>& rhs)
    {
        T* tmp_t = rhs.m_Front;
        rhs.m_Front = m_Front;
        m_Front = tmp_t;
        tmp_t = rhs.m_End;
        rhs.m_End = m_End;
        m_End = tmp_t;
        tmp_t = rhs.m_Back;
        rhs.m_Back = m_Back;
        m_Back = tmp_t;
        uint16_t tmp_i = rhs.m_State;
        rhs.m_State = m_State;
        m_State = tmp_i;
    }

private:
    T *m_Front, *m_End;
    T *m_Back;
    uint16_t m_State : 1;

    // Simply to forbid usage of these methods..
    dmArray(const dmArray<T>&) {}
    void operator=(const dmArray<T>&  ) { }
    void operator==(const dmArray<T>& ) { }
};




#endif // DM_ARRAY_H
