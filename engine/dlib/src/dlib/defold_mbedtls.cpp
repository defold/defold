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

#include "defold_mbedtls.hpp"

#include <string.h>

#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>

namespace dmMbedTls
{

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

} // namespace dmMbedTls

#if defined(MBEDTLS_THREADING_ALT)

#include <mbedtls/threading.h>
#include <dlib/mutex.h>

static void defold_mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex_wrapper)
{
    mutex_wrapper->mutex = dmMutex::New();
}

static void defold_mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper->mutex != 0x0)
    {
        dmMutex::Delete((dmMutex::HMutex)mutex_wrapper->mutex);
    }
}

static int defold_mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Lock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

static int defold_mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Unlock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

namespace dmMbedTls
{

void Initialize()
{
    mbedtls_threading_set_alt(
        defold_mbedtls_mutex_init,
        defold_mbedtls_mutex_free,
        defold_mbedtls_mutex_lock,
        defold_mbedtls_mutex_unlock
    );
}

void Finalize()
{
    mbedtls_threading_free_alt();
}

} // namespace


#else

namespace dmMbedTls
{

void Initialize()
{
}

void Finalize()
{
}

} // namespace


#endif // MBEDTLS_THREADING_ALT
