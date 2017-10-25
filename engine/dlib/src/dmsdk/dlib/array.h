#ifndef DMSDK_ARRAY_H
#define DMSDK_ARRAY_H

#include <assert.h>
#include <stdint.h>
#include <string.h>

/*# SDK Array API documentation
 * [file:<dmsdk/dlib/array.h>]
 *
 * Templatized array with bounds checking.<br/>template `<typename T>` class dmArray<br/>
 *
 * The backing storage is either auto-allocated (dynamically allocated) or user-allocated (supplied by user).
 * With exception of changing the size and capacity, all operations are guaranteed to be O(1).
 *
 * ```cpp
 * dmArray<int> a;
 * a.Push(1);
 * ```
 *
 * @document
 * @name Array
 * @namespace dmArray
 */

/**
 * Utility functions
 */
namespace dmArrayUtil
{
    /** Set capacity of an array, given front, back and end array pointers
     * @param capacity new capacity
     * @param type_size size of array element
     * @param first pointer to a pointer containing the first array element
     * @param last pointer to a pointer containing the last array element
     * @param end pointer to a pointer containing the end array element
     */
    void SetCapacity(uint32_t capacity, uint32_t type_size, uintptr_t* first, uintptr_t* last, uintptr_t* end);
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
    /*# empty auto-allocated array
     *
     * @name dmArray::dmArray
     * @examples
     *
     * ```cpp
     * dmArray<int>* a = new dmArray<int>();
     * ```
     */
    dmArray();

    /*# user-allocated array
     *
     * user-allocated array with initial size and capacity
     *
     * @name dmArray::dmArray
     * @param user_array [type:T*] User-allocated array to be used as storage.
     * @param size [type:uint32_t] Initial size
     * @param capacity [type:uint32_t] Initial capacity
     */
    dmArray(T* user_array, uint32_t size, uint32_t capacity);

    /*# array destructor
     *
     * Only frees memory when auto-allocated.
     *
     * @name dmArray::~dmArray
     */
    ~dmArray();

    /*# array begin
     *
     * Pointer to the start of the backing storage
     *
     * @name dmArray::Begin
     * @return pointer [type:T*] pointer to start of memory
     */
    T* Begin();

    /*# array end
     *
     * Pointer to the end of the backing storage
     * The end is essentially outside of the used storage.
     *
     * @name dmArray::End
     * @return pointer [type:T*] pointer to end of memory
     */
    T* End();

    /*# array front
     *
     * First element of the array
     *
     * @name dmArray::Front
     * @return reference [type:T&] reference to the first element
     */
    T& Front();

    /*# array front (const)
     *
     * First element of the array (const)
     *
     * @name dmArray::Front
     * @return reference [type:const T&] const-reference to the first element
     */
    const T& Front() const;

    /*# array back
     *
     * Last element of the array
     *
     * @name dmArray::Back
     * @return reference [type:T&] reference to the last element
     */
    T& Back();

    /*# array back (const)
     *
     * Last element of the array (const)
     *
     * @name dmArray::Back
     * @return reference [type:const T&] const-reference to the last element
     */
    const T& Back() const;

    /*# size of array
     *
     * Size of the array in elements
     *
     * @name dmArray::Size
     * @return number [type:uint32_t] array size
     */
    uint32_t Size() const;

    /*# capacity of array
     *
     * Capacity is currently allocated storage.
     *
     * @name dmArray::Capacity
     * @return number [type:uint32_t] array capacity
     */
    uint32_t Capacity() const;

    /*# array full
     *
     * Check if the array is full.
     * The array is full when the size is equal to the capacity.
     *
     * @name dmArray::Full
     * @return boolean [type:boolean] true if the array is full
     */
    bool Full() const;

    /*# array empty
     *
     * Check if the array is empty.
     * The array is empty when the size is zero.
     *
     * @name dmArray::Empty
     * @return boolean [type:boolean] true if the array is empty
     */
    bool Empty() const;

    /*# remaining size of array
     *
     * Amount of additional elements that can be stored
     *
     * @name dmArray::Remaining
     * @return number [type:uint32_t] amount of additional elements that can be stored
     */
    uint32_t Remaining() const;

