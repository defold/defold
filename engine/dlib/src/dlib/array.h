#ifndef DM_ARRAY_H
#define DM_ARRAY_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <dlib/array.h>

extern "C"
{
#include <sol/runtime.h>
}

/**
 * Utility functions
 */
namespace dmArrayUtil
{
    void SetCapacity(uint32_t, uint32_t, uintptr_t*, uintptr_t*, uintptr_t*, bool);
};

/** Templatized array with bounds checking.
 *
 * The backing storage is either auto-allocated (dynamically allocated) or user-allocated (supplied by user).
 * With exception of changing the size and capacity, all operations are guaranteed to be O(1).
 *
 * @tparam T Contained type, must obey memcpy semantics
 */
template <typename T>
class dmArray
{
public:
    /** empty auto-allocated array
     */
    dmArray(bool use_sol_allocator = false);

    /** user-allocated array with initial size and capacity
     *
     * @param user_array User-allocated array to be used as storage
     * @param size Initial size
     * @param capacity Initial capacity
     */
    dmArray(T* user_array, uint32_t size, uint32_t capacity);

    /** destructor
     * Only frees memory when auto-allocated.
     */
    ~dmArray();

    /** pointer to the start of the backing storage
     * @return pointer to start of memory
     */
    T* Begin();

    /** pointer to the end of the backing storage
     * The end is essentially outside of the used storage.
     * @return pointer to end of memory
     */
    T* End();

    /** first element of the array
     * @return reference to the first element
     */
    T& Front();

    /** first element of the array (const)
     * @return const-reference to the first element
     */
    const T& Front() const;

    /** last element of the array
     * @return reference to the last element
     */
    T& Back();

    /** last element of the array (const)
     * @return const-reference to the last element
     */
    const T& Back() const;

    /** size of the array
     * @return array size
     */
    uint32_t Size() const;

    /** capacity of the array
     * Capacity is currently allocated storage.
     * @return array capacity
     */
    uint32_t Capacity() const;

    /** check if the array is full
     * The array is full when the size is equal to the capacity.
     * @return true if the array is full
     */
    bool Full() const;

    /** check if the array is empty
     * The array is empty when the size is zero.
     * @return true if the array is empty
     */
    bool Empty() const;

    /** amount of elements that can be currently stored
     * @return amount of elements
     */
    uint32_t Remaining() const;

    /** retrieve an element by index
     * @return reference to the element at the specified index
     */
    T& operator[](uint32_t i);

    /** retrieve an element by index (const)
     * @return const-reference to the element at the specified index
     */
    const T& operator[](uint32_t i) const;

    /** set the capacity of the array
     * If the size is less than the capacity, the array is truncated.
     * If it is larger, the array is extended.
     * @note Only allowed for auto-allocated arrays and will result in a new dynamic allocation followed by memcpy of the elements.
     * @param capacity capacity of the array
     */
    void SetCapacity(uint32_t capacity);

    /** relative change of capacity
     * Equivalent to SetCapacity(Capacity() + offset).
     * @note Only allowed for auto-allocated arrays and will result in a new dynamic allocation followed by memcpy of the elements.
     * @param offset relative amount of elements to change the capacity
     */
    void OffsetCapacity(int32_t offset);

    /** set size of the array
     * @param size size of the array, must be less or equal to the capacity
     */
    void SetSize(uint32_t size);

    /** remove the element at the specified index
     * The removed element is replaced by the element at the end (if any), thus altering the order.
     * @note Might alter the order of the array.
     * @param index index of the element to remove
     */
    T& EraseSwap(uint32_t index);

    /** remove the element by reference
     * The removed element is replaced by the element at the end (if any), thus altering the order.
     * @note Might alter the order of the array.
     * @param element_ref reference of the element to remove
     */
    T& EraseSwapRef(T& element);

    /** add an element to the end of the array
     * @note Only allowed when the capacity is larger than size.
     * @param element element to add
     */
    void Push(const T& element);

    /** add an array of elements to the end of the array
     * @param array array of elements to add
     * @param count amount of elements in the array
     * @note Only allowed when the capacity is larger than size + count.
     */
    void PushArray(const T* array, uint32_t count);

    /** remove the last element of the array
     * @note Only allowed when the size is larger than zero.
     */
    void Pop();

    /** swap the content of two arrays
     * @param rhs array to swap content with
     */
    void Swap(dmArray<T>& rhs);

private:
    T *m_Front, *m_End;
    T *m_Back;

    uint16_t m_UserAllocated   : 1;
    uint16_t m_UseSolAllocator : 1;

