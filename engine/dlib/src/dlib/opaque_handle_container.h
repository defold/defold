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

#ifndef DM_OPAQUE_HANDLE_CONTAINER_H
#define DM_OPAQUE_HANDLE_CONTAINER_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t HOpaqueHandle;

/// Identifier for an invalid handle
static const HOpaqueHandle INVALID_OPAQUE_HANDLE = 0xFFFFFFFF;

/*# Templated handle container
 *
 * The handle container is a helper structure for associating dynamically allocated object pointers with a generic numeric handle.
 * A handle is constructed by two parts: a index and a version. The index part of the handle is an index into the list of objects
 * stored by the container. The version of a handle is a serial number that will be increased for every item put into the list,
 * which is used to determine the validity of the handles in case a lingering handle is used to fetch objects in the container.
 *
 * Notes:
 *   - The maximum handles this container can store is 65535 (0xFFFF)
 *   - The list of objects can only grow upwards by calling the Allocate function, there is currently no way of shrinking the list of objects.
 *
 * @class
 * @name dmOpaqueHandleContainer
 * @tparam T Contained type pointer
 */
template <typename T>
class dmOpaqueHandleContainer
{
public:
    /*# constructor without object container allocations
     *
     * @name dmOpaqueHandleContainer
     */
    dmOpaqueHandleContainer();

    /*# constructor, auto-allocates storage
     *
     * Allocates a predefined number of slots for objects when this function is called,
     * that contains 'initial_capacity' amount of empty objects.
     *
     * @name dmOpaqueHandleContainer
     * @param inital_capacity [type:uint32_t] Initial capacity
     */
    dmOpaqueHandleContainer(uint32_t inital_capacity);

    /*# array destructor
     *
     * Frees up the dynamically container memory, but will not
     * free the objects themselves.
     *
     * @name ~dmOpaqueHandleContainer
     */
    ~dmOpaqueHandleContainer();

    /*# container allocate
     *
     * Increases the size of the container.
     *
     * @name Allocate
     * @param count [type:uint32_t] Number of items to increase the container with
     */
    bool Allocate(uint32_t count);

    /*# container get
     *
     * Get the pointer to the object associated with a handle. The handle will be checked for validity,
     * i.e the handle version must match the object version and the index of the handle must be in a valid
     * range of the object list. If the handle is invalid, a NULL pointer will be returned.
     *
     * @name Get
     * @param handle [type:HOpaqueHandle] The object handle
     * @return pointer [type:T*] Return the object associated with the handle If it is valid, NULL otherwise
     */
    T* Get(HOpaqueHandle handle);

    /*# container get by index
     *
     * Get the pointer to the object based on an index into the container. The handle will be checked for validity,
     * i.e the handle version must match the object version and the index of the handle must be in a valid
     * range of the object list. If the handle is invalid, a NULL pointer will be returned.
     *
     * Note: this is indented for internal use only and relies on the fact that the backing
     *       data structure for the container is a continuous array.
     *
     * @name GetByIndex
     * @param index [type:uint32_t] The index to query the container with
     * @return pointer [type:T*] Return the object associated with the handle If it is valid, NULL otherwise
     */
    T* GetByIndex(uint32_t index);

    /*# container get handle by index
     *
     * Constructs an opaque handle from the index itself plus the stored version for that index.
     * The index must be within a valid range of the backing array,
     * as denoted by the container capacity ([0...c-1] where c equals capacity).
     *
     * Note: this is indented for internal use only and relies on the fact that the backing
     *       data structure for the container is a continuous array.
     *
     * @name IndexToHandle
     * @param index [type:uint32_t] The index to query the container with
     * @return [type:HOpaqueHandle] Return the constructed object handle
     */
    HOpaqueHandle IndexToHandle(uint32_t index);

    /*# container put
     *
     * Adds a reference to an object to the list and returns an opaque handle to that object. If the container
     * is full, an assert will be triggered. If you need to know if a handle can be stored, you need to call
     * your_container.Full() before issuing a .Put() call.
     *
     * Note: This function DOES NOT check if the object exists before adding it to the list. You will have
     *       to maintain this manually by checking that with Get(handle) first.
     *
     * @name Put
     * @param obj [type:T*] The object pointer
     * @return [type:HOpaqueHandle] Return an opaque handle to the object.
     */
    HOpaqueHandle Put(T* obj);

    /*# container release
     *
     * Removes the references to an object based on a handle, if the handle is valid (i.e points to a valid object in the container).
     *
     * @name Release
     * @param handle [type:HOpaqueHandle] The handle to release
     */
    void Release(HOpaqueHandle handle);

    /*# container capacity
     *
     * Queries the max objects this container can hold.
     *
     * @name Capacity
     * @return [type:uint32_t] Returns the max object references this container can hold.
     */
    uint32_t Capacity();

