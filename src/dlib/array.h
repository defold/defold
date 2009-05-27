
#ifndef DLIB_ARRAY_H
#define DLIB_ARRAY_H

#include <stdint.h>
#include <assert.h>

/**
 * Helper functions for Array template class.
 * Do not use!
 */
class ArrayHelper
{
public:
    static void       SetCapacity(void **, uint32_t, uint32_t);
    static uint8_t*   Alloc(uint32_t);
    static void       Free(uint8_t *);
    static int32_t    Check(void **, uint32_t);
};

#ifdef DEBUG
//#define ARRAY_BUFFER_OVERRUN_CHECK
#endif
#ifdef ARRAY_BUFFER_OVERRUN_CHECK
#define ARRAY_BUFFER_OVERRUN_CHECK assert(ArrayHelper::Check((void **) this, this->m_UserAllocated));
#else
#define ARRAY_BUFFER_OVERRUN_CHECK
#endif

/**
 * Light-weight array class with bound checking.
 * The contained type must be a value type and conform to memcpy-semantics.
 * Except for SetSize(.) and SetCapacity(.) all operations are O(1).
 * Note: PushBack is only valid if capacity - size > 0 due to O(1)-complexity.
 */
template <typename T>
class Array
{
public:
    /**
     * Creates an empty array.
     */
    Array()
    {
        m_First = 0;
        m_End = 0;
        m_Last = 0;
        m_UserAllocated = 0;
    }

    /**
     * Creates an array with user allocated memory. User allocated arrays are not resizable.
     * @param user_allocated Pointer to user allocated data
     * @param size Size or number of valid elements.
     * @param capacity Maximum capacity
     */
    Array(T *user_allocated, uint32_t size, uint32_t capacity)
    {
        assert(user_allocated != 0);
        assert(size  < capacity);
        m_First = user_allocated;
        m_End = user_allocated + size;
        m_Last = user_allocated + capacity;
        m_UserAllocated = 1;
    }

    /**
     * Destructor. 
     * @note Does not free user allocated memory!
     */
    ~Array()
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        if (m_UserAllocated == 0 && m_First)
        {
            ArrayHelper::Free((uint8_t*) m_First);
        }
    }

    /**
     * First element
     * @return A reference to the first element
     */
    T& Front()
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert( Size() > 0 );
        return m_First[0];
    }

    /**
     * First element (const)
     * @return A const reference to the first element
     */
    const T& Front() const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert( Size() > 0 );
        return m_First[0];
    }

    /**
     * Last element
     * @return A reference to the last element
     */
    T& Back()
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert( Size() > 0 );
        return m_End[-1];
    }

    /**
     * Last element (const)
     * @return A const reference to the last element
     */
    const T& Back() const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert( Size() > 0 );
        return m_End[-1];
    }

    /**
     * Array size
     * @return The size of the array
     */
    uint32_t Size() const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        return m_End - m_First;
    }

    /**
     * Total capacity
     * @return Returns the total capacity
     */
    uint32_t Capacity() const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        return m_Last - m_First;
    }

    /**
     * Return true if the array is empty, ie Size == 0.
     * @return true if the array is empty
     */
    bool Empty() const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        return m_End == m_First;
    }

    /**
     * Index operator
     * @return Returns a reference to the element at offset i
     */
    T& operator[](uint32_t i)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert (  i < Size() );
        return m_First[i];
    }

    /**
     * Index operator (const)
     * @return Returns a const reference to the element at offset i
     */
    const T& operator[](uint32_t i) const
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert ( i >= 0 && i < Size() );
        return m_First[i];
    }

    /**
     * Set capacity. If less than current size, the array is truncated.
     * Changing the capacity will result in a new memory allocation (avoid if not absolutely nescessary)!
     * @param new_capacity New array capacity
     */
    void SetCapacity(uint32_t new_capacity)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert (m_UserAllocated == 0 && "SetCapacity is an illegal operation on user allocated arrays");
        void **this_ = (void **) this;
        ArrayHelper::SetCapacity(this_, sizeof(T), new_capacity);
    }

    /**
     * Increase capacity. This is equivalent of calling SetCapacity(Capacity() + grow_capacity).
     * Increasing capacity will result in a new memory allocation (avoid if not absolutely nescessary)!
     * @param grow_capacity the amount to increase capacity of array with.
     */
    void IncreaseCapacity(uint32_t grow_capacity)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert (m_UserAllocated == 0 && "IncreaseCapacity is an illegal operation on user allocated arrays");
        void **this_ = (void **) this;
        ArrayHelper::SetCapacity(this_, sizeof(T), Capacity() + grow_capacity);
    }

    /**
     * Decreases capacity. This is equivalent of calling SetCapacity(Capacity() - decrease_capacity)
     * Decreasing capacity will result in a new memory allocation (avoid if not absolutely nescessary)!
     * @param decrease_capacity the amount to decrease capacity of array with.
     */
    void DecreaseCapacity(uint32_t decrease_capacity)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert (m_UserAllocated == 0 && "DecreaseCapacity is an illegal operation on user allocated arrays");
        assert (Capacity() >= decrease_capacity)
        void **this_ = (void **) this;
        ArrayHelper::SetCapacity(this_, sizeof(T), Capacity() - decrease_capacity);
    }

    /**
     * Set array size. New size must be less than capacity
     * @param new_size New array size
     */
    void SetSize(uint32_t new_size)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert( new_size <= Capacity() );
        m_End = m_First + new_size;
    }

    /**
     * Removes the element at element index by replacing with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_index array index of element to remove
     */
    T& EraseSwap(uint32_t element_index)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert ( element_index < Size() );
        m_First[element_index] = *(m_End - 1);
        m_End--;
        assert ( m_End >= m_First );
        return m_First[element_index];
    }

    /**
     * Removes the element with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_ref element to remove
     */
    T& EraseSwapRef(T& element_ref)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert ( element_ref >= m_First && element_ref < m_End);
        element_ref = *(m_End - 1);
        m_End--;
        assert ( m_End >= m_First );
        return element_ref;
    }

    /**
     * Adds a new element at the end.
     * @note Capacity() - Size() must be greater than 0.
     * @param x Element to add
     */
    void PushBack(const T& x)
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert ( Capacity() - Size() > 0 );
        *m_End++ = x;
    }

    /**
     * Removes the last element in the array.
     * @note Size() must be greater than 0.
     */
    void PopBack()
    {
        ARRAY_BUFFER_OVERRUN_CHECK
        assert ( Size() > 0 );
        m_End--;
    }

    /**
     * Return true if the array is full, ie Size == Capacity.
     * @return true if size of array has reached capacity.
     */
    bool Full() const
    {
        return m_End == m_Last;
    }

    /**
     * Returns the amount of space left in the array for use, in elements.
     * @return nr of space left in array, in elements.
     */
    uint32_t Remaining() const
    {
        return m_Last - m_End;
    }

private:
    T *m_First;
    T *m_End;
    T *m_Last;
    uint16_t m_UserAllocated : 1;

    Array(const Array<T>&) {}
    void operator=(const Array<T>&  ) { }
    void operator==(const Array<T>& ) { }
};




#endif // DLIB_ARRAY_H
