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

#include "crypt.h"

#include <string.h>

#import <CommonCrypto/CommonDigest.h>
#import <Foundation/Foundation.h>

namespace dmCrypt
{
    void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        CC_SHA1(buf, (CC_LONG)buflen, digest);
    }

    void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        CC_SHA256(buf, (CC_LONG)buflen, digest);
    }

    void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
        CC_SHA512(buf, (CC_LONG)buflen, digest);
    }

    void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
    {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        CC_MD5(buf, (CC_LONG)buflen, digest);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    }

    bool Base64Encode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        @autoreleasepool
        {
            NSData* data = [NSData dataWithBytes:src length:src_len];
            NSData* encoded = [data base64EncodedDataWithOptions:0];
            NSUInteger encoded_len = [encoded length];
            NSUInteger required_len = encoded_len + 1;

            if (*dst_len == 0)
            {
                *dst_len = (uint32_t)required_len;
                return false;
            }

            if (*dst_len < required_len)
            {
                *dst_len = 0xFFFFFFFF;
                return false;
            }

            memcpy(dst, [encoded bytes], encoded_len);
            dst[encoded_len] = 0;
            *dst_len = (uint32_t)encoded_len;
            return true;
        }
    }

    bool Base64Decode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
    {
        @autoreleasepool
        {
            NSData* data = [NSData dataWithBytes:src length:src_len];
            NSData* decoded = [[NSData alloc] initWithBase64EncodedData:data options:NSDataBase64DecodingIgnoreUnknownCharacters];
            if (decoded == nil)
            {
                *dst_len = 0xFFFFFFFF;
                return false;
            }

            NSUInteger decoded_len = [decoded length];
            if (*dst_len == 0)
            {
                *dst_len = (uint32_t)decoded_len;
                return false;
            }

            if (*dst_len < decoded_len)
            {
                *dst_len = 0xFFFFFFFF;
                return false;
            }

            memcpy(dst, [decoded bytes], decoded_len);
            *dst_len = (uint32_t)decoded_len;
            return true;
        }
    }
}
