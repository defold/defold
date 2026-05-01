// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_SET_H
#define DM_SET_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*# Templatized set
 * Templatized set with bounds checking.
 *
 * The backing storage is auto-allocated (dynamically allocated)
 * With exception of changing the size and capacity, all operations are guaranteed to be O(1).
 *
 * ```cpp
 * dmSet<int> a;
 * a.SetCapacity(1);
 * a.Add(1);
 * uint32_t size = a.Size();
 * bool exists = a.Contains(1);
 * int b = a[0];
 * a.Remove(1);
 * ```
 *
 * @document
 * @name Set
 * @namespace dmSet
 * @language C++
 */

/*# Templatized set with bounds checking.
 *
 * The backing storage is either auto-allocated (dynamically allocated) or user-allocated (supplied by user).
 * With exception of changing the size and capacity, all operations are guaranteed to be O(1).
 *
 * @class
 * @name dmSet
 * @tparam T Contained type, must obey memcpy semantics and support operator<
 */
template <typename T>
class dmSet
{
public:
    /*# constructor. empty auto-allocated memory
     *
     * @name dmSet
     * @examples
     *
     * ```cpp
     * dmSet<int> a;
     * a.Add(1);
     * a.Remove(1);
     * ```
     */
    dmSet();

    /*# set destructor
     *
     * Only frees memory when auto-allocated.
     *
     * @name ~dmSet
     */
    ~dmSet();

    /*# set begin
     *
     * Pointer to the start of the backing storage
     *
     * @name Begin
     * @return pointer [type: const T*] pointer to start of memory
     */
    const T* Begin() const;

    /*# set end
     *
     * Pointer to the end of the backing storage
     * The end is essentially outside of the used storage.
     *
     * @name End
     * @return pointer [type:const T*] pointer to end of memory
     */
    const T* End() const;

    /*# size of set
     *
     * Size of the set in elements
     *
     * @name Size
     * @return number [type:uint32_t] set size
     */
    uint32_t Size() const;

    /*# capacity of set
     *
     * Capacity is currently allocated storage.
     *
     * @name Capacity
     * @return number [type:uint32_t] set capacity
     */
    uint32_t Capacity() const;

    /*# set full
     *
     * Check if the set is full.
     * The set is full when the size is equal to the capacity.
     *
     * @name Full
     * @return boolean [type:boolean] true if the set is full
     */
    bool Full() const;

    /*# set empty
     *
     * Check if the set is empty.
     * The set is empty when the size is zero.
     *
     * @name Empty
     * @return boolean [type:boolean] true if the set is empty
     */
    bool Empty() const;

    /*# remaining size of set
     *
     * Amount of additional elements that can be stored
     *
     * @name Remaining
     * @return number [type:uint32_t] amount of additional elements that can be stored
     */
    uint32_t Remaining() const;

    /*# set operator[] (const)
     *
     * Retrieve an element by index (const).
     * Consider the values unsorted.
     *
     * @name operator[]
     * @param index [type:uint32_t] set index
     * @return reference [type:const T&] const-reference to the element at the specified index
     */
    const T& operator[](uint32_t i) const;

    /*# set capacity
     *
     * Set the capacity of the set.
     * If the size is less than the capacity, the set is truncated.
     * If it is larger, the set is extended.
     *
     * @name SetCapacity
     * @param capacity [type:uint32_t] capacity of the set
     */
    void SetCapacity(uint32_t capacity);

    /*# set offset capacity
     *
     * Relative change of capacity
     * Equivalent to SetCapacity(Capacity() + offset).
     *
     * @name OffsetCapacity
     * @param offset [type:uint32_t] relative amount of elements to change the capacity
     */
    void OffsetCapacity(int32_t offset);

    /*# set remove
     *
     * Remove the value from the set
     *
     * @name Remove
     * @param value [type: const T&] Value to remove
     * @return result [type: bool] True if the element was removed. False otherwise
     */
    bool Remove(const T& element);

