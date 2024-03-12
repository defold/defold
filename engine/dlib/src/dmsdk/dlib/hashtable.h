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

#ifndef DMSDK_HASHTABLE_H
#define DMSDK_HASHTABLE_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*# Hash table
 *
 * Hash table
 *
 * @document
 * @name Hashtable
 * @namespace dmHashTable
 * @path engine/dlib/src/dmsdk/dlib/hashtable.h
 */


/*# hashtable
 * Hashtable with chaining for collision resolution, memcpy-copy semantics (POD types) and 32-bit indicies instead of pointers. (NUMA-friendly)
 * @note The key type needs to support == and % operators
 * @type class
 * @name dmHashTable
 */
template <typename KEY, typename T>
class dmHashTable
{
    enum STATE_FLAGS
    {
        STATE_DEFAULT           = 0x0,
        STATE_USER_ALLOCATED    = 0x1
    };

    static const uint32_t INVALID_INDEX = 0xFFFFFFFF;
    static const uint32_t MAX_SIZE      = 0xFFFFFFFF;

public:
    struct Entry
    {
        KEY      m_Key;
        T        m_Value;
        uint32_t m_Next;
    };

    /**
     * Constructor. Create an empty hashtable with zero capacity and zero hashtable (buckets)
     * @name dmHashTable
     */
    dmHashTable()
    {
        memset(this, 0, sizeof(*this));
        m_FreeEntries = INVALID_INDEX;
    }

    /**
     * Creates a hashtable array with user allocated memory.
     * @note User allocated arrays can not change capacity.
     * @name dmHashTable
     * @param user_allocated Pointer to user allocated continous data-block ((table_size*sizeof(uint32_t)) + (capacity*sizeof(dmHashTable::Entry))
     * @param table_size Hashtable size, ie number of buckets. table_size < 0xffffffff
     * @param capacity Capacity. capacity < 0xffffffff
     */
    dmHashTable(void *user_allocated, uint32_t table_size, uint32_t capacity)
    {
        assert(table_size < MAX_SIZE);
        assert(capacity < MAX_SIZE);
        memset(this, 0, sizeof(*this));
        m_FreeEntries = MAX_SIZE;

        m_HashTableSize = table_size;
        m_HashTable = (uint32_t*) user_allocated;
        memset(m_HashTable, 0xff, sizeof(uint32_t) * table_size);

        m_InitialEntries = (Entry*) (m_HashTable + table_size);
        m_InitialEntriesNextFree = m_InitialEntries;
        m_InitialEntriesEnd = m_InitialEntries + capacity;
        m_State = STATE_USER_ALLOCATED;
    }

    /**
     * Removes all the entries from the table.
     * @name Clear
     */
    void Clear()
    {
        memset(m_HashTable, 0xff, sizeof(uint32_t) * m_HashTableSize);
        m_InitialEntriesNextFree = m_InitialEntries;
        m_FreeEntries = INVALID_INDEX;
        m_Count = 0;
    }

    /**
     * Destructor.
     * @name ~dmHashTable
     * @note If user allocated, memory is not free'd
     */
    ~dmHashTable()
    {
        if(!(m_State & STATE_USER_ALLOCATED))
        {
            if (m_InitialEntries)
            {
                free(m_InitialEntries);
            }

            if (m_HashTable)
            {
                free(m_HashTable);
            }
        }
    }

    /**
     * Number of entries stored in table. (not the actual hashtable size)
     * @name Size
     * @return Number of entries.
     */
    uint32_t Size() const
    {
        return m_Count;
    }

    /**
     * Hashtable capacity. Maximum number of entries possible to store in table
     * @name Capacity
     * @return [type: uint32_t] the capacity of the table
     */
    uint32_t Capacity() const
    {
        return (uint32_t)(uintptr_t)(m_InitialEntriesEnd - m_InitialEntries);
    }

