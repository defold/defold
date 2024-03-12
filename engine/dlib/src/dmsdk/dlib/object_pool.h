// Copyright 2020-2024 The Defold Foundation
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


#ifndef DMSDK_OBJECT_POOL_H
#define DMSDK_OBJECT_POOL_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>

/*# SDK Object Pool API documentation
 *
 * @document
 * @name ObjectPool
 * @path engine/dlib/src/dmsdk/dlib/object_pool.h
 */

/*#
 * Object pool data-structure with the following properties
 * - Mapping from logical index to physical index
 * - Logical index does not changes
 * - Allocated objects are contiguously laid out in memory
 *   Loop of m_Objects [0..Size()-1] times to iterate all objects
 * - Internal physical order is not preserved and a direct consequence of the
 *   contiguous property
 *
 * @struct
 * @name dmObjectPool
 */
template <typename T>
class dmObjectPool
{
private:

    struct Entry
    {
        uint32_t m_Physical;
        uint32_t m_Next;
    };

    dmArray<T>          m_Objects; // All objects [0..Size()]
    dmArray<Entry>      m_Entries;
    dmArray<uint32_t>   m_ToLogical;
    uint32_t            m_FirstFree;

public:

    /*#
     * Constructor
     * @name dmObjectPool
     * @param capacity
     */
    dmObjectPool()
    {
        m_FirstFree = 0xffffffff;
    }

    /*#
     * Set capacity. New capacity must be >= old_capacity
     * @name SetCapacity
     * @param capacity [type: uint32_t] max number of objects to store
     */
    void SetCapacity(uint32_t capacity)
    {
        assert(capacity >= m_Objects.Capacity());

        m_Entries.SetCapacity(capacity);
        m_Objects.SetCapacity(capacity);

        m_ToLogical.SetCapacity(capacity);
        m_ToLogical.SetSize(capacity);
    }

    /*#
     * Allocate a new object
     * @name Alloc
     * @return index [type:uint32_t] logical index
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

    /*# get size
     * Get number of objects currently stored
     * @name Size
     * @return size [type: uint32_t] returns the number of objects currently stored
     */
    uint32_t Size()
    {
        return m_Objects.Size();
    }

    /*#
     * Checks if the pool is full
     * @name Full
     * @return result [type: bool] returns true if the pool is full
     */
    bool Full()
    {
        return m_Objects.Remaining() == 0;
    }

    /*#
     * @name Capacity
     * @return capacity [type: uint32_t] maximum number of objects
     */
    uint32_t Capacity()
    {
        return m_Objects.Capacity();
    }

    /*#
     * Returns object index to the object pool
     *
     * @name Free
     * @param index [type: uint32_t] index of object
     * @param clear [type: bool] If set, memset's the object memory
     */
    void Free(uint32_t index, bool clear)
    {
        uint32_t size = m_Objects.Size();
        Entry* e = &m_Entries[index];
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

    /*#
     * Get the array of currently active objects
     * @note The order of objects in this array may change if Alloc() or Free() has been called
     * @name GetRawObjects
     * @return object [type: dmArray<T>&] a reference to the array of objects
     */
    dmArray<T>& GetRawObjects()
    {
        return m_Objects;
    }

    /*#
     * Get object from logical index
     * @name Get
     * @param index [type: uint32_t] index of the object
     * @return object [type: T&] a reference to the object
     */
    T& Get(uint32_t index)
    {
        Entry* e = &m_Entries[index];
        T& o = m_Objects[e->m_Physical];
        return o;
    }

    /*#
     * Set object from logical index
     * @name Set
     * @param index [type: uint32_t] index of object
     * @param object [type: T&] reference ot object. THe object stored is copied by value.
     */
    void Set(uint32_t index, T& object)
    {
        Entry* e = &m_Entries[index];
        m_Objects[e->m_Physical] = object;
    }

};

#endif /* DMSDK_OBJECT_POOL_H */
