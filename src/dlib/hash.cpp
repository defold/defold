#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "hash.h"

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

// Based on MurmurHash2A but endian neutral
uint32_t dmHashBuffer32(const void * key, uint32_t len)
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

// Based on MurmurHash2A but endian neutral
uint64_t dmHashBuffer64(const void * key, uint32_t len)
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


uint32_t DM_DLLEXPORT dmHashString32(const char* string)
{
    return dmHashBuffer32(string, strlen(string));
}

uint64_t DM_DLLEXPORT dmHashString64(const char* string)
{
    return dmHashBuffer64(string, strlen(string));
}

// Based on CMurmurHash2A
void dmHashInit32(HashState32* hash_state)
{
    memset(hash_state, 0, sizeof(*hash_state));
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
}

uint32_t dmHashFinal32(HashState32* hash_state)
{
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    mmix(hash_state->m_Hash, hash_state->m_Tail);
    mmix(hash_state->m_Hash, hash_state->m_Size);

    hash_state->m_Hash ^= hash_state->m_Hash >> 13;
    hash_state->m_Hash *= m;
    hash_state->m_Hash ^= hash_state->m_Hash >> 15;

    return hash_state->m_Hash;
}

// Based on CMurmurHash2A
void dmHashInit64(HashState64* hash_state)
{
    memset(hash_state, 0, sizeof(*hash_state));
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

void dmHashUpdateBuffer64(HashState64* hash_state, const void* buffer, uint64_t buffer_len)
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

    return hash_state->m_Hash;
}