    /**
     * Set hashtable capacity. New capacity must be greater or equal to current capacity
     * @name SetCapacity
     * @param table_size Hashtable size, ie number of buckets. table_size < 0xffffffff
     * @param capacity Capacity. capacity < 0xffffffff
     */
    void SetCapacity(uint32_t table_size, uint32_t capacity)
    {
        assert(table_size > 0);
        assert(table_size < MAX_SIZE);
        assert(capacity < MAX_SIZE);
        assert(capacity >= Capacity());

        if (m_InitialEntries == 0)
        {
            m_HashTableSize = table_size;
            m_HashTable = (uint32_t*) malloc(sizeof(uint32_t) * table_size);
            memset(m_HashTable, 0xff, sizeof(uint32_t) * table_size);

            m_InitialEntries = (Entry*) malloc(sizeof(Entry) * capacity);
            m_InitialEntriesNextFree = m_InitialEntries;
            m_InitialEntriesEnd = m_InitialEntries + capacity;
        }
        else
        {
            // Rehash table
            dmHashTable<KEY, T> new_ht;
            new_ht.SetCapacity(table_size, capacity);
            this->Iterate<dmHashTable<KEY, T> >(&FillCallback<KEY, T>, &new_ht);

            free(m_HashTable);
            free(m_InitialEntries);
            memcpy(this, &new_ht, sizeof(*this));

            // Avoid double free()
            new_ht.m_HashTable = 0;
            new_ht.m_InitialEntries = 0;
        }
    }

    /**
     * Swaps the contents of two hash tables
     * @name Swap
     * @param other [type: dmHashTable<KEY, T>&] the other table
     */
    void Swap(dmHashTable<KEY, T>& other)
    {
        char buf[sizeof(*this)];
        memcpy(buf, &other, sizeof(buf));
        memcpy(&other, this, sizeof(buf));
        memcpy(this, buf, sizeof(buf));
    }

    /**
     * Check if the table is full
     * @name Full
     * @return true if the table is full
     */
    bool Full()
    {
        return m_Count == Capacity();
    }

    /**
     * Check if the table is empty
     * @name Empty
     * @return true if the table is empty
     */
    bool Empty()
    {
        return m_Count == 0;
    }

    /**
     * Put key/value pair in hash table. NOTE: The method will "assert" if the hashtable is full.
     * @name Put
     * @param key [type: Key] Key
     * @param value [type: const T&] Value
     */
    void Put(KEY key, const T& value)
    {
        assert(!Full());
        Entry* entry = FindEntry(key);

        // Key already in table?
        if (entry != 0)
        {
            // TODO: memcpy or similar to enforce memcpy-semantics?
            entry->m_Value = value;
            return;
        }
        else
        {
            entry = AllocateEntry();
            entry->m_Key = key;
            entry->m_Value = value;
            entry->m_Next = INVALID_INDEX;

            uint32_t bucket_index = (uint32_t) (key % m_HashTableSize);
            uint32_t entry_ptr = m_HashTable[bucket_index];
            if (entry_ptr == INVALID_INDEX)
            {
                m_HashTable[bucket_index] = (uint32_t)(uintptr_t)(entry - m_InitialEntries); // Store the index of the entry
            }
            else
            {
                // We need to traverse the list of entries for the bucket
                Entry* prev_entry;
                while (entry_ptr != INVALID_INDEX)
                {
                    prev_entry = &m_InitialEntries[entry_ptr];
                    entry_ptr = prev_entry->m_Next;
                }
                assert(prev_entry->m_Next == INVALID_INDEX);

                // Link prev entry to this
                prev_entry->m_Next = (uint32_t)(uintptr_t)(entry - m_InitialEntries);
            }
        }

        m_Count++;
    }

    /**
     * Get pointer to value from key
     * @name Get
     * @param key [type: Key] Key
     * @return value [type: T*] Pointer to value. NULL if the key/value pair doesn't exist.
     */
    T* Get(KEY key)
    {
        Entry* entry = FindEntry(key);

        // Key already in table?
        if (entry != 0)
        {
            return &entry->m_Value;
        }
        else
        {
            return 0;
        }
    }

    /**
     * Get pointer to value from key. "const" version.
     * @name Get
     * @param key [type: Key] Key
     * @return value [type: const T*] Pointer to value. NULL if the key/value pair doesn't exist.
     */
    const T* Get(KEY key) const
    {
        Entry* entry = FindEntry(key);

        // Key already in table?
        if (entry != 0)
        {
            return &entry->m_Value;
        }
        else
        {
            return 0;
        }
    }

