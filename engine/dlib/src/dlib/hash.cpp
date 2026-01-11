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

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "dalloca.h"
#include "dlib.h"
#include "hash.h"
#include "array.h"
#include "index_pool.h"
#include "align.h"
#include <dlib/dstrings.h>
#include <dlib/mutex.h>
#include <dlib/hashtable.h>

struct ReverseHashEntry
{
    inline ReverseHashEntry(void* value, uint32_t length) : m_Value(value), m_Length (length) {}
    void*    m_Value;
    uint16_t m_Length;
};

template<typename TABLE>
static void IncreaseTableCapacity(TABLE* table, uint32_t increment)
{
    uint32_t capacity = table->Capacity() + increment;
    table->SetCapacity((capacity*5)/7, capacity);
}

struct ReverseHashContainer
{
    static const size_t m_HashTableCapacityIncrement = 16 * 1024;
    static const size_t m_HashStatesCapacity = 512;
    static const size_t m_HashStatesCapacityIncrement = 256;

    dmMutex::HMutex                 m_Mutex;
    bool                            m_Enabled;
    dmHashTable32<ReverseHashEntry> m_HashTable32Entries;
    dmHashTable64<ReverseHashEntry> m_HashTable64Entries;
    dmArray<ReverseHashEntry>       m_HashStates;
    dmIndexPool32                   m_HashStatesSlots;

    ReverseHashContainer()
    {
        m_Mutex = dmMutex::New();
        m_Enabled = false;
    }

    ~ReverseHashContainer()
    {
        Enable(false);
        dmMutex::Delete(m_Mutex);
    }

    template <typename KEY>
    static inline void FreeEntryCallback(void* context, const KEY* key, ReverseHashEntry* value)
    {
        free(value->m_Value);
    }

    template <typename INDEX>
    static inline void FreeStateCallback(void* context, const INDEX index)
    {
        dmArray<ReverseHashEntry>& states = *((dmArray<ReverseHashEntry>*) context);
        states[index].m_Value = 0;
    }

    void Enable(bool enable)
    {
        if(m_Enabled == enable)
            return;
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        m_Enabled = enable;

        if(enable)
        {
            if (m_HashTable32Entries.Full())
                IncreaseTableCapacity(&m_HashTable32Entries, m_HashTableCapacityIncrement);
            if (m_HashTable64Entries.Full())
                IncreaseTableCapacity(&m_HashTable64Entries, m_HashTableCapacityIncrement);
            m_HashTable64Entries.Clear();
            m_HashStates.SetCapacity(m_HashStatesCapacity);
            m_HashStates.SetSize(m_HashStatesCapacity);
            m_HashStatesSlots.SetCapacity(m_HashStatesCapacity);
            m_HashStatesSlots.Clear();
            uint32_t invalid_slot = m_HashStatesSlots.Pop();
            assert(invalid_slot == 0);  // we rely on first index to be 0 in the index pool implementation. 0 implies invalid/unused slot.
        }
        else
        {
            m_HashTable32Entries.Iterate(FreeEntryCallback, (void*) 0);
            m_HashTable32Entries.Clear();
            m_HashTable64Entries.Iterate(FreeEntryCallback, (void*) 0);
            m_HashTable64Entries.Clear();
            if(m_HashStatesSlots.Size() != 0)
            {
                m_HashStatesSlots.Push(0);
                m_HashStatesSlots.IterateRemaining(FreeStateCallback, (void*) &m_HashStates);
                for(uint32_t i = 0; i < m_HashStates.Size(); ++i)
                {
                    if(m_HashStates[i].m_Value)
                    {
                        free(m_HashStates[i].m_Value);
                    }
                }
                m_HashStatesSlots.Clear();
            }
        }
    }

    inline uint32_t AllocReverseHashStatesSlot()
    {
        if(m_HashStatesSlots.Remaining() == 0)
        {
            m_HashStatesSlots.SetCapacity(m_HashStatesSlots.Capacity() + m_HashStatesCapacityIncrement);
            m_HashStates.SetCapacity(m_HashStates.Capacity() + m_HashStatesCapacityIncrement);
            m_HashStates.SetSize(m_HashStates.Capacity());
        }
        return m_HashStatesSlots.Pop();
    }

