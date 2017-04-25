#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "dlib.h"
#include "hash.h"
#include "mutex.h"
#include "hashtable.h"
#include "log.h"

static const uint32_t DMHASH_INVALID_REVERSE_LENGTH = 0xffffff;

struct dmReverseHashEntryInternal
{
    inline dmReverseHashEntryInternal(void* value, uint32_t length) : m_Value(value), m_Length (length) {}
    inline dmReverseHashEntryInternal(dmReverseHashEntry& entry)
    {
        m_Length = entry.m_Length;
        if(m_Length)
        {
            m_Value = malloc(m_Length + 1);
            memcpy(m_Value, &entry.m_Value[0],  m_Length + 1);
            return;
        }
        m_Value = 0;
    }
    void*    m_Value;
    uint16_t m_Length; // If set to DMHASH_INVALID_REVERSE_LENGTH reverse hashing is disabled
};

struct dmHashIncrementalStateKey32
{
    uint32_t m_Hash;
    uint32_t m_Tail;

    inline dmHashIncrementalStateKey32(uint32_t hash, uint32_t tail)
    {
        m_Hash = hash;
        m_Tail = tail;
    }

    inline uint32_t operator %(uint32_t x)
    {
        uint64_t value = uint64_t(m_Hash) << 32Ull | m_Tail;
        return value % x;
    }

    inline bool operator==(const dmHashIncrementalStateKey32& other)
    {
        return m_Hash == other.m_Hash && m_Tail == other.m_Tail;
    }
};

struct dmHashIncrementalStateKey64
{
    uint64_t m_Hash;
    uint64_t m_Tail;

    inline dmHashIncrementalStateKey64(uint64_t hash, uint64_t tail)
    {
        m_Hash = hash;
        m_Tail = tail;
    }

    inline uint64_t operator %(uint64_t x)
    {
        // NOTE: This is pseudo-modulo. We add the number prior to modulo.
        uint64_t value = m_Hash + m_Tail;
        return value % x;
    }

    inline bool operator==(const dmHashIncrementalStateKey64& other)
    {
        return m_Hash == other.m_Hash && m_Tail == other.m_Tail;
    }
};

struct dmHashInitializer
{
    dmMutex::Mutex                                               m_Mutex;
    bool                                                         m_ReverseHashEnabled;
    dmHashTable32<dmReverseHashEntryInternal>                    m_ReverseHashTable32;
    dmHashTable64<dmReverseHashEntryInternal>                    m_ReverseHashTable64;

    dmHashInitializer()
    {
        m_Mutex = dmMutex::New();
        m_ReverseHashTable32.SetCapacity(1024U, 256U);
        m_ReverseHashEnabled = true;
    }

    template <typename KEY>
    static void FreeCallback(void* context, const KEY* key, dmReverseHashEntryInternal* value)
    {
        free(value->m_Value);
    }

    ~dmHashInitializer()
    {
        dmMutex::Delete(m_Mutex);
        m_ReverseHashTable32.Iterate(dmHashInitializer::FreeCallback, (void*) 0);
        m_ReverseHashTable64.Iterate(dmHashInitializer::FreeCallback, (void*) 0);
    }
};

dmHashInitializer g_dmHashInitializer;

void dmHashEnableReverseHash(bool enable)
{
    g_dmHashInitializer.m_ReverseHashEnabled = enable;
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

    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode() && len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashTable32<dmReverseHashEntryInternal>* hash_table = &g_dmHashInitializer.m_ReverseHashTable32;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            char* copy = (char*) malloc(len + 1);
            memcpy(copy, key, len);
            copy[len] = '\0';
            hash_table->Put(h, dmReverseHashEntryInternal(copy, len));
        }
    }

    return h;
}

// Based on MurmurHash2A but endian neutral
uint64_t dmHashBuffer64(const void * key, uint32_t len)
{
    uint32_t original_len = len;
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

    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode() && original_len <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashTable64<dmReverseHashEntryInternal>* hash_table = &g_dmHashInitializer.m_ReverseHashTable64;
        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            char* copy = (char*) malloc(original_len + 1);
            memcpy(copy, key, original_len);
            copy[original_len] = '\0';
            hash_table->Put(h, dmReverseHashEntryInternal(copy, original_len));
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
    hash_state->m_Hash = 0;
    hash_state->m_Tail = 0;
    hash_state->m_Count = 0;
    hash_state->m_Size = 0;
    hash_state->m_ReverseEntry.m_Length = (reverse_hash ? 0x0 : DMHASH_INVALID_REVERSE_LENGTH);
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
    uint32_t original_len = buffer_len;
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

    if (g_dmHashInitializer.m_ReverseHashEnabled &&
        dLib::IsDebugMode() &&
        hash_state->m_ReverseEntry.m_Length != DMHASH_INVALID_REVERSE_LENGTH &&
        hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashIncrementalStateKey32 key(hash_state->m_Hash, hash_state->m_Tail);
        dmReverseHashEntry& entry = hash_state->m_ReverseEntry;
        uint8_t* p = &entry.m_Value[entry.m_Length];
        memcpy(p, buffer, original_len);
        p[original_len] = '\0';
        entry.m_Length += original_len;
    }

}

