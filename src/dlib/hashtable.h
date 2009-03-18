#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/**
 * Hashtable with memcpy-copy semantics.
 * Only uint16_t, uint32_t and uint64_t is supported as KEY type.
 */
template <typename KEY, typename T>
    class THashTable
{
public:
    struct Entry
    {
        uint16_t m_Next;
        KEY      m_Key;
        T        m_Value;
    };

    /**
     * Constructor. Create an empty hashtable with zero capacity and zero hashtable (buckets)
     */
    THashTable()
    {
        memset(this, 0, sizeof(*this));
        m_FreeEntries = 0xffff;
    }

    /**
     * Constructor.
     * @param table_size Hashtable size, ie number of buckets.
     */
    THashTable(uint32_t table_size)
    {
        assert(table_size > 0);
        assert(table_size < 0xffff);

        memset(this, 0, sizeof(*this));

        m_HashTableSize = table_size;
        m_HashTable = (uint16_t*) malloc(sizeof(uint16_t) * table_size);
        memset(m_HashTable, 0xff, sizeof(uint16_t) * table_size);

        m_FreeEntries = 0xffff;
    }

    ~THashTable()
    {
        if (m_InitialEntries)
        {
            free(m_InitialEntries);
        }
    }

    /**
     * Number of entries stored in table. (not the actual hashtable size)
     * @return Number of entries.
     */
    uint16_t Size()
    {
        return m_Count;
    }

    /**
     * Hashtable capacity. Maximum number of entries possible to store in table
     * @return Capcity
     */
    uint16_t Capacity()
    {
        return m_InitialEntriesEnd - m_InitialEntries;
    }

    /**
     * Set new hashtable capacity. Only valid to run once initially.
     * @param capacity Capacity
     */
    void SetCapacity(uint16_t capacity)
    {
        assert(m_InitialEntries == 0);
        m_InitialEntries = (Entry*) malloc(sizeof(Entry) * capacity);
        m_InitialEntriesNextFree = m_InitialEntries;
        m_InitialEntriesEnd = m_InitialEntries + capacity;
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
    void     Put(KEY key, const T& value)
    {
        assert(!Full());
        Entry* entry = FindEntry(key);

        // Key already in table?
        if (entry != 0)
        {
            entry->m_Value = value;
        }
        else
        {
            entry = AllocateEntry();
            entry->m_Key = key;
            entry->m_Value = value;
            entry->m_Next = 0xffff;

            uint16_t bucket_index = key % m_HashTableSize;
            uint16_t entry_ptr = m_HashTable[bucket_index];
            if (entry_ptr == 0xffff)
            {
                m_HashTable[bucket_index] = entry - m_InitialEntries; // Store the index of the entry
            }
            else
            {
                // We need to traverse the list of entries for the bucket
                Entry* prev_entry;
                while (entry_ptr != 0xffff)
                {
                    prev_entry = &m_InitialEntries[entry_ptr];
                    entry_ptr = prev_entry->m_Next;
                }
                assert(prev_entry->m_Next == 0xffff);

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
     * Remove key/value pair. NOTE: Only valid if key exists in table.
     * @param key Key to remove
     */
    void Erase(KEY key)
    {
        uint16_t bucket_index = key % m_HashTableSize;
        uint16_t entry_ptr = m_HashTable[bucket_index];

        // Empty list?
        assert(entry_ptr != 0xffff);

        Entry* prev_e = 0;
        while (entry_ptr != 0xffff)
        {
            Entry* e = &m_InitialEntries[entry_ptr];
            if (e->m_Key == key)
            {
                --m_Count;
                // The entry first in list
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
     * Verify internal structuty. "assert" if invalid.
     */
    void Verify()
    {
        uint16_t free_ptr = m_FreeEntries;
        while (free_ptr != 0xffff)
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
    }

private:
    Entry* FindEntry(KEY key)
    {
        uint16_t bucket_index = key % m_HashTableSize;
        uint16_t bucket = m_HashTable[bucket_index];

        // Empty list?
        if (bucket == 0xffff)
        {
            return 0;
        }
        else
        {
            uint16_t entry_ptr = bucket;
            while (entry_ptr != 0xffff)
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
            assert(m_FreeEntries != 0xffff && "No free entries in hashtable");

            Entry*ret = &m_InitialEntries[m_FreeEntries];

            // Last free?
            if (ret->m_Next == 0xffff)
            {
                m_FreeEntries = 0xffff;
            }
            else
            {
                m_FreeEntries = ret->m_Next;
            }

            return ret;
        }
    }

    void FreeEntry(Entry* e)
    {
        // Empty list of entries?
        if (m_FreeEntries == 0xffff)
        {
            m_FreeEntries = e - m_InitialEntries;
            e->m_Next = 0xffff;
        }
        else
        {
            e->m_Next = m_FreeEntries;
            m_FreeEntries = e - m_InitialEntries;
        }
    }

    // The actual hash table
    uint16_t* m_HashTable;
    uint16_t  m_HashTableSize;

    // Pointer to all entries
    Entry*    m_InitialEntries;
    // Pointer to next free entry
    Entry*    m_InitialEntriesNextFree;
    // Pointer to last entry - exclusive
    Entry*    m_InitialEntriesEnd;

    // Linked list of free entries.
    uint16_t  m_FreeEntries;

    // Number of key/value pairs in table
    uint16_t  m_Count;
};

template <typename T>
class THashTable16 : public THashTable<uint16_t, T> {};

template <typename T>
class THashTable32 : public THashTable<uint32_t, T> {};

template <typename T>
class THashTable64 : public THashTable<uint64_t, T> {};

#endif // HASHTABLE_H