    inline void FreeReverseHashStatesSlot(uint32_t slot_index)
    {
        assert(slot_index != 0);
        m_HashStatesSlots.Push(slot_index);
    }

    inline void CloneReverseHashState(uint32_t state_index, uint32_t source_state_index)
    {
        assert(state_index != 0);
        ReverseHashEntry& state = m_HashStates[state_index];
        ReverseHashEntry& source_state = m_HashStates[source_state_index];
        uint32_t length = source_state.m_Length;
        state.m_Value = malloc(DM_ALIGN(length + 1, 16));
        uint8_t* p = (uint8_t*) state.m_Value;
        memcpy(p, source_state.m_Value, length);
        p[length] = '\0';
        state.m_Length = length;
    }

    inline void UpdateReversHashState(uint32_t state_index, uint32_t len, const void* buffer, uint32_t buffer_len)
    {
        assert(state_index != 0);
        ReverseHashEntry& entry = m_HashStates[state_index];
        size_t length = entry.m_Length + buffer_len;
        entry.m_Value = realloc(entry.m_Value, DM_ALIGN(length + 1, 16) + 16);
        uint8_t* p = (uint8_t*) entry.m_Value;
        memcpy(&p[entry.m_Length], buffer, buffer_len);
        p[length] = '\0';
        entry.m_Length = length;
    }

};

static inline ReverseHashContainer& dmHashContainer()
{
    // DEF-3677 The hash functions are in some cases called from static initializers
    // and we need to make sure the ReverseHashContainer initializer is called before
    // it is accessed via other static hash functions.
    static ReverseHashContainer dmHashContainerPrivate;
    return dmHashContainerPrivate;
}

void dmHashEnableReverseHash(bool enable)
{
    dmHashContainer().Enable(enable);
}

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

// Based on MurmurHash2A but endian neutral
uint32_t dmHashBufferNoReverse32(const void* key, uint32_t len)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t l = len;

    const unsigned char * data = (const unsigned char *)key;

    uint32_t h = /*seed*/ 0;

    while(len >= 4)
    {
        uint32_t k;

        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        mmix(h,k);

        data += 4;
        len -= 4;
    }

    uint32_t t = 0;

    switch(len)
    {
    case 3: t ^= data[2] << 16;
    case 2: t ^= data[1] << 8;
    case 1: t ^= data[0];
    };

    mmix(h,t);
    mmix(h,l);

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

uint32_t dmHashBuffer32(const void * key, uint32_t len)
{
    uint32_t h = dmHashBufferNoReverse32(key, len);

    if (dmHashContainer().m_Enabled && len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        dmHashTable32<ReverseHashEntry>* hash_table = &dmHashContainer().m_HashTable32Entries;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                IncreaseTableCapacity(hash_table, dmHashContainer().m_HashTableCapacityIncrement);
            }
            char* copy = (char*) malloc(len + 1);
            memcpy(copy, key, len);
            copy[len] = '\0';
            hash_table->Put(h, ReverseHashEntry(copy, len));
        }
    }

    return h;
}

// Based on MurmurHash2A but endian neutral
uint64_t dmHashBufferNoReverse64(const void * key, uint32_t len)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    uint64_t l = len;

    const unsigned char * data = (const unsigned char *)key;

    uint64_t h = /*seed*/ 0;

    while(len >= 8)
    {
        uint64_t k;

        k  = uint64_t(data[0]);
        k |= uint64_t(data[1]) << 8;
        k |= uint64_t(data[2]) << 16;
        k |= uint64_t(data[3]) << 24;
        k |= uint64_t(data[4]) << 32;
        k |= uint64_t(data[5]) << 40;
        k |= uint64_t(data[6]) << 48;
        k |= uint64_t(data[7]) << 56;

        mmix(h,k);

        data += 8;
        len -= 8;
    }

    uint64_t t = 0;

    switch(len)
    {
    case 7: t ^= uint64_t(data[6]) << 48;
    case 6: t ^= uint64_t(data[5]) << 40;
    case 5: t ^= uint64_t(data[4]) << 32;
    case 4: t ^= uint64_t(data[3]) << 24;
    case 3: t ^= uint64_t(data[2]) << 16;
    case 2: t ^= uint64_t(data[1]) << 8;
    case 1: t ^= uint64_t(data[0]);
    };

    mmix(h,t);
    mmix(h,l);

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

