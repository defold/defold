/*
ABOUT:
    A c++ ring buffer class

LICENSE:
    Copyright (c) 2023 Mathias Westerdahl

    The MIT License (MIT)
    See full license at the end of this file

DISCLAIMER:

    This software is supplied "AS IS" without any warranties and support

*/

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <assert.h> // disable with NDEBUG

#include <string.h> // memmove

#define JC_SWAP(type, lhs, rhs)\
    {\
        type tmp = rhs;\
        rhs = lhs;\
        lhs = tmp;\
    }

namespace jc
{

/*
 * Uses realloc to allocate memory. Don't use for aligned types
 */
template <typename T>
class RingBuffer
{
public:
    RingBuffer() : m_Buffer(0), m_Head(0), m_Tail(0), m_Max(0), m_Full(1) {}
    RingBuffer(uint32_t capacity) : m_Buffer(0), m_Head(0), m_Tail(0), m_Max(0) { SetCapacity(capacity); }
    ~RingBuffer()                               { Free(); }

    void Swap(RingBuffer<T>& rhs)               {
                                                    JC_SWAP(T*,       m_Buffer, rhs.m_Buffer);
                                                    JC_SWAP(uint32_t, m_Head,   rhs.m_Head);
                                                    JC_SWAP(uint32_t, m_Tail,   rhs.m_Tail);
                                                    JC_SWAP(uint32_t, m_Max,    rhs.m_Max);
                                                    JC_SWAP(uint32_t, m_Full,   rhs.m_Full);
                                                }

    /// Gets the size of the buffer
    uint32_t    Size() const                    {
                                                    if (m_Full)
                                                        return m_Max;
                                                    return m_Head >= m_Tail ?
                                                            m_Head - m_Tail :
                                                            m_Max - (m_Tail - m_Head);
                                                }
    /// Empty if Size()==0
    bool        Empty() const                   { return Size()==0; }
    bool        Full() const                    { return m_Full!=0; }
    /// Clears the buffer and sets size to 0. Does not free memory
    void        Clear()                         { m_Head = m_Tail = 0; m_Full = m_Max==0; }
    /// Gets the capacity of the array
    uint32_t    Capacity() const                { return (uint32_t)m_Max; }
    /// Increases or decreases capacity. Invalidates pointers to elements
    void        SetCapacity(uint32_t capacity);
    /// Increases or decreases capacity. Invalidates pointers to elements
    void        OffsetCapacity(uint32_t grow);

    /// Adds one item to the ring buffer. Asserts if the buffer is full
    void        Push(const T& item);
    /// Adds one item to the ring buffer. Does not assert, If full, overwrites the value at the tail.
    void        PushUnchecked(const T& item);
    /// Removes (and returns) the first inserted item from the ring buffer. Asserts if the buffer is empty
    T           Pop();
    /// Removes the element at logical index (0-based), keeps order, returns old value
    T           Erase(uint32_t index);

    T&          operator[] (size_t i)           { assert(i < Size()); return m_Buffer[(m_Tail + i) % m_Max]; }
    const T&    operator[] (size_t i) const     { assert(i < Size()); return m_Buffer[(m_Tail + i) % m_Max]; }

    // Useful for sorting
    void        Flatten();           // in-place linearization so data starts at m_Buffer[0]
    void        FlattenUnordered();  // compacts contiguously without preserving logical order
    T*          Buffer() const               { return m_Buffer; }

    // Mainly for unit tests
    uint32_t    Head() const                 { return m_Head; }
    uint32_t    Tail() const                 { return m_Tail; }

private:
    T*          m_Buffer;
    uint32_t    m_Head;
    uint32_t    m_Tail;
    uint32_t    m_Max:31;
    uint32_t    m_Full:1;
    uint32_t    :32;

    RingBuffer(const RingBuffer<T>& rhs);
    RingBuffer<T>& operator= (const RingBuffer<T>& rhs);
    bool operator== (const RingBuffer<T>& rhs);

    void ReverseRange(uint32_t begin, uint32_t end_exclusive);

    void Free() {
        free(m_Buffer);
        m_Buffer = 0;
        m_Max = 0;
    }

    void IncPointer() {
        if (m_Full)
            m_Tail = (m_Tail + 1) % m_Max;
        m_Head = (m_Head + 1) % m_Max;
        m_Full = m_Tail == m_Head ? 1 : 0;
    }

