// Copyright 2020-2025 The Defold Foundation
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
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "shared_library.h"
#include "crypt.h"

#include <dlib/endian.h>
#include <mbedtls/md5.h>
#include <mbedtls/base64.h>
#include <mbedtls/error.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>


#include <dlib/log.h> // For debugging the manifest verification issue

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

    static void LogMbedTlsError(int result)
    {
        char buffer[512] = "";
        mbedtls_strerror(result, buffer, sizeof(buffer));
        dmLogError("mbedtls: %s0x%04x - %s", result < 0 ? "-":"", result < 0 ? -result:result, buffer);
    }

    static mbedtls_md_type_t HashAlgoritmToMbAlgorithm(HashAlgorithm algorithm)
    {
        switch(algorithm)
        {
            case HASH_ALGORITHM_MD5: return MBEDTLS_MD_MD5;
            case HASH_ALGORITHM_SHA1: return MBEDTLS_MD_SHA1;
            case HASH_ALGORITHM_SHA256: return MBEDTLS_MD_SHA256;
            case HASH_ALGORITHM_SHA512: return MBEDTLS_MD_SHA512;
            default: return MBEDTLS_MD_NONE;
        }
    }

    Result Verify(HashAlgorithm algorithm, const uint8_t* key, uint32_t keylen, const uint8_t* data, uint32_t data_len, const unsigned char* expected_signature, uint32_t expected_signature_len)
    {
        Result result = RESULT_OK;

        mbedtls_pk_context pk;
        mbedtls_pk_init(&pk);

        mbedtls_md_type_t mb_algo = HashAlgoritmToMbAlgorithm(algorithm);

        int ret;
        if ((ret = mbedtls_pk_parse_public_key(&pk, key, keylen) != 0))
        {
            LogMbedTlsError(ret);
            dmLogError("Verify: mbedtls_pk_parse_public_key failed: %d", ret);
            mbedtls_pk_free(&pk);
            return RESULT_ERROR;
        }

        if (mb_algo == MBEDTLS_MD_NONE)
        {
            dmLogError("Verify: mb_algo == MBEDTLS_MD_NONE");
            mbedtls_pk_free(&pk);
            return RESULT_ERROR;
        }
        if(mbedtls_pk_get_type(&pk) != MBEDTLS_PK_RSA)
        {
            mbedtls_pk_free(&pk);
            return RESULT_ERROR;
        }

        mbedtls_rsa_context* rsa = mbedtls_pk_rsa(pk);

        if (mbedtls_rsa_get_len(rsa) != expected_signature_len)
        {
            mbedtls_pk_free(&pk);
            return RESULT_INVALID_LENGTH; 
        }

        if ((ret = mbedtls_rsa_pkcs1_verify(rsa, mb_algo, data_len, data, expected_signature)) != 0)
        {
            LogMbedTlsError(ret);
            dmLogError("Verify: mbedtls_rsa_pkcs1_verify failed: %d", ret);
            if (ret == MBEDTLS_ERR_RSA_VERIFY_FAILED)
            {
                result = RESULT_SIGNATURE_MISMATCH;
            }
            else
            {
                result = RESULT_ERROR;
            }
        }
        mbedtls_pk_free(&pk);
        return result;
    }

    void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        mbedtls_sha1_context ctx;
        mbedtls_sha1_init(&ctx);
        mbedtls_sha1_starts(&ctx);
        mbedtls_sha1_update(&ctx, (const unsigned char*)buf, (size_t)buflen);
        int ret = mbedtls_sha1_finish(&ctx, (unsigned char*)digest);
        mbedtls_sha1_free(&ctx);
        if (ret != 0) {
            memset(digest, 0, 20);
        }
    }

    void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        int ret = mbedtls_sha256((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest, 0);
        if (ret != 0) {
            memset(digest, 0, 32);
        }
    }

    void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        int ret = mbedtls_sha512((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest, 0);
        if (ret != 0) {
            memset(digest, 0, 64);
        }
    }

    void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        int ret = mbedtls_md5((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest);
        if (ret != 0) {
            memset(digest, 0, 16);
        }
    }

    bool Base64Encode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        size_t out_len = 0;
        int r = mbedtls_base64_encode(dst, *dst_len, &out_len, src, src_len);
        if (r != 0)
        {
            if (*dst_len == 0)
                *dst_len = (uint32_t)out_len; // Seems to return 1 more than necessary, but better to err on the safe side! (see test_crypt.cpp)
            else
                *dst_len = 0xFFFFFFFF;
            return false;
        }
        *dst_len = (uint32_t)out_len;
        return true;
    }

    bool Base64Decode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        size_t out_len = 0;
        int r = mbedtls_base64_decode(dst, *dst_len, &out_len, src, src_len);
        if (r != 0)
        {
            if (*dst_len == 0)
                *dst_len = (uint32_t)out_len;
            else
                *dst_len = 0xFFFFFFFF;
            return false;
        }
        *dst_len = (uint32_t)out_len;
        return true;
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