    /*# set add
     *
     * Add an element to the end of the set
     * Only allowed when the capacity is larger than size.
     * Asserts if Full()
     *
     * @name Add
     * @param element [type:const T&] element element to add
     * @return result [type: bool] True if the element was added. False if it already existed.
     */
    bool Add(const T& element);

    /*# set contains
     *
     * Returns true of the set contains the value
     *
     * @name Contains
     * @param element [type: const T&] element element to add
     * @return exists [type: bool] True if the element exists in the set
     */
    bool Contains(const T& element);

private:
    T*        m_Values;
    uint32_t  m_Size;
    uint32_t  m_Capacity;

    // Restrict copy-construction etc.
    dmSet(const dmSet<T>&) {}
    void operator =(const dmSet<T>&) {}
    void operator ==(const dmSet<T>&) {}

    static T* LowerBound(T* first, T* last, T val)
    {
        uint32_t size = (uint32_t)(last - first);
        while (size > 0) {
            uint32_t half = size >> 1;
            T* middle = first + half;

            if (*middle < val)
            {
                first = middle + 1;
                size = size - half - 1;
            }
            else
            {
                size = half;
            }
        }
        return first;
    }
};

template <typename T>
dmSet<T>::dmSet()
{
    memset(this, 0, sizeof(*this));
}

template <typename T>
dmSet<T>::~dmSet()
{
    free((void*)m_Values);
}

template <typename T>
const T* dmSet<T>::Begin() const
{
    return m_Values;
}

template <typename T>
const T* dmSet<T>::End() const
{
    return m_Values + m_Size;
}

template <typename T>
uint32_t dmSet<T>::Size() const
{
    return m_Size;
}

template <typename T>
uint32_t dmSet<T>::Capacity() const
{
    return m_Capacity;
}

template <typename T>
bool dmSet<T>::Full() const
{
    return m_Size == m_Capacity;
}

template <typename T>
bool dmSet<T>::Empty() const
{
    return m_Size == 0;
}

template <typename T>
uint32_t dmSet<T>::Remaining() const
{
    return m_Capacity - m_Size;
}

template <typename T>
const T& dmSet<T>::operator[](uint32_t i) const
{
    assert(i < Size());
    return m_Values[i];
}

template <typename T>
void dmSet<T>::SetCapacity(uint32_t capacity)
{
    m_Values = (T*)realloc(m_Values, sizeof(T) * capacity);
    m_Capacity = capacity;
    if (m_Size > m_Capacity)
        m_Size = m_Capacity;
}

template <typename T>
void dmSet<T>::OffsetCapacity(int32_t offset)
{
    SetCapacity(Capacity() + offset);
}

template <typename T>
bool dmSet<T>::Contains(const T& value)
{
    if (m_Size == 0)
        return 0;
    T* end = m_Values + m_Size;
    const T* p = LowerBound(m_Values, end, value);
    if (p >= end)
        return 0;
    return (*p) == value;
}

template <typename T>
bool dmSet<T>::Add(const T& value)
{
    T* end = m_Values + m_Size;
    T* p = LowerBound(m_Values, end, value);
    if (p < end && (*p) == value)
    {
        return false; // Already existed
    }

    assert(!Full());

    uint32_t count = end - p;
    memmove(p+1, p, count * sizeof(T));
    (*p) = value;
    m_Size++;
    return true;
}

template <typename T>
bool dmSet<T>::Remove(const T& value)
{
    if (m_Size == 0)
        return false;

    T* end = m_Values + m_Size;
    T* p = LowerBound(m_Values, end, value);
    if (p >= end)
        return false;
    if ((*p) != value)
        return false;

    uint32_t count = end - p;
    memmove(p, p+1, (count-1) * sizeof(T));
    m_Size--;
    return true;
}

#endif // DM_SET_H
