#include "crypt.h"

#include <assert.h>
#include <string.h>

#include "endian.h"
#include "shared_library.h"

namespace dmCrypt
{
    const uint32_t NUM_ROUNDS = 32;

    static inline uint64_t EncryptXTea(uint64_t v, uint32_t* key)
    {
        uint32_t v0 = (uint32_t) (v >> 32);
        uint32_t v1 = (uint32_t) (v & 0xffffffff);

        uint32_t sum = 0, delta = 0x9e3779b9;
        for (uint32_t i = 0; i < NUM_ROUNDS; i++) {
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + dmEndian::ToHost(key[sum & 3]));
            sum += delta;
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + dmEndian::ToHost(key[(sum>>11) & 3]));
        }
        uint64_t ret = dmEndian::ToHost((((uint64_t) v0) << 32 | v1));
        return ret;
    }

    static void EncryptXTeaCTR(uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        const uint32_t block_len = 8;
        uint8_t paddedkey[16] = {0};
        memcpy(paddedkey, key, keylen);

        uint64_t counter = 0;

        uint32_t i = 0;
        uint64_t* d = (uint64_t*) data;
        for (i = 0; i < datalen / block_len; i++) {
            uint64_t enc_counter = EncryptXTea(counter, (uint32_t*) paddedkey);
            d[i] ^= enc_counter;
            data += block_len;
            counter++;
        }

        uint64_t enc_counter = EncryptXTea(counter, (uint32_t*) paddedkey);
        uint32_t rest = datalen & (block_len - 1);
        uint8_t* ec = (uint8_t*) &enc_counter;
        for (uint32_t j = 0; j < rest; j++) {
            data[j] ^= ec[j];
        }
    }

    Result Encrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        assert(keylen <= 16);
        EncryptXTeaCTR(data, datalen, key, keylen);
        return RESULT_OK;
    }

    Result Decrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        assert(keylen <= 16);
        EncryptXTeaCTR(data, datalen, key, keylen);
        return RESULT_OK;
    }

}

extern "C" {
    DM_DLLEXPORT int EncryptXTeaCTR(uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        return dmCrypt::Encrypt(dmCrypt::ALGORITHM_XTEA, data, datalen, key, keylen);
    }

    DM_DLLEXPORT int DecryptXTeaCTR(uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        return dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, data, datalen, key, keylen);
    }

}