    /**
     * Get pointer to first entry in table
     * @return Pointer to first entry. NULL if the table is empty.
     */
    // Entry* GetFirstEntry()
    // {
    //     if(Empty())
    //         return 0;
    //     return m_InitialEntries;

    // }

    /**
     * Remove key/value pair.
     * @name Get
     * @param key [type: Key] Key to remove
     * @note Only valid if key exists in table
     */
    void Erase(KEY key)
    {
        // Avoid module division by zero
        assert(m_HashTableSize != 0);

        uint32_t bucket_index = key % m_HashTableSize;
        uint32_t entry_ptr = m_HashTable[bucket_index];

        // Empty list?
        assert(entry_ptr != INVALID_INDEX);

        Entry* prev_e = 0;
        while (entry_ptr != INVALID_INDEX)
        {
            Entry* e = &m_InitialEntries[entry_ptr];
            if (e->m_Key == key)
            {
                --m_Count;
                // The entry first in list?
                if (prev_e == 0)
                {
                    // Relink
                    m_HashTable[bucket_index] = e->m_Next;
                }
                else
                {
                    // Relink, skip "this"
                    prev_e->m_Next = e->m_Next;
                }
                FreeEntry(e);
                return;
            }
            entry_ptr = e->m_Next;
            prev_e = e;
        }
        assert(false && "Key not found (erase)");
    }

    /**
     * Iterate over all entries in table
     * @name Iterate
     * @param call_back Call-back called for every entry
     * @param context Context
     */
    template <typename CONTEXT>
    void Iterate(void (*call_back)(CONTEXT *context, const KEY* key, T* value), CONTEXT* context) const
    {
        for (uint32_t i = 0; i < m_HashTableSize; ++i)
        {
            uint32_t entry_ptr = m_HashTable[i];
            while (entry_ptr != INVALID_INDEX)
            {
                Entry* e = &m_InitialEntries[entry_ptr];
                call_back(context, &e->m_Key, &e->m_Value);
                entry_ptr = e->m_Next;
            }
        }
    }

    /*#
     * Iterator to the key/value pairs of a hash table
     * @struct
     * @name Iterator
     * @member GetKey()
     * @member GetValue()
     */
    struct Iterator
    {
        // public
        const KEY&  GetKey()    { return m_EntryPtr->m_Key; }
        const T&    GetValue()  { return m_EntryPtr->m_Value; }

        Iterator(dmHashTable<KEY, T>& table)
            : m_Table(table)
            , m_EntryPtr(0)
            , m_Entry(INVALID_INDEX)
            , m_BucketIndex(0)
        {
        }

        bool Next() {
            if (m_EntryPtr)
                m_Entry = m_EntryPtr->m_Next;

            if (m_Entry == INVALID_INDEX)
            {
                for (uint32_t i = m_BucketIndex; i < m_Table.m_HashTableSize; ++i)
                {
                    if (m_Table.m_HashTable[i] != INVALID_INDEX)
                    {
                        m_Entry = m_Table.m_HashTable[i];
                        m_BucketIndex = i+1;
                        break;
                    }
                }
            }
            m_EntryPtr = m_Entry == INVALID_INDEX ? 0 : &m_Table.m_InitialEntries[m_Entry];
            return m_EntryPtr != 0;
        }

        // private
        dmHashTable<KEY, T>&    m_Table;
        Entry*                  m_EntryPtr;
        uint32_t                m_Entry;
        uint32_t                m_BucketIndex;
    };

    /*#
     * Get an iterator for the key/value pairs
     * @name GetIterator
     * @return iterator [type: dmHashTable<T>::Iterator] the iterator
     * @examples
     * ```cpp
     * dmHashTable<dmhash_t, int>::Iterator iter = ht.GetIterator();
     * while(iter.Next())
     * {
     *     printf("%s: %d\n", dmHashReverseSafe64(iter.GetKey()), iter.GetValue());
     * }
     * ```
     */
    Iterator GetIterator()
    {
        Iterator iter(*this);
        return Iterator(*this);
    }