    /*# container full
     *
     * Queries if the container is full. The container is full if every slots in the list is taken by some object.
     *
     * @name Full
     * @return [type:bool] Returns true if all object slots are used, and false if there are available slots.
     */
    bool Full();

private:
    T**       m_Objects;
    uint16_t* m_ObjectVersions;
    uint32_t  m_Capacity;
    uint16_t  m_Version;

    // TODO: We can improve the performance of this function by adding as freelist (see comments in https://github.com/defold/defold/pull/7421)
    uint32_t GetFirstEmptyIndex()
    {
        for (int i = 0; i < m_Capacity; ++i)
        {
            if (m_Objects[i] == 0)
            {
                return i;
            }
        }
        return INVALID_OPAQUE_HANDLE;
    }

    dmOpaqueHandleContainer(const dmOpaqueHandleContainer<T>&) {}
    void operator =(const dmOpaqueHandleContainer<T>&) {}
    void operator ==(const dmOpaqueHandleContainer<T>&) {}
};


//////////////////////////////////////////////////////////////
// Public API implementation
//////////////////////////////////////////////////////////////

template <typename T>
dmOpaqueHandleContainer<T>::dmOpaqueHandleContainer()
: m_Objects(0)
, m_ObjectVersions(0)
, m_Capacity(0)
, m_Version(0)
{}

template <typename T>
dmOpaqueHandleContainer<T>::dmOpaqueHandleContainer(uint32_t inital_capacity)
: dmOpaqueHandleContainer()
{
    Allocate(inital_capacity);
}

template <typename T>
dmOpaqueHandleContainer<T>::~dmOpaqueHandleContainer()
{
    if (m_Objects != 0)
    {
        free(m_Objects);
        free(m_ObjectVersions);
    }
}

template <typename T>
bool dmOpaqueHandleContainer<T>::Allocate(uint32_t count)
{
    uint32_t new_capacity = m_Capacity + count;
    assert(new_capacity <= 0xFFFF);

    m_Objects             = (T**) realloc(m_Objects, new_capacity * sizeof(T*));
    m_ObjectVersions      = (uint16_t*) realloc(m_ObjectVersions, new_capacity * sizeof(uint16_t));

    memset(m_Objects + m_Capacity, 0, count * sizeof(T*));
    memset(m_ObjectVersions + m_Capacity, 0, count * sizeof(uint16_t));

    m_Capacity += count;

    return m_Objects != 0x0 && m_ObjectVersions != 0x0;
}

template <typename T>
T* dmOpaqueHandleContainer<T>::Get(HOpaqueHandle handle)
{
    if (handle == 0 || handle == INVALID_OPAQUE_HANDLE)
    {
        return 0;
    }

    uint32_t version = handle >> 16;
    uint32_t index   = handle & 0xFFFF;
    T* entry         = GetByIndex(index);

    if (entry == 0 || m_ObjectVersions[index] != version)
    {
        return 0;
    }

    return entry;
}

template <typename T>
T* dmOpaqueHandleContainer<T>::GetByIndex(uint32_t index)
{
    assert(index < m_Capacity);
    return m_Objects[index];
}

template <typename T>
HOpaqueHandle dmOpaqueHandleContainer<T>::IndexToHandle(uint32_t index)
{
    assert(index < m_Capacity);
    return m_ObjectVersions[index] << 16 | index;
}

template <typename T>
HOpaqueHandle dmOpaqueHandleContainer<T>::Put(T* obj)
{
    uint32_t index = GetFirstEmptyIndex();
    // Same as asserting on Full(), but faster
    assert(index != INVALID_OPAQUE_HANDLE);

    m_Version++;
    if (m_Version == 0 || m_Version == 0xFFFF)
    {
        // Set to 1 to avoid potentially producing a 0 or a 0xFFFF handle
        m_Version = 1;
    }

    m_ObjectVersions[index]  = m_Version;
    m_Objects[index]         = obj;
    HOpaqueHandle new_handle = m_Version << 16 | index;

    assert(new_handle != INVALID_OPAQUE_HANDLE);
    return new_handle;
}

template <typename T>
void dmOpaqueHandleContainer<T>::Release(HOpaqueHandle handle)
{
    if (handle == 0 || handle == INVALID_OPAQUE_HANDLE || !Get(handle))
    {
        return;
    }
    uint32_t index          = handle & 0xFFFF;
    m_Objects[index]        = 0;
    m_ObjectVersions[index] = 0;
}

template <typename T>
uint32_t dmOpaqueHandleContainer<T>::Capacity()
{
    return m_Capacity;
}

template <typename T>
bool dmOpaqueHandleContainer<T>::Full()
{
    return GetFirstEmptyIndex() == INVALID_OPAQUE_HANDLE;
}

#endif // DM_OPAQUE_HANDLE_CONTAINER_H
