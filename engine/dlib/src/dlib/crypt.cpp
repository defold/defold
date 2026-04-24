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

#include <string.h>
#include <assert.h>
#include "shared_library.h"
#include "crypt.h"

#include <dlib/endian.h>
#include <dlib/defold_mbedtls.hpp>

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
        assert(keylen <= 16);
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
        EncryptXTeaCTR(data, datalen, key, keylen);
        return RESULT_OK;
    }

    Result Decrypt(Algorithm algo, uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    {
        EncryptXTeaCTR(data, datalen, key, keylen);
        return RESULT_OK;
    }

    void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        dmMbedTls::HashSha1(buf, buflen, digest);
    }

    void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        dmMbedTls::HashSha256(buf, buflen, digest);
    }

    void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        dmMbedTls::HashSha512(buf, buflen, digest);
    }

    void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        dmMbedTls::HashMd5(buf, buflen, digest);
    }

    bool Base64Encode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        return dmMbedTls::Base64Encode(src, src_len, dst, dst_len);
    }

    bool Base64Decode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        return dmMbedTls::Base64Decode(src, src_len, dst, dst_len);
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