    /*# array operator[]
     *
     * Retrieve an element by index
     *
     * @name dmArray::operator[]
     * @param index [type:uint32_t] array index
     * @return reference [type:T&] reference to the element at the specified index
     */
    T& operator[](uint32_t i);

    /*# array operator[] (const)
     *
     * Retrieve an element by index (const)
     *
     * @name dmArray::operator[]
     * @param index [type:uint32_t] array index
     * @return reference [type:const T&] const-reference to the element at the specified index
     */
    const T& operator[](uint32_t i) const;

    /*# array set capacity
     *
     * Set the capacity of the array.
     * If the size is less than the capacity, the array is truncated.
     * If it is larger, the array is extended.
     * Only allowed for auto-allocated arrays and will result in a new dynamic allocation followed by memcpy of the elements.
     *
     * @name dmArray::SetCapacity
     * @param capacity [type:uint32_t] capacity of the array
     */
    void SetCapacity(uint32_t capacity);

    /*# array offset capacity
     *
     * Relative change of capacity
     * Equivalent to SetCapacity(Capacity() + offset).
     * Only allowed for auto-allocated arrays and will result in a new dynamic allocation followed by memcpy of the elements.
     *
     * @name dmArray::OffsetCapacity
     * @param offset [type:uint32_t] relative amount of elements to change the capacity
     */
    void OffsetCapacity(int32_t offset);

    /*# array set size
     *
     * Set size of the array
     *
     * @name dmArray::SetSize
     * @param size [type:uint32_t] size of the array, must be less or equal to the capacity
     */
    void SetSize(uint32_t size);

    /*# array eraseswap
     *
     * Remove the element at the specified index.
     * The removed element is replaced by the element at the end (if any), thus potentially altering the order.
     * While operation changes the array size, it is guaranteed to be O(1).
     *
     * @name dmArray::EraseSwap
     * @param index [type:uint32_t] index of the element to remove
     * @return reference [type:T&] reference to the new element at index
     */
    T& EraseSwap(uint32_t index);

    /*# array reference eraseswap
     *
     * Remove the element by reference
     * The removed element is replaced by the element at the end (if any), thus potentially altering the order.
     * While operation changes the array size, it is guaranteed to be O(1).
     *
     * @name dmArray::EraseSwapRef
     * @param element [type:T&] reference to the element to remove.
     * @return reference [type:T&] reference to the new referenced element
     */
    T& EraseSwapRef(T& element);

    /*# array push
     *
     * Add an element to the end of the array
     * Only allowed when the capacity is larger than size.
     *
     * @name dmArray::Push
     * @param element [type:const T&] element element to add
     */
    void Push(const T& element);

    /*# array push array
     *
     * Add an array of elements to the end of the array
     * Only allowed when the capacity is larger than size + count
     *
     * @name dmArray::PushArray
     * @param array [type:const T&] array of elements to add
     * @param count [type:uint32_t] amount of elements in the array
     */
    void PushArray(const T* array, uint32_t count);

    /*# array pop
     *
     * Remove the last element of the array
     * Only allowed when the size is larger than zero.
     *
     * @name dmArray::Pop
     */
    void Pop();


    /*# array swap
     *
     * Swap the content of two arrays
     *
     * @name dmArray::Swap
     * @param rhs [type:dmArray`<T>`&] reference to array to swap content with
     */
    void Swap(dmArray<T>& rhs);

private:
    T *m_Front, *m_End;
    T *m_Back;
    uint16_t m_UserAllocated : 1;

    // Restrict copy-construction etc.
    dmArray(const dmArray<T>&) {}
    void operator =(const dmArray<T>&) {}
    void operator ==(const dmArray<T>&) {}
};

template <typename T>
dmArray<T>::dmArray()
{
    memset(this, 0, sizeof(*this));
}

template <typename T>
dmArray<T>::dmArray(T *user_array, uint32_t size, uint32_t capacity)
{
    assert(user_array != 0);
    assert(size  <= capacity);
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
        delete[] (uint8_t*) m_Front;
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
    dmArrayUtil::SetCapacity(capacity, sizeof(T), (uintptr_t*)&m_Front, (uintptr_t*)&m_Back, (uintptr_t*)&m_End);
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

#endif // DMSDK_ARRAY_H
