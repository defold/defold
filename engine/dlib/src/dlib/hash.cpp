#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "dlib.h"
#include "hash.h"
#include "mutex.h"
#include "hashtable.h"
#include "array.h"
#include "index_pool.h"
#include "align.h"

struct ReverseHashEntry
{
    inline ReverseHashEntry(void* value, uint32_t length) : m_Value(value), m_Length (length) {}
    void*    m_Value;
    uint16_t m_Length;
};

static struct ReverseHashContainer
{
    static const size_t m_HashTableSize = 1024;
    static const size_t m_HashTableCapacity = 512;
    static const size_t m_HashTableCapacityIncrement = 256;
    static const size_t m_HashStatesCapacity = 512;
    static const size_t m_HashStatesCapacityIncrement = 256;

    dmMutex::Mutex                  m_Mutex;
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

            if(m_HashTable32Entries.Capacity() < m_HashTableCapacity)
                m_HashTable32Entries.SetCapacity(m_HashTableSize, m_HashTableCapacity);
            m_HashTable32Entries.Clear();
            if(m_HashTable64Entries.Capacity() < m_HashTableCapacity)
                m_HashTable64Entries.SetCapacity(m_HashTableSize, m_HashTableCapacity);
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

} g_dmHashContainer;

void dmHashEnableReverseHash(bool enable)
{
    g_dmHashContainer.Enable(enable);
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

    if (g_dmHashContainer.m_Enabled && len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        dmHashTable32<ReverseHashEntry>* hash_table = &g_dmHashContainer.m_HashTable32Entries;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(g_dmHashContainer.m_HashTableSize, hash_table->Capacity() + g_dmHashContainer.m_HashTableCapacityIncrement);
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

    if (g_dmHashContainer.m_Enabled && len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        dmHashTable64<ReverseHashEntry>* hash_table = &g_dmHashContainer.m_HashTable64Entries;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(g_dmHashContainer.m_HashTableSize, hash_table->Capacity() + g_dmHashContainer.m_HashTableCapacityIncrement);
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
    if(reverse_hash && g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        uint32_t new_index = hash_state->m_ReverseHashEntryIndex = g_dmHashContainer.AllocReverseHashStatesSlot();
        memset(&g_dmHashContainer.m_HashStates[new_index], 0x0, sizeof(ReverseHashEntry));
    }
}

void dmHashClone32(HashState32* hash_state, const HashState32* source_hash_state, bool reverse_hash)
{
    memcpy(hash_state, source_hash_state, sizeof(HashState32));
    if(g_dmHashContainer.m_Enabled && source_hash_state->m_ReverseHashEntryIndex)
    {
        if(reverse_hash)
        {
            DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
            uint32_t new_index = hash_state->m_ReverseHashEntryIndex = g_dmHashContainer.AllocReverseHashStatesSlot();
            g_dmHashContainer.CloneReverseHashState(new_index, source_hash_state->m_ReverseHashEntryIndex);
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
    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        g_dmHashContainer.UpdateReversHashState(hash_state->m_ReverseHashEntryIndex, hash_state->m_Size, buffer, buffer_len);
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

    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        dmHashTable32<ReverseHashEntry>* hash_table = &g_dmHashContainer.m_HashTable32Entries;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(g_dmHashContainer.m_HashTableSize, hash_table->Capacity() + g_dmHashContainer.m_HashTableCapacityIncrement);
            }
            hash_table->Put(hash_state->m_Hash, g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex]);
        }
        else
        {
            free(g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        }
        g_dmHashContainer.FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }

    return hash_state->m_Hash;
}

void dmHashRelease32(HashState32* hash_state)
{
    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        free(g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        g_dmHashContainer.FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }
}

// Based on CMurmurHash2A
void dmHashInit64(HashState64* hash_state, bool reverse_hash)
{
    memset(hash_state, 0x0, sizeof(HashState64));
    if(reverse_hash && g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        uint32_t new_index = hash_state->m_ReverseHashEntryIndex = g_dmHashContainer.AllocReverseHashStatesSlot();
        memset(&g_dmHashContainer.m_HashStates[new_index], 0x0, sizeof(ReverseHashEntry));
    }
}

void dmHashClone64(HashState64* hash_state, const HashState64* source_hash_state, bool reverse_hash)
{
    memcpy(hash_state, source_hash_state, sizeof(HashState64));
    if(g_dmHashContainer.m_Enabled && source_hash_state->m_ReverseHashEntryIndex)
    {
        if(reverse_hash)
        {
            DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
            uint32_t new_index = hash_state->m_ReverseHashEntryIndex = g_dmHashContainer.AllocReverseHashStatesSlot();
            g_dmHashContainer.CloneReverseHashState(new_index, source_hash_state->m_ReverseHashEntryIndex);
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
    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        g_dmHashContainer.UpdateReversHashState(hash_state->m_ReverseHashEntryIndex, hash_state->m_Size, buffer, buffer_len);
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

    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex && hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        dmHashTable64<ReverseHashEntry>* hash_table = &g_dmHashContainer.m_HashTable64Entries;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(g_dmHashContainer.m_HashTableSize, hash_table->Capacity() + g_dmHashContainer.m_HashTableCapacityIncrement);
            }
            hash_table->Put(hash_state->m_Hash, g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex]);
        }
        else
        {
            free(g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        }
        g_dmHashContainer.FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }

    return hash_state->m_Hash;
}

void dmHashRelease64(HashState64* hash_state)
{
    if (g_dmHashContainer.m_Enabled && hash_state->m_ReverseHashEntryIndex)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        free(g_dmHashContainer.m_HashStates[hash_state->m_ReverseHashEntryIndex].m_Value);
        g_dmHashContainer.FreeReverseHashStatesSlot(hash_state->m_ReverseHashEntryIndex);
        hash_state->m_ReverseHashEntryIndex = 0;
    }
}

DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length)
{
    if (g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        ReverseHashEntry* reverse = g_dmHashContainer.m_HashTable32Entries.Get(hash);
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

DM_DLLEXPORT const void* dmHashReverse64(uint64_t hash, uint32_t* length)
{
    if (g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        ReverseHashEntry* reverse = g_dmHashContainer.m_HashTable64Entries.Get(hash);
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

DM_DLLEXPORT void dmHashReverseErase32(uint32_t hash)
{
    if (g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        ReverseHashEntry* reverse = g_dmHashContainer.m_HashTable32Entries.Get(hash);
        if (reverse)
        {
            free(reverse->m_Value);
            g_dmHashContainer.m_HashTable32Entries.Erase(hash);
        }
    }
}

DM_DLLEXPORT void dmHashReverseErase64(uint64_t hash)
{
    if (g_dmHashContainer.m_Enabled)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashContainer.m_Mutex);
        ReverseHashEntry* reverse = g_dmHashContainer.m_HashTable64Entries.Get(hash);
        if (reverse)
        {
            free(reverse->m_Value);
            g_dmHashContainer.m_HashTable64Entries.Erase(hash);
        }
    }
}

DM_DLLEXPORT const char* dmHashReverseSafe64(uint64_t hash)
{
    const char* s = (const char*)dmHashReverse64(hash, 0);
    return s != 0 ? s : "<unknown>";
}