uint32_t dmHashFinal32(HashState32* hash_state)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    uint32_t size = hash_state->m_Size;
    mmix(hash_state->m_Hash, hash_state->m_Tail);
    mmix(hash_state->m_Hash, hash_state->m_Size);

    hash_state->m_Hash ^= hash_state->m_Hash >> 13;
    hash_state->m_Hash *= m;
    hash_state->m_Hash ^= hash_state->m_Hash >> 15;

    if (g_dmHashInitializer.m_ReverseHashEnabled &&
        dLib::IsDebugMode() &&
        hash_state->m_ReverseEntry.m_Length != DMHASH_INVALID_REVERSE_LENGTH &&
        size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashTable32<dmReverseHashEntryInternal>* hash_table = &g_dmHashInitializer.m_ReverseHashTable32;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }
            hash_table->Put(hash_state->m_Hash, dmReverseHashEntryInternal(hash_state->m_ReverseEntry));
        }
    }

    return hash_state->m_Hash;
}

// Based on CMurmurHash2A
void dmHashInit64(HashState64* hash_state, bool reverse_hash)
{
    hash_state->m_Hash = 0;
    hash_state->m_Tail = 0;
    hash_state->m_Count = 0;
    hash_state->m_Size = 0;
    hash_state->m_ReverseEntry.m_Length = (reverse_hash ? 0x0 : DMHASH_INVALID_REVERSE_LENGTH);
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
    uint32_t original_len = buffer_len;
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

    if (g_dmHashInitializer.m_ReverseHashEnabled &&
       dLib::IsDebugMode() &&
       hash_state->m_ReverseEntry.m_Length != DMHASH_INVALID_REVERSE_LENGTH &&
       hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashIncrementalStateKey64 key(hash_state->m_Hash, hash_state->m_Tail);
        dmReverseHashEntry& entry = hash_state->m_ReverseEntry;

        uint8_t* p = &entry.m_Value[entry.m_Length];
        memcpy(p, buffer, original_len);
        p[original_len] = '\0';
        entry.m_Length += original_len;
    }

}

uint64_t dmHashFinal64(HashState64* hash_state)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;

    uint64_t s = hash_state->m_Size;

    uint32_t size = hash_state->m_Size;
    mmix(hash_state->m_Hash, hash_state->m_Tail);
    mmix(hash_state->m_Hash, s);

    hash_state->m_Hash ^= hash_state->m_Hash >> r;
    hash_state->m_Hash *= m;
    hash_state->m_Hash ^= hash_state->m_Hash >> r;

    if (g_dmHashInitializer.m_ReverseHashEnabled &&
        dLib::IsDebugMode() &&
        hash_state->m_ReverseEntry.m_Length != DMHASH_INVALID_REVERSE_LENGTH &&
        size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmHashTable64<dmReverseHashEntryInternal>* hash_table = &g_dmHashInitializer.m_ReverseHashTable64;
        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }
            hash_table->Put(hash_state->m_Hash, dmReverseHashEntryInternal(hash_state->m_ReverseEntry));
        }
    }

    return hash_state->m_Hash;
}

DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length)
{
    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode())
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntryInternal* reverse = g_dmHashInitializer.m_ReverseHashTable32.Get(hash);
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
    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode())
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntryInternal* reverse = g_dmHashInitializer.m_ReverseHashTable64.Get(hash);
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
    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode())
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntryInternal* reverse = g_dmHashInitializer.m_ReverseHashTable32.Get(hash);
        if (reverse)
        {
            g_dmHashInitializer.m_ReverseHashTable32.Erase(hash);
            free(reverse->m_Value);
        }
    }
}

DM_DLLEXPORT void dmHashReverseErase64(uint64_t hash)
{
    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode())
    {
        DM_MUTEX_SCOPED_LOCK(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntryInternal* reverse = g_dmHashInitializer.m_ReverseHashTable64.Get(hash);
        if (reverse)
        {
            g_dmHashInitializer.m_ReverseHashTable64.Erase(hash);
            free(reverse->m_Value);
        }
    }
}
