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
    /// Adds one item to the ring buffer. Asserts if the buffer is full
    void        Push(const T& item);
    /// Adds one item to the ring buffer. Does not assert, If full, overwrites the value at the tail.
    void        PushUnchecked(const T& item);
    /// Removes (and returns) the first inserted item from the ring buffer. Asserts if the buffer is empty
    T           Pop();

    T&          operator[] (size_t i)           { assert(i < Size()); return m_Buffer[(m_Tail + i) % m_Max]; }
    const T&    operator[] (size_t i) const     { assert(i < Size()); return m_Buffer[(m_Tail + i) % m_Max]; }

    // Mainly for unit tests
    uint32_t    Head() const                 { return m_Head; }
    uint32_t    Tail() const                 { return m_Tail; }
    T*          Buffer() const               { return m_Buffer; }

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

} // namespace

/*

VERSION:

    1.0 Initial version


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
