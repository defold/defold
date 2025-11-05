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

#include "resource_util.h"
#include "resource_manifest_private.h"

#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>

namespace dmResource
{

static Result DecryptWithXtea(void* buffer, uint32_t buffer_len);
const char* KEY = "aQj8CScgNP4VsfXK";

FDecryptResource g_ResourceDecryption = DecryptWithXtea; // Currently global since we didn't use the resource factory as the context!

void RegisterResourceDecryptionFunction(FDecryptResource decrypt_resource)
{
    g_ResourceDecryption = decrypt_resource;
    if (g_ResourceDecryption == 0)
        g_ResourceDecryption = DecryptWithXtea;
}

static dmResource::Result DecryptWithXtea(void* buffer, uint32_t buffer_len)
{
    dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) buffer, buffer_len, (const uint8_t*) KEY, strlen(KEY));
    if (cr != dmCrypt::RESULT_OK)
    {
        return dmResource::RESULT_UNKNOWN_ERROR;
    }
    return dmResource::RESULT_OK;
}

dmResource::Result DecryptBuffer(void* buffer, uint32_t buffer_len)
{
    return g_ResourceDecryption(buffer, buffer_len);
}

static dmCrypt::HashAlgorithm DDFHashToCryptHash(dmLiveUpdateDDF::HashAlgorithm algo)
{
    switch(algo)
    {
        case dmLiveUpdateDDF::HASH_MD5: return dmCrypt::HASH_ALGORITHM_MD5;
        case dmLiveUpdateDDF::HASH_SHA1: return dmCrypt::HASH_ALGORITHM_SHA1;
        case dmLiveUpdateDDF::HASH_SHA256: return dmCrypt::HASH_ALGORITHM_SHA256;
        case dmLiveUpdateDDF::HASH_SHA512: return dmCrypt::HASH_ALGORITHM_SHA512;
        default: return dmCrypt::HASH_ALGORITHM_NONE;
    }
}

static Result DmCryptResultToResourceResult(dmCrypt::Result crypt_result)
{
    switch (crypt_result)
    {
    case dmCrypt::RESULT_OK:
        return RESULT_OK;
    case dmCrypt::RESULT_INVALID_LENGTH:
        return RESULT_FORMAT_ERROR;
    case dmCrypt::RESULT_SIGNATURE_MISMATCH:
        return RESULT_SIGNATURE_MISMATCH;
    }
    return RESULT_INVALID_DATA;
}

Result VerifySignatureHash(const dmResource::HManifest manifest, const uint8_t* pub_key_buf, uint32_t pub_key_len)
{
    const uint8_t* data = manifest->m_DDF->m_Data.m_Data;
    uint32_t data_len = manifest->m_DDF->m_Data.m_Count;

    dmLiveUpdateDDF::HashAlgorithm hash_algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
    uint8_t hashed_content[64] = { 0 };
    uint32_t hash_len = HashLength(hash_algorithm);
    switch(hash_algorithm)
    {
        case dmLiveUpdateDDF::HASH_MD5:
            dmCrypt::HashMd5(data, data_len, hashed_content);
            break;
        case dmLiveUpdateDDF::HASH_SHA1:
            dmCrypt::HashSha1(data, data_len, hashed_content);
            break;
        case dmLiveUpdateDDF::HASH_SHA256:
            dmCrypt::HashSha256(data, data_len, hashed_content);
            break;
        case dmLiveUpdateDDF::HASH_SHA512:
            dmCrypt::HashSha512(data, data_len, hashed_content);
            break;
        default:
            return RESULT_INVALID_DATA;
    }

    const uint8_t* signature = manifest->m_DDF->m_Signature.m_Data;
    uint32_t signature_len = manifest->m_DDF->m_Signature.m_Count;
    dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
    dmCrypt::Result r = dmCrypt::Verify(DDFHashToCryptHash(algorithm), pub_key_buf, pub_key_len, hashed_content, hash_len, signature, signature_len);
    return DmCryptResultToResourceResult(r);
}

uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
{
    const uint32_t bitlen[5] = { 0U, 128U, 160U, 256U, 512U };
    return bitlen[(int) algorithm] / 8U;
}

void BytesToHexString(const uint8_t* byte_buf, uint32_t byte_buf_len, char* out_buf, uint32_t out_len)
{
    if (out_buf != NULL && out_len > 0)
    {
        uint32_t out_len_cond = (out_len + 1) / 2;
        out_buf[0] = 0x0;
        for (uint32_t i = 0; i < byte_buf_len; ++i)
        {
            char current[3];
            dmSnPrintf(current, 3, "%02x", byte_buf[i]);

            if (i < out_len_cond)
                strncat(out_buf, current, 1);
            if (i+1 < out_len_cond)
                strncat(out_buf, current+1, 1);
            else
                break;
        }
    }
}

Result CreateResourceHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buf, size_t buflen, uint8_t* digest)
{
    if (algorithm == dmLiveUpdateDDF::HASH_MD5)
    {
        dmCrypt::HashMd5(buf, buflen, digest);
    }
    else if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
    {
        dmCrypt::HashSha1(buf, buflen, digest);
    }
    else
    {
        dmLogError("The algorithm specified for manifest verification hashing is not supported (%i)", algorithm);
        return RESULT_INVALID_DATA;
    }
    return RESULT_OK;
}

Result CreateManifestHash(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* buf, size_t buflen, uint8_t* digest)
{
    if (algorithm == dmLiveUpdateDDF::HASH_SHA1)
    {
        dmCrypt::HashSha1(buf, buflen, digest);
    }
    else if (algorithm == dmLiveUpdateDDF::HASH_SHA256)
    {
        dmCrypt::HashSha256(buf, buflen, digest);
    }
    else if (algorithm == dmLiveUpdateDDF::HASH_SHA512)
    {
        dmCrypt::HashSha512(buf, buflen, digest);
    }
    else
    {
        dmLogError("The algorithm specified for manifest verification hashing is not supported (%i)", algorithm);
        return RESULT_INVALID_DATA;
    }
    return RESULT_OK;
}

Result MemCompare(const uint8_t* digest, uint32_t len, const uint8_t* expected_digest, uint32_t expected_len)
{
    if (expected_len != len)
    {
        dmLogError("Length mismatch in hash comparison. Expected %u, got %u", expected_len, len);
        return RESULT_FORMAT_ERROR;
    }
    if (memcmp(digest, expected_digest, len) != 0)
    {
        return RESULT_SIGNATURE_MISMATCH;;
    }
    return RESULT_OK;
}

// TODO: Test this...
uint32_t GetCanonicalPathFromBase(const char* base_dir, const char* relative_dir, char* buf, uint32_t buf_len)
{
    dmSnPrintf(buf, buf_len, "%s/%s", base_dir, relative_dir);

    char* source = buf;
    char* dest = buf;
    char last_c = 0;
    while (*source != 0)
    {
        char c = *source;
        if (c != '/' || (c == '/' && last_c != '/'))
            *dest++ = c;

        last_c = c;
        ++source;
    }
    *dest = '\0';
    return (uint32_t)(dest - buf);
}

uint32_t GetCanonicalPath(const char* relative_dir, char* buf, uint32_t buf_len)
{
    return GetCanonicalPathFromBase("", relative_dir, buf, buf_len);
}


void PrintHash(const uint8_t* hash, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i)
    {
        printf("%02x", hash[i]);
    }
}


}