    void DecPointer() {
        if (++m_Tail == m_Max)
            m_Tail = 0;
        m_Full = false;
    }
};

template <typename T>
void RingBuffer<T>::SetCapacity(uint32_t capacity)
{
    size_t old_capacity = Capacity();
    if (capacity == old_capacity)
        return;

    if (capacity == 0)
    {
        Free();
        return;
    }

    T* newbuffer = newbuffer = (T*)malloc(capacity*sizeof(T));

    uint32_t size = Size();
    if (size > capacity)
    {
        size = capacity;
    }

    // copy elements in reverse in order to keep the last inserted items
    // E.g. resize from cap=6 to cap=4: 123456 -> 3456
    for (int i = ((int)size)-1; i >= 0; --i)
    {
        newbuffer[i] = (*this)[(uint32_t)i];
    }

    T* tmp = m_Buffer;
    m_Buffer = newbuffer;
    free(tmp);

    m_Tail = 0;
    m_Head = size;
    m_Max = capacity;
    m_Full = capacity == size ? 1 : 0;
}

template <typename T>
void RingBuffer<T>::OffsetCapacity(uint32_t grow)
{
    SetCapacity(Capacity() + grow);
}

template <typename T>
void RingBuffer<T>::Push(const T& item)
{
    assert(!Full());
    m_Buffer[m_Head] = item;
    IncPointer();
}

template <typename T>
void RingBuffer<T>::PushUnchecked(const T& item)
{
    m_Buffer[m_Head] = item;
    IncPointer();
}

template <typename T>
T RingBuffer<T>::Pop()
{
    assert(!Empty());
    T item = m_Buffer[m_Tail];
    DecPointer();
    return item;
}

template <typename T>
T RingBuffer<T>::Erase(uint32_t index)
{
    uint32_t size = Size();
    assert(index < size);

    // Compute physical index of the element to erase
    uint32_t pos = (m_Tail + index) % m_Max;

    // Store old value to return
    T old_value = m_Buffer[pos];

    // Shift elements left from pos towards head to preserve order
    // Stop when the next index equals m_Head (the logical end)
    while (true)
    {
        uint32_t next = pos + 1;
        if (next >= m_Max) next = 0;
        if (next == m_Head)
            break;
        m_Buffer[pos] = m_Buffer[next];
        pos = next;
    }

    // One element removed: move head back by one and clear full flag
    if (m_Head == 0)
    {
        m_Head = m_Max - 1;
    }
    else
    {
        --m_Head;
    }
    m_Full = 0;

    return old_value;
}


// some cases: .=free, T=Tail, H=Head, *=used
//  1) [T****H....]
//  2) [...T***H..]
//  3) [*H...T****]

template <typename T>
void RingBuffer<T>::FlattenUnordered()
{
    uint32_t capacity = m_Max;
    uint32_t size = Size();
    if (size == 0 || m_Tail == 0)
        return; // already contiguous or empty

    if (m_Tail < m_Head)
    {
        // Single contiguous block at offset; shift entire block to start
        memmove(m_Buffer, m_Buffer + m_Tail, sizeof(T) * size);
    }
    else
    {
        // Wrapped case: move the rightmost part down directly after the left part.
        // This compacts elements into [0..size-1] without preserving order.
        uint32_t right_len = capacity - m_Tail; // [m_Tail .. m_Max)
        memmove(m_Buffer + m_Head, m_Buffer + m_Tail, sizeof(T) * right_len);
    }

    m_Tail = 0;
    m_Head = size;
}

// In-place linearization. After this call, the logical sequence is contiguous
// at m_Buffer[0..Size()-1]. Does not allocate temporary buffers.
template <typename T>
void RingBuffer<T>::Flatten()
{
    uint32_t capacity = m_Max;
    uint32_t size = Size();

    // Early out when already contiguous (tail at 0) or empty
    // No state changes in these cases.
    if (size == 0 || m_Tail == 0)
        return;

    if (m_Tail < m_Head)
    {
        // Single contiguous block but offset; shift down to index 0
        memmove(m_Buffer, m_Buffer + m_Tail, sizeof(T) * size);
    }
    else
    {
        // Wrapped case: rotate the whole buffer left by m_Tail positions.
        // This makes the logical tail land at index 0. Elements outside
        // [0..size-1] are unspecified and may contain moved free slots.
        uint32_t n = capacity;
        uint32_t k = m_Tail % n;
        if (k)
        {
            ReverseRange(0, k);
            ReverseRange(k, n);
            ReverseRange(0, n);
        }
    }

    m_Tail = 0;
    m_Head = size;
}

template <typename T>
void RingBuffer<T>::ReverseRange(uint32_t begin, uint32_t end_exclusive)
{
    while (begin < end_exclusive)
    {
        --end_exclusive;
        if (begin >= end_exclusive)
            break;
        T tmp = m_Buffer[begin];
        m_Buffer[begin] = m_Buffer[end_exclusive];
        m_Buffer[end_exclusive] = tmp;
        ++begin;
    }
}


} // namespace

#undef JC_SWAP

/*

VERSION:

    1.1 2025-10-08  Added Swap(), Erase() and Flatten*() functions
    1.0             Initial version


LICENSE:

    The MIT License (MIT)

    Copyright (c) 2023 Mathias Westerdahl

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

*/