    // Restrict copy-construction etc.
    dmArray(const dmArray<T>&) {}
    void operator =(const dmArray<T>&) {}
    void operator ==(const dmArray<T>&) {}
};

template <typename T>
dmArray<T>::dmArray(bool use_sol_allocator)
{
    memset(this, 0, sizeof(*this));
    m_UseSolAllocator = use_sol_allocator;
}

template <typename T>
dmArray<T>::dmArray(T *user_array, uint32_t size, uint32_t capacity)
{
    assert(user_array != 0);
    assert(size  < capacity);
    m_Front = user_array;
    m_End = user_array + size;
    m_Back = user_array + capacity;
    m_UserAllocated = 1;
}

template <typename T>
dmArray<T>::~dmArray()
{
    if (!m_UserAllocated && m_Front)
    {
        if (m_UseSolAllocator)
        {
            runtime_unpin((void *)m_Front);
        }
        else
        {
            free((void *)m_Front);
        }
    }
}

template <typename T>
T* dmArray<T>::Begin()
{
    return m_Front;
}

template <typename T>
T* dmArray<T>::End()
{
    return m_End;
}

template <typename T>
T& dmArray<T>::Front()
{
    assert(Size() > 0);
    return m_Front[0];
}

template <typename T>
const T& dmArray<T>::Front() const
{
    assert(Size() > 0);
    return m_Front[0];
}

template <typename T>
T& dmArray<T>::Back()
{
    assert(Size() > 0);
    return m_End[-1];
}

template <typename T>
const T& dmArray<T>::Back() const
{
    assert(Size() > 0);
    return m_End[-1];
}

template <typename T>
uint32_t dmArray<T>::Size() const
{
    return (uint32_t)(m_End - m_Front);
}

template <typename T>
uint32_t dmArray<T>::Capacity() const
{
    return (uint32_t)(m_Back - m_Front);
}

template <typename T>
bool dmArray<T>::Full() const
{
    return m_End == m_Back;
}

template <typename T>
bool dmArray<T>::Empty() const
{
    return m_End == m_Front;
}

template <typename T>
uint32_t dmArray<T>::Remaining() const
{
    return m_Back - m_End;
}

template <typename T>
T& dmArray<T>::operator[](uint32_t i)
{
    assert(i < Size());
    return m_Front[i];
}

template <typename T>
const T& dmArray<T>::operator[](uint32_t i) const
{
    assert(i < Size());
    return m_Front[i];
}

template <typename T>
void dmArray<T>::SetCapacity(uint32_t capacity)
{
    assert(!m_UserAllocated && "SetCapacity is not allowed for user-allocated arrays");
    dmArrayUtil::SetCapacity(capacity, sizeof(T), (uintptr_t*)&m_Front, (uintptr_t*)&m_Back, (uintptr_t*)&m_End, m_UseSolAllocator);
}

template <typename T>
void dmArray<T>::OffsetCapacity(int32_t offset)
{
    SetCapacity((uint32_t)((int32_t)Capacity()) + offset);
}

template <typename T>
void dmArray<T>::SetSize(uint32_t size)
{
    assert(size <= Capacity());
    m_End = m_Front + size;
}

template <typename T>
T& dmArray<T>::EraseSwap(uint32_t index)
{
    assert(index < Size());
    m_Front[index] = *(m_End - 1);
    m_End--;
    assert(m_End >= m_Front);
    return m_Front[index];
}

template <typename T>
T& dmArray<T>::EraseSwapRef(T& element)
{
    assert(&element >= m_Front && &element < m_End);
    element = *(m_End - 1);
    m_End--;
    assert(m_End >= m_Front);
    return element;
}

template <typename T>
void dmArray<T>::Push(const T& element)
{
    assert(Capacity() - Size() > 0);
    *m_End++ = element;
}

template <typename T>
void dmArray<T>::PushArray(const T* array, uint32_t count)
{
    assert(Capacity() - Size() >= count);
    memcpy(m_End, array, sizeof(T) * count);
    m_End += count;
}

template <typename T>
void dmArray<T>::Pop()
{
    assert(Size() > 0);
    m_End--;
}

#define SWAP(type, lhs, rhs)\
    {\
        type tmp = rhs;\
        rhs = lhs;\
        lhs = tmp;\
    }

template <typename T>
void dmArray<T>::Swap(dmArray<T>& rhs)
{
    SWAP(T*, m_Front, rhs.m_Front);
    SWAP(T*, m_End, rhs.m_End);
    SWAP(T*, m_Back, rhs.m_Back);
    SWAP(uint16_t, m_UserAllocated, rhs.m_UserAllocated);
}

#undef SWAP

#endif // DM_ARRAY_H
