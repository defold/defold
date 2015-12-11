#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "dlib.h"
#include "hash.h"
#include "mutex.h"
#include "hashtable.h"
#include "log.h"

#include <sol/reflect.h>

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
    dmHashTable32<dmReverseHashEntry>                            m_ReverseHashTable32;
    dmHashTable64<dmReverseHashEntry>                            m_ReverseHashTable64;
    dmHashTable<dmHashIncrementalStateKey32, dmReverseHashEntry> m_ReverseStateHashTable32;
    dmHashTable<dmHashIncrementalStateKey64, dmReverseHashEntry> m_ReverseStateHashTable64;

    dmHashInitializer()
    {
        m_Mutex = dmMutex::New();
        m_ReverseHashTable32.SetCapacity(1024U, 256U);
        m_ReverseStateHashTable32.SetCapacity(1024U, 256U);
        m_ReverseHashEnabled = true;
    }

    static void FreeCallback(void* context, const uintptr_t* key, bool* value)
    {
        free((void*) *key);
    }

    template <typename KEY>
    static void BuiltPointerSetCallback(dmHashTable<uintptr_t, bool>* pointer_set, const KEY* key, dmReverseHashEntry* value)
    {
        if (pointer_set->Get((uintptr_t) value->m_Value) == (bool*) 0)
        {
            if (pointer_set->Full())
            {
                pointer_set->SetCapacity(1024, pointer_set->Capacity() + 1024);
            }
            pointer_set->Put((uintptr_t) value->m_Value, true);
        }
    }

    ~dmHashInitializer()
    {
        dmMutex::Delete(m_Mutex);
        dmHashTable<uintptr_t, bool> pointer_set;
        uint32_t table_size = 0;
        table_size += m_ReverseHashTable32.Size();
        table_size += m_ReverseHashTable64.Size();
        table_size += m_ReverseStateHashTable32.Size();
        table_size += m_ReverseStateHashTable64.Size();
        pointer_set.SetCapacity(table_size / 2 + 17, table_size);

        /*
         * Memory can be shared between m_ReverseHashTableX and m_ReverseStateHashTableX
         * We must first build a set of all memory to free
         */
        m_ReverseHashTable32.Iterate(dmHashInitializer::BuiltPointerSetCallback<uint32_t>, &pointer_set);
        m_ReverseHashTable64.Iterate(dmHashInitializer::BuiltPointerSetCallback<uint64_t>, &pointer_set);
        m_ReverseStateHashTable32.Iterate(dmHashInitializer::BuiltPointerSetCallback<dmHashIncrementalStateKey32>, &pointer_set);
        m_ReverseStateHashTable64.Iterate(dmHashInitializer::BuiltPointerSetCallback<dmHashIncrementalStateKey64>, &pointer_set);
        pointer_set.Iterate(dmHashInitializer::FreeCallback, (void*) 0);
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
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmHashTable32<dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseHashTable32;

        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            char* copy = (char*) malloc(len + 1);
            memcpy(copy, key, len);
            copy[len] = '\0';
            hash_table->Put(h, dmReverseHashEntry(copy, len));
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
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
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmHashTable64<dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseHashTable64;

        if (hash_table->Get(h) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            char* copy = (char*) malloc(original_len + 1);
            memcpy(copy, key, original_len);
            copy[original_len] = '\0';
            hash_table->Put(h, dmReverseHashEntry(copy, original_len));
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
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
    memset(hash_state, 0, sizeof(*hash_state));
    if (!reverse_hash)
    {
        hash_state->m_ReverseEntry.m_Length = 0xffffffff;
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
        hash_state->m_ReverseEntry.m_Length != 0xffffffff &&
        hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmHashTable<dmHashIncrementalStateKey32, dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseStateHashTable32;

        dmHashIncrementalStateKey32 key(hash_state->m_Hash, hash_state->m_Tail);

        dmReverseHashEntry* stored_entry = hash_table->Get(key);
        if (stored_entry == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            dmReverseHashEntry entry = hash_state->m_ReverseEntry;
            uint32_t buffer_size = original_len + entry.m_Length;
            void* copy = (char*) malloc(buffer_size + 1);

            char* p = (char*) copy;
            memcpy(p, entry.m_Value, entry.m_Length);
            p += entry.m_Length;
            memcpy(p, buffer, original_len);
            p[original_len] = '\0';

            hash_table->Put(key, dmReverseHashEntry(copy, buffer_size));
            hash_state->m_ReverseEntry = dmReverseHashEntry(copy, buffer_size);
        }
        else
        {
            hash_state->m_ReverseEntry = *stored_entry;
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
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
        hash_state->m_ReverseEntry.m_Length != 0xffffffff &&
        size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);

        dmHashTable32<dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseHashTable32;

        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            dmReverseHashEntry entry = hash_state->m_ReverseEntry;
            hash_table->Put(hash_state->m_Hash, dmReverseHashEntry(entry.m_Value, entry.m_Length));
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
    }

    return hash_state->m_Hash;
}

// Based on CMurmurHash2A
void dmHashInit64(HashState64* hash_state, bool reverse_hash)
{
    memset(hash_state, 0, sizeof(*hash_state));
    if (!reverse_hash)
    {
        hash_state->m_ReverseEntry.m_Length = 0xffffffff;
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
       hash_state->m_ReverseEntry.m_Length != 0xffffffff &&
       hash_state->m_Size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmHashTable<dmHashIncrementalStateKey64, dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseStateHashTable64;

        dmHashIncrementalStateKey64 key(hash_state->m_Hash, hash_state->m_Tail);

        dmReverseHashEntry* stored_entry = hash_table->Get(key);
        if (stored_entry == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            dmReverseHashEntry entry = hash_state->m_ReverseEntry;
            uint32_t buffer_size = original_len + entry.m_Length;
            void* copy = (char*) malloc(buffer_size + 1);

            char* p = (char*) copy;
            memcpy(p, entry.m_Value, entry.m_Length);
            p += entry.m_Length;
            memcpy(p, buffer, original_len);
            p[original_len] = '\0';

            hash_table->Put(key, dmReverseHashEntry(copy, buffer_size));
            hash_state->m_ReverseEntry = dmReverseHashEntry(copy, buffer_size);
        }
        else
        {
            hash_state->m_ReverseEntry = *stored_entry;
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
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
        hash_state->m_ReverseEntry.m_Length != 0xffffffff &&
        size <= DMHASH_MAX_REVERSE_LENGTH)
    {
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);

        dmHashTable64<dmReverseHashEntry>* hash_table = &g_dmHashInitializer.m_ReverseHashTable64;

        if (hash_table->Get(hash_state->m_Hash) == 0)
        {
            if (hash_table->Full())
            {
                hash_table->SetCapacity(1024U, hash_table->Capacity() + 512U);
            }

            dmReverseHashEntry entry = hash_state->m_ReverseEntry;
            hash_table->Put(hash_state->m_Hash, dmReverseHashEntry(entry.m_Value, entry.m_Length));
        }
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);
    }

    return hash_state->m_Hash;
}