uint64_t dmHashBuffer64(const void * key, uint32_t len)
{
    uint64_t h = dmHashBufferNoReverse64(key, len);

    if (dmHashContainer().m_Enabled && len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        dmHashTable64<ReverseHashEntry>* hash_table = &dmHashContainer().m_HashTable64Entries;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                IncreaseTableCapacity(hash_table, dmHashContainer().m_HashTableCapacityIncrement);
            }
            char* copy = (char*) malloc(len + 1);
            memcpy(copy, key, len);
            copy[len] = '\0';
            hash_table->Put(h, ReverseHashEntry(copy, len));
        }
    }

    return h;
}

uint32_t DM_DLLEXPORT dmHashString32(const char* string)
{
    return dmHashBuffer32(string, strlen(string));
}

uint64_t DM_DLLEXPORT dmHashString64(const char* string)
{
    return dmHashBuffer64(string, strlen(string));
}

// Based on CMurmurHash2A
void dmHashInit32(HashState32* hash_state, bool reverse_hash)
{
    memset(hash_state, 0x0, sizeof(HashState32));
    if(reverse_hash && dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        uint32_t new_index = hash_state->m_ReverseHashEntryIndex = dmHashContainer().AllocReverseHashStatesSlot();
        memset(&dmHashContainer().m_HashStates[new_index], 0x0, sizeof(ReverseHashEntry));
    }
}

void dmHashClone32(HashState32* hash_state, const HashState32* source_hash_state, bool reverse_hash)
{
    memcpy(hash_state, source_hash_state, sizeof(HashState32));
    if(dmHashContainer().m_Enabled && source_hash_state->m_ReverseHashEntryIndex)
    {
        if(reverse_hash)
        {
            DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
            uint32_t new_index = hash_state->m_ReverseHashEntryIndex = dmHashContainer().AllocReverseHashStatesSlot();
            dmHashContainer().CloneReverseHashState(new_index, source_hash_state->m_ReverseHashEntryIndex);
        }
        else
        {
            hash_state->m_ReverseHashEntryIndex = 0;
        }
    }
}

static void MixTail32(HashState32* hash_state, const unsigned char * & data, int & len)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    while( len && ((len<4) || hash_state->m_Count) )
    {
        uint32_t d = *data++;
        hash_state->m_Tail |= d << uint32_t(hash_state->m_Count * 8);

        hash_state->m_Count++;
        len--;

        if(hash_state->m_Count == 4)
        {
            mmix(hash_state->m_Hash, hash_state->m_Tail);
            hash_state->m_Tail = 0;
            hash_state->m_Count = 0;
        }
    }
}

void dmHashUpdateBuffer32(HashState32* hash_state, const void* buffer, uint32_t buffer_len)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    const unsigned char * data = (const unsigned char *) buffer;
    int len = (int) buffer_len;

    hash_state->m_Size += len;

    MixTail32(hash_state, data, len);

    while(len >= 4)
    {
        uint32_t k;
        k  = data[0];
        k |= data[1] << 8;
        k |= data[2] << 16;
        k |= data[3] << 24;

        mmix(hash_state->m_Hash,k);

        data += 4;
        len -= 4;
    }

    MixTail32(hash_state, data, len);
    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmHashContainer().UpdateReversHashState(hash_state->m_ReverseHashEntryIndex, hash_state->m_Size, buffer, buffer_len);
    }
}

uint32_t dmHashFinal32(HashState32* hash_state)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;
    uint32_t s = hash_state->m_Size;
    mmix(hash_state->m_Hash, hash_state->m_Tail);
    mmix(hash_state->m_Hash, s);

    hash_state->m_Hash ^= hash_state->m_Hash >> 13;
    hash_state->m_Hash *= m;
    hash_state->m_Hash ^= hash_state->m_Hash >> 15;

    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        dmHashTable32<ReverseHashEntry>* hash_table = &dmHashContainer().m_HashTable32Entries;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                IncreaseTableCapacity(hash_table, dmHashContainer().m_HashTableCapacityIncrement);
            }
            hash_table->Put(hash_state->m_Hash, dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex]);
        }
        else
        {
            free(dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        }
        dmHashContainer().FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }

    return hash_state->m_Hash;
}

