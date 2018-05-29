#ifndef DM_HASHTABLE_H
#define DM_HASHTABLE_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * Hashtable with chaining for collision resolution, memcpy-copy semantics and 32-bit indicies instead of pointers. (NUMA-friendly)
 * Only uint16_t, uint32_t and uint64_t is supported as KEY type.
 */
template <typename KEY, typename T>
class dmHashTable
{
    enum STATE_FLAGS
    {
        STATE_DEFAULT           = 0x0,
        STATE_USER_ALLOCATED    = 0x1
    };

public:
    struct Entry
    {
        KEY      m_Key;
        T        m_Value;
        uint32_t m_Next;
    };

    /**
     * Constructor. Create an empty hashtable with zero capacity and zero hashtable (buckets)
     */
    dmHashTable()
    {
        memset(this, 0, sizeof(*this));
        m_FreeEntries = 0xffffffff;
    }

    /**
     * Creates a hashtable array with user allocated memory. User allocated arrays can not change capacity.
     * @param user_allocated Pointer to user allocated continous data-block ((table_size*sizeof(uint32_t)) + (capacity*sizeof(dmHashTable::Entry))
     * @param table_size Hashtable size, ie number of buckets. table_size < 0xffffffff
     * @param capacity Capacity. capacity < 0xffffffff
     */
    dmHashTable(void *user_allocated, uint32_t table_size, uint32_t capacity)
    {
        assert(table_size < 0xffffffff);
        assert(capacity < 0xffffffff);
        memset(this, 0, sizeof(*this));
        m_FreeEntries = 0xffffffff;

        m_HashTableSize = table_size;
        m_HashTable = (uint32_t*) user_allocated;
        memset(m_HashTable, 0xff, sizeof(uint32_t) * table_size);

        m_InitialEntries = (Entry*) (m_HashTable + table_size);
        m_InitialEntriesNextFree = m_InitialEntries;
        m_InitialEntriesEnd = m_InitialEntries + capacity;
        m_State = STATE_USER_ALLOCATED;
    }

    void Clear()
    {
        memset(m_HashTable, 0xff, sizeof(uint32_t) * m_HashTableSize);
        m_InitialEntriesNextFree = m_InitialEntries;
        m_FreeEntries = 0xffffffff;
        m_Count = 0;
    }

    /**
     * Destructor.
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
     * @return Number of entries.
     */
    uint32_t Size()
    {
        return m_Count;
    }

    /**
     * Hashtable capacity. Maximum number of entries possible to store in table
     * @return Capcity
     */
    uint32_t Capacity()
    {
        return m_InitialEntriesEnd - m_InitialEntries;
    }

    /**
     * Set hashtable capacity. New capacity must be greater or equal to current capacity
     * @param table_size Hashtable size, ie number of buckets. table_size < 0xffffffff
     * @param capacity Capacity. capacity < 0xffffffff
     */
    void SetCapacity(uint32_t table_size, uint32_t capacity)
    {
        assert(table_size > 0);
        assert(table_size < 0xffffffff);
        assert(capacity < 0xffffffff);
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

    void Swap(dmHashTable<KEY, T>& other)
    {
        char buf[sizeof(*this)];
        memcpy(buf, &other, sizeof(buf));
        memcpy(&other, this, sizeof(buf));
        memcpy(this, buf, sizeof(buf));
    }

    /**
     * Check if the table is full
     * @return true if the table is full
     */
    bool Full()
    {
        return m_Count == Capacity();
    }

    /**
     * Check if the table is empty
     * @return true if the table is empty
     */
    bool Empty()
    {
        return m_Count == 0;
    }

    /**
     * Put key/value pair in hash table. NOTE: The method will "assert" if the hashtable is full.
     * @param key Key
     * @param value Value
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
            entry->m_Next = 0xffffffff;

            uint32_t bucket_index = (uint32_t) (key % m_HashTableSize);
            uint32_t entry_ptr = m_HashTable[bucket_index];
            if (entry_ptr == 0xffffffff)
            {
                m_HashTable[bucket_index] = entry - m_InitialEntries; // Store the index of the entry
            }
            else
            {
                // We need to traverse the list of entries for the bucket
                Entry* prev_entry;
                while (entry_ptr != 0xffffffff)
                {
                    prev_entry = &m_InitialEntries[entry_ptr];
                    entry_ptr = prev_entry->m_Next;
                }
                assert(prev_entry->m_Next == 0xffffffff);

                // Link prev entry to this
                prev_entry->m_Next = entry - m_InitialEntries;
            }
        }

        m_Count++;
    }

    /**
     * Get pointer to value from key
     * @param key Key
     * @return Pointer to value. NULL if the key/value pair doesn't exists.
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
     * @param key Key
     * @return Pointer to value. NULL if the key/value pair doesn't exists.
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
    Entry* GetFirstEntry()
    {
        if(Empty())
            return 0;
        return m_InitialEntries;

    }

    /**
     * Remove key/value pair. NOTE: Only valid if key exists in table.
     * @param key Key to remove
     */
    void Erase(KEY key)
    {
        // Avoid module division by zero
        assert(m_HashTableSize != 0);

        uint32_t bucket_index = key % m_HashTableSize;
        uint32_t entry_ptr = m_HashTable[bucket_index];

        // Empty list?
        assert(entry_ptr != 0xffffffff);

        Entry* prev_e = 0;
        while (entry_ptr != 0xffffffff)
        {
            Entry* e = &m_InitialEntries[entry_ptr];
            if (e->m_Key == key)
            {
                --m_Count;
                // The entry first in list?
                if (prev_e == 0)
                {
                    // Relink
                    m_HashTable[bucket_index] = e->m_Next;;
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
     * @param call_back Call-back called for every entry
     * @param context Context
     */
    template <typename CONTEXT>
    void Iterate(void (*call_back)(CONTEXT *context, const KEY* key, T* value), CONTEXT* context)
    {
        for (uint32_t i = 0; i < m_HashTableSize; ++i)
        {
            if (m_HashTable[i] != 0xffffffff)
            {
                uint32_t entry_ptr = m_HashTable[i];
                while (entry_ptr != 0xffffffff)
                {
                    Entry*e = &m_InitialEntries[entry_ptr];
                    call_back(context, &e->m_Key, &e->m_Value);
                    entry_ptr = e->m_Next;
                }
            }
        }
    }

    /**
     * Verify internal structure. "assert" if invalid.
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

    Entry* FindEntry(KEY key)
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
            m_FreeEntries = e - m_InitialEntries;
            e->m_Next = 0xffffffff;
        }
        else
        {
            e->m_Next = m_FreeEntries;
            m_FreeEntries = e - m_InitialEntries;
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

template <typename T>
class dmHashTable16 : public dmHashTable<uint16_t, T> {};

template <typename T>
class dmHashTable32 : public dmHashTable<uint32_t, T> {};

template <typename T>
class dmHashTable64 : public dmHashTable<uint64_t, T> {};

#endif // DM_HASHTABLE_H