DM_DLLEXPORT const void* dmHashReverse32(uint32_t hash, uint32_t* length)
{
    if (g_dmHashInitializer.m_ReverseHashEnabled && dLib::IsDebugMode())
    {
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntry* reverse = g_dmHashInitializer.m_ReverseHashTable32.Get(hash);
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);

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
        dmMutex::Lock(g_dmHashInitializer.m_Mutex);
        dmReverseHashEntry* reverse = g_dmHashInitializer.m_ReverseHashTable64.Get(hash);
        dmMutex::Unlock(g_dmHashInitializer.m_Mutex);

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

extern "C"
{
    uint64_t SolHashString64(const char* string)
    {
        return dmHashBuffer64(string, strlen(string));
    }

    uint32_t SolHashString32(const char* string)
    {
        return dmHashBuffer32(string, strlen(string));
    }

    void SolHashInit32(HashState32* state)
    {
        if (state)
        {
            dmHashInit32(state, false);
        }
        else
        {
            dmLogError("Invalid parameters sent to hash function");
        }
    }
    
    // gets pointer to struct contents.
    static bool GetObjData(::Type* type, void* value, void** out, uint32_t* size)
    {
        if (!type || !value)
            return false;
            
        if (type->referenced_type)
            return GetObjData(type, value, out, size);
        if (!type->struct_type)
            return false;    
        *out = value;
        *size = type->struct_type->size;
        return true;
    }
    
    void SolHashUpdate32Internal(HashState32* state, ::Type* type, uint64_t value)
    {
        switch (type->kind)
        {
            case KIND_REFERENCE:
                SolHashUpdate32Internal(state, type->referenced_type, value);
                break;
            case KIND_STRUCT:
            {
                void *ptr = (void*)value;;
                dmHashUpdateBuffer32(state, ptr, type->struct_type->size);
                break;
            }
            case KIND_INT8:
            case KIND_UINT8:
                dmHashUpdateBuffer32(state, &value, 1);
                break;
            case KIND_INT16:
            case KIND_UINT16:
                dmHashUpdateBuffer32(state, &value, 2);
                break;
            case KIND_INT32:
            case KIND_UINT32:
            case KIND_FLOAT32:
                dmHashUpdateBuffer32(state, &value, 4);
                break;
            case KIND_INT64:
            case KIND_UINT64:
            case KIND_FLOAT64:
                dmHashUpdateBuffer32(state, &value, 8);
                break;
            case KIND_HANDLE:
                dmHashUpdateBuffer32(state, &value, sizeof(void*));
                break;
            default:
                dmLogWarning("Attempting to hash type %d which is not supported", type->kind);
                break;
        }
    }
    
    // shallow object hashing
    void SolHashUpdate32(HashState32* state, ::Any object)
    {
        if (!state)
        {  
            dmLogError("Invalid state passed to hash.update");
            return;
        }
        SolHashUpdate32Internal(state, reflect_get_any_type(object), reflect_get_any_value(object));
    }
    
    uint32_t SolHashFinal32(HashState32* state)
    {
        if (state)
        {
            return dmHashFinal32(state);
        }
        else
        {
            dmLogError("Null state passed to hash function");
            return 0;
        }
    }
}
    