void dmHashRelease32(HashState32* hash_state)
{
    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        free(dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        dmHashContainer().FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }
}

// Based on CMurmurHash2A
void dmHashInit64(HashState64* hash_state, bool reverse_hash)
{
    memset(hash_state, 0x0, sizeof(HashState64));
    if(reverse_hash && dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        uint32_t new_index = hash_state->m_ReverseHashEntryIndex = dmHashContainer().AllocReverseHashStatesSlot();
        memset(&dmHashContainer().m_HashStates[new_index], 0x0, sizeof(ReverseHashEntry));
    }
}

void dmHashClone64(HashState64* hash_state, const HashState64* source_hash_state, bool reverse_hash)
{
    memcpy(hash_state, source_hash_state, sizeof(HashState64));
    if(dmHashContainer().m_Enabled && source_hash_state->m_ReverseHashEntryIndex)
    {
        if(reverse_hash)
        {
            DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
            uint32_t new_index = hash_state->m_ReverseHashEntryIndex = dmHashContainer().AllocReverseHashStatesSlot();
            dmHashContainer().CloneReverseHashState(new_index, source_hash_state->m_ReverseHashEntryIndex);
        }
        else
        {
            hash_state->m_ReverseHashEntryIndex = 0;
        }
    }
}

static void MixTail64(HashState64* hash_state, const unsigned char * & data, int & len)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    while( len && ((len<8) || hash_state->m_Count) )
    {
        uint64_t d = *data++;
        hash_state->m_Tail |= d << uint64_t(hash_state->m_Count * 8);

        hash_state->m_Count++;
        len--;

        if(hash_state->m_Count == 8)
        {
            mmix(hash_state->m_Hash, hash_state->m_Tail);
            hash_state->m_Tail = 0;
            hash_state->m_Count = 0;
        }
    }
}

void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint32_t buffer_len)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    const unsigned char * data = (const unsigned char *) buffer;
    int len = (int) buffer_len;

    hash_state->m_Size += len;

    MixTail64(hash_state, data, len);

    while(len >= 8)
    {
        uint64_t k;
        k  = uint64_t(data[0]);
        k |= uint64_t(data[1]) << 8;
        k |= uint64_t(data[2]) << 16;
        k |= uint64_t(data[3]) << 24;
        k |= uint64_t(data[4]) << 32;
        k |= uint64_t(data[5]) << 40;
        k |= uint64_t(data[6]) << 48;
        k |= uint64_t(data[7]) << 56;

        mmix(hash_state->m_Hash, k);

        data += 8;
        len -= 8;
    }

    MixTail64(hash_state, data, len);
    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmHashContainer().UpdateReversHashState(hash_state->m_ReverseHashEntryIndex, hash_state->m_Size, buffer, buffer_len);
    }
}

uint64_t dmHashFinal64(HashState64* hash_state)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64_t s = hash_state->m_Size;
    mmix(hash_state->m_Hash, hash_state->m_Tail);
    mmix(hash_state->m_Hash, s);

    hash_state->m_Hash ^= hash_state->m_Hash >> r;
    hash_state->m_Hash *= m;
    hash_state->m_Hash ^= hash_state->m_Hash >> r;

    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        dmHashTable64<ReverseHashEntry>* hash_table = &dmHashContainer().m_HashTable64Entries;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                IncreaseTableCapacity(hash_table, dmHashContainer().m_HashTableCapacityIncrement);
            }
            hash_table->Put(hash_state->m_Hash, dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex]);
        }
        else
        {
            free(dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        }
        dmHashContainer().FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }

    return hash_state->m_Hash;
}

void dmHashRelease64(HashState64* hash_state)
{
    if (dmHashContainer().m_Enabled && hash_state->m_ReverseHashEntryIndex)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        free(dmHashContainer().m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        dmHashContainer().FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }
}

DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable32Entries.Get(hash);
        if (reverse)
        {
            if (length)
            {
                *length = reverse->m_Length;
            }
            return reverse->m_Value;
        }
    }
    return 0;
}

