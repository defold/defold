#ifndef DM_CIRCULAR_ARRAY_H
#define DM_CIRCULAR_ARRAY_H

#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "math.h"

/**
 * Array class with basic bound-checking.
 * The contained type must be a value type and conform to memcpy-semantics.
 * Except for SetSize(.) and SetCapacity(.) all operations are O(1).
 */
template <typename T>
class dmCircularArray
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
    dmCircularArray()
    {
        m_Buffer = 0x0;
        m_Capacity = 0;
        m_Front = 0x0;
        m_Back = 0x0;
        m_State = STATE_DEFAULT;
    }

    /**
     * Creates an array with user allocated memory. User allocated arrays can not change capacity.
     * @param user_allocated Pointer to user allocated continous data-block
     * @param size Initial number of valid elements.
     * @param capacity Max capacity
     */
    dmCircularArray(T *user_allocated, uint32_t size, uint32_t capacity)
    {
        assert(user_allocated != 0);
        assert(size  < capacity);
        m_Buffer = user_allocated;
        m_Capacity = capacity;
        m_Front = user_allocated;
        m_Back = &user_allocated[size - 1];
        m_State = STATE_USER_ALLOCATED;
    }

    /**
     * Destructor.
     * @note If user allocated, memory is not free'd
     */
    ~dmCircularArray()
    {
        if (!(m_State & STATE_USER_ALLOCATED) && m_Buffer)
        {
            delete[] (uint8_t*) m_Buffer;
        }
    }

    /**
     * First element
     * @return Reference to the first element
     */
    T& Front()
    {
        return *m_Front;
    }

    /**
     * First element (const)
     * @return Const reference to the first element
     */
    const T& Front() const
    {
        return *m_Front;
    }

    /**
     * First element
     * @return Reference to the top element (same as last Push() or will be affected by next Pop()).
     */
    T& Top()
    {
        return *m_Back;
    }

    /**
     * First element
     * @return Reference to the top element (same as last Push() or will be affected by next Pop()).
     */
    const T& Top() const
    {
        return *m_Back;
    }

    /**
     * Last element
     * @return Reference to the last element
     */
    T& Back()
    {
        assert(Size() > 0);
        return *m_Back;
    }

    /**
     * Last element (const)
     * @return Const reference to the last element
     */
    const T& Back() const
    {
        assert(Size() > 0);
        return *m_Back;
    }

    /**
     * Array size
     * @return Returns size of the array
     */
    uint32_t Size() const
    {
        if (m_Front == 0x0)
            return 0;
        else if (m_Front > m_Back)
            return (uint32_t)(m_Capacity - (m_Front - m_Back)) + 1;
        else
            return (uint32_t)(m_Back - m_Front) + 1;
    }

    /**
     * Total capacity
     * @return Returns total capacity of the array
     */
    uint32_t Capacity() const
    {
        return m_Capacity;
    }

    /**
     * Return true if the array is full.
     * @return Returns true if size of array has reached capacity.
     */
    bool Full() const
    {
        return Capacity() == Size();
    }

    /**
     * Return true if the array is empty.
     * @return Returns true if the array is empty
     */
    bool Empty() const
    {
        return m_Front == 0x0;
    }

    /**
     * Returns the amount of space left in the array for use, in elements.
     * @return Returns space left in array, in elements.
     */
    uint32_t Remaining() const
    {
        return m_Capacity - Size();
    }

    /**
     * Index operator
     * @return Returns reference to the element at offset i
     */
    T& operator[](uint32_t i)
    {
        assert(i < Size());
        return m_Buffer[((uint32_t)(m_Front - m_Buffer) + i) % m_Capacity];
    }

    /**
     * Index operator (const)
     * @return Returns const reference to the element at offset i
     */
    const T& operator[](uint32_t i) const
    {
        assert(i >= 0 && i < Size());
        return m_Buffer[((uint32_t)(m_Front - m_Buffer) + i) % m_Capacity];
    }

    /**
     * Set capacity. If less than current size, the array will be truncated.
     * @note Changing the capacity will result in a new memory allocation.
     * @param new_capacity New array capacity
     */
    void SetCapacity(uint32_t new_capacity)
    {
        assert (!(m_State & STATE_USER_ALLOCATED) && "SetCapacity is an illegal operation on user allocated arrays");
        if(new_capacity == m_Capacity)
            return;

        size_t type_size = sizeof(T);
        uint8_t *new_block;
        uint32_t new_size = 0;
        if(new_capacity)
        {
            new_block = new uint8_t[new_capacity * type_size];
            assert(new_block != 0 && "SetCapacity failed allocating memory");

            if (m_Front < m_Back)
            {
                new_size = dmMath::Min(new_capacity, (uint32_t)(m_Back - m_Front) + 1);
                memcpy(new_block, (void*)m_Buffer, type_size * new_size);
            }
            else if (m_Back < m_Front)
            {
                new_size = dmMath::Min(new_capacity, m_Capacity - (uint32_t)(m_Front - m_Back) + 1);
                memcpy(new_block, (void*)m_Front, type_size * (uint32_t)(m_Buffer + m_Capacity - m_Front));
                memcpy(&new_block[type_size * (m_Capacity - (uint32_t)(m_Front - m_Buffer))], (void*)m_Buffer, type_size * (uint32_t)(m_Back - m_Buffer));
            }
        }
        else
        {
            new_block = 0;
        }

        if (m_Capacity > 0)
        {
            delete[] (uint8_t*)m_Buffer;
        }

        m_Buffer = (T*)new_block;
        m_Capacity = new_capacity;
        if (new_size > 0)
        {
            m_Front = m_Buffer;
            m_Back = (T*)(new_block + type_size * (new_size - 1));
        }
        else
        {
            m_Front = 0x0;
            m_Back = 0x0;
        }
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
        if (new_size == 0)
        {
            m_Front = 0x0;
            m_Back = 0x0;
        }
        else
        {
            m_Front = m_Buffer;
            m_Back = &m_Buffer[((uint32_t)(m_Front - m_Buffer) + new_size - 1) % m_Capacity];
        }
    }

    /**
     * Removes the element at element index by replacing with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_index array index of element to remove
     */
    T& EraseSwap(uint32_t element_index)
    {
        assert( element_index < Size() );
        if (m_Front == m_Back)
        {
            m_Front = 0x0;
            m_Back = 0x0;
            return m_Buffer[0];
        }
        else
        {
            m_Front[((uint32_t)(m_Front - m_Buffer) + element_index) % m_Capacity] = *m_Back;
            m_Back = &m_Buffer[((uint32_t)(m_Back - m_Buffer) + m_Capacity - 1) % m_Capacity];
            return m_Front[element_index];
        }
    }

    /**
     * Removes the element with the last element. O(1) operation.
     * @note Does not preserve order!
     * @param element_ref element to remove
     */
    T& EraseSwapRef(T& element_ref)
    {
        assert(&element_ref >= m_Buffer && (uint32_t)(&element_ref - m_Buffer) < m_Capacity);
        element_ref = *m_Back;
        m_Back = &m_Buffer[((uint32_t)(m_Back - m_Buffer) + m_Capacity - 1) % m_Capacity];
        return element_ref;
    }

    /**
     * Adds a new element at the end of the array.
     * @note Push will overwrite the back element if the array is full.
     * @param x Element to add
     */
    void Push(const T& x)
    {
        if (m_Front == 0x0)
        {
            *m_Buffer = x;
            m_Front = m_Buffer;
            m_Back = m_Buffer;
        }
        else
        {
            m_Back = &m_Buffer[((uint32_t)(m_Back - m_Buffer) + 1) % m_Capacity];
            *m_Back = x;
            if (m_Front == m_Back)
                m_Front = &m_Buffer[((uint32_t)(m_Front - m_Buffer) + 1) % m_Capacity];
        }
    }

    /**
     * Removes the back element in the array.
     * @note Pop will fail if size is 0.
     */
    void Pop()
    {
        assert(Size() > 0);
        if (m_Front == m_Back)
        {
            m_Front = 0x0;
            m_Back = 0x0;
        }
        else
        {
            m_Back = &m_Buffer[((uint32_t)(m_Back - m_Buffer) + m_Capacity - 1) % m_Capacity];
        }
    }

private:
    T* m_Buffer;
    T* m_Front;
    T* m_Back;
    uint16_t m_Capacity;
    uint16_t m_State : 1;

    // Simply to forbid usage of these methods..
    dmCircularArray(const dmCircularArray<T>&) {}
    void operator=(const dmCircularArray<T>&  ) { }
    void operator==(const dmCircularArray<T>& ) { }
};

#endif // DM_CIRCULAR_ARRAY_H