    /**
     * Verify internal structure. "assert" if invalid. For unit testing
     */
    void Verify()
    {
        // Ensure that not items in free list is used
        uint32_t free_ptr = m_FreeEntries;
        while (free_ptr != 0xffffffff)
        {
            Entry* e = &m_InitialEntries[free_ptr];
            // Check that free entry not is in table
            Entry* found = FindEntry(e->m_Key);
            if (found && found == e )
            {
                printf("Key '%d' in table but also key '%d' in free list.\n", found->m_Key, e->m_Key);
            }
            assert( found != e );
            free_ptr = e->m_Next;
        }

        uint32_t real_count = 0;
        for (uint32_t i = 0; i < m_HashTableSize; ++i)
        {
            if (m_HashTable[i] != 0xffffffff)
            {
                uint32_t entry_ptr = m_HashTable[i];
                while (entry_ptr != 0xffffffff)
                {
                    real_count++;
                    Entry*e = &m_InitialEntries[entry_ptr];
                    entry_ptr = e->m_Next;
                }
            }
        }
        assert(real_count == m_Count);
    }

private:
    // Forbid assignment operator and copy-constructor
    dmHashTable(const dmHashTable<KEY, T>&);
    const dmHashTable<KEY, T>& operator=(const dmHashTable<KEY, T>&);

    template <typename KEY2, typename T2>
    static void FillCallback(dmHashTable<KEY2,T2> *ht, const KEY2* key, T2* value)
    {
        ht->Put(*key, *value);
    }

    Entry* FindEntry(KEY key) const
    {
        // Avoid module division by zero
        if (!m_HashTableSize)
            return 0;

        uint32_t bucket_index = (uint32_t) (key % m_HashTableSize);
        uint32_t bucket = m_HashTable[bucket_index];

        uint32_t entry_ptr = bucket;
        while (entry_ptr != 0xffffffff)
        {
            Entry* e = &m_InitialEntries[entry_ptr];
            if (e->m_Key == key)
            {
                return e;
            }
            entry_ptr = e->m_Next;
        }
        return 0;
    }

    Entry* AllocateEntry()
    {
        // Free entries in the initial "depot"?
        if (m_InitialEntriesNextFree != m_InitialEntriesEnd)
        {
            return m_InitialEntriesNextFree++;
        }
        else
        {
            // No, pick an entry from the free list.
            assert(m_FreeEntries != 0xffffffff && "No free entries in hashtable");

            Entry*ret = &m_InitialEntries[m_FreeEntries];
            m_FreeEntries = ret->m_Next;

            return ret;
        }
    }

    void FreeEntry(Entry* e)
    {
        // Empty list of entries?
        if (m_FreeEntries == 0xffffffff)
        {
            m_FreeEntries = (uint32_t)(uintptr_t)(e - m_InitialEntries);
            e->m_Next = 0xffffffff;
        }
        else
        {
            e->m_Next = m_FreeEntries;
            m_FreeEntries = (uint32_t)(uintptr_t)(e - m_InitialEntries);
        }
    }

    // The actual hash table
    uint32_t* m_HashTable;
    uint32_t  m_HashTableSize;

    // Pointer to all entries
    Entry*    m_InitialEntries;
    // Pointer to next free entry
    Entry*    m_InitialEntriesNextFree;
    // Pointer to last entry - exclusive
    Entry*    m_InitialEntriesEnd;

    // Linked list of free entries.
    uint32_t  m_FreeEntries;

    // Number of key/value pairs in table
    uint32_t  m_Count;

    // state flags/info
    uint16_t m_State : 1;
};


/*#
 * Specialized hash table with [type:uint16_t] as keys
 * @type class
 * @name dmHashTable16
 */
template <typename T>
class dmHashTable16 : public dmHashTable<uint16_t, T> {};


/*#
 * Specialized hash table with [type:uint32_t] as keys
 * @type class
 * @name dmHashTable32
 */
template <typename T>
class dmHashTable32 : public dmHashTable<uint32_t, T> {};

/*#
 * Specialized hash table with [type:uint64_t] as keys
 * @type class
 * @name dmHashTable64
 */
template <typename T>
class dmHashTable64 : public dmHashTable<uint64_t, T> {};

#endif // DMSDK_HASHTABLE_H