DM_DLLEXPORT const void* dmHashReverse32Alloc(dmAllocator* allocator, uint32_t hash, uint32_t* length)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable32Entries.Get(hash);
        if (reverse)
        {
            if (length)
            {
                *length = reverse->m_Length;
            }

            uint8_t* out = (uint8_t*)dmMemAlloc(allocator, reverse->m_Length+1);
            if (out)
            {
                memcpy(out, reverse->m_Value, reverse->m_Length);
                out[reverse->m_Length] = 0; // make sure it's null terminated
            }
            return out;
        }
    }
    return 0;
}

DM_DLLEXPORT const void* dmHashReverse64(uint64_t hash, uint32_t* length)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable64Entries.Get(hash);
        if (reverse)
        {
            if (length)
            {
                *length = reverse->m_Length;
            }
            return reverse->m_Value;
        }
    }
    return 0;
}

DM_DLLEXPORT const void* dmHashReverse64Alloc(dmAllocator* allocator, uint64_t hash, uint32_t* length)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable64Entries.Get(hash);
        if (reverse)
        {
            if (length)
            {
                *length = reverse->m_Length;
            }

            uint8_t* out = (uint8_t*)dmMemAlloc(allocator, reverse->m_Length+1);
            if (out)
            {
                memcpy(out, reverse->m_Value, reverse->m_Length);
                out[reverse->m_Length] = 0; // make sure it's null terminated
            }
            return out;
        }
    }
    return 0;
}

DM_DLLEXPORT void dmHashReverseErase32(uint32_t hash)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable32Entries.Get(hash);
        if (reverse)
        {
            free(reverse->m_Value);
            dmHashContainer().m_HashTable32Entries.Erase(hash);
        }
    }
}

DM_DLLEXPORT void dmHashReverseErase64(uint64_t hash)
{
    if (dmHashContainer().m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(dmHashContainer().m_Mutex);
        ReverseHashEntry* reverse = dmHashContainer().m_HashTable64Entries.Get(hash);
        if (reverse)
        {
            free(reverse->m_Value);
            dmHashContainer().m_HashTable64Entries.Erase(hash);
        }
    }
}

// Deprecated
DM_DLLEXPORT const char* dmHashReverseSafe64(uint64_t hash)
{
    const char* s = (const char*)dmHashReverse64(hash, 0);
    return s != 0 ? s : "<unknown>";
}
// Deprecated
DM_DLLEXPORT const char* dmHashReverseSafe32(uint32_t hash)
{
    const char* s = (const char*)dmHashReverse32(hash, 0);
    return s != 0 ? s : "<unknown>";
}

#if defined(DM_PLATFORM_VENDOR)
    #include <dmsdk/dlib/hash_vendor.h>
#elif defined(__linux__) && !defined(__ANDROID__)
    #define DM_HASH_LONG_FMT "%lu"
#else
    #define DM_HASH_LONG_FMT "%llu"
#endif

DM_DLLEXPORT const char* dmHashReverseSafe64Alloc(dmAllocator* allocator, uint64_t hash)
{
    uint32_t length = 0;
    const char* reverse = (const char*)dmHashReverse64Alloc(allocator, hash, &length);
    if (reverse)
    {
        return reverse;
    }

    const uint32_t out_length = 31; // unknown + <> + count(18446744073709551615) + 1 = 8 + 2 + 20 + 1 = 31
    char* out = (char*)dmMemAlloc(allocator, out_length);
    if (!out)
        return "<unknown>";

    dmSnPrintf(out, out_length, "<unknown:" DM_HASH_LONG_FMT ">", hash);
    return out;
}

#undef DM_HASH_LONG_FMT

DM_DLLEXPORT const char* dmHashReverseSafe32Alloc(dmAllocator* allocator, uint32_t hash)
{
    uint32_t length = 0;
    const char* reverse = (const char*)dmHashReverse32Alloc(allocator, hash, &length);
    if (reverse)
        return reverse;

    const uint32_t out_length = 21; // unknown + <> + count(4294967295) + 1 = 8 + 2 + 10 + 1 = 21
    char* out = (char*)dmMemAlloc(allocator, out_length);
    if (!out)
        return "<unknown>";
    dmSnPrintf(out, out_length, "<unknown:%u>", hash);
    return out;
}
