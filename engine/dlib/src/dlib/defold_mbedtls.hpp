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

#ifndef DM_DEFOLD_MBEDTLS_H
#define DM_DEFOLD_MBEDTLS_H

#include <stdint.h>

namespace dmMbedTls
{
    void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest);      // output is 20 bytes
    void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest);    // output is 32 bytes
    void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest);    // output is 64 bytes
    void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest);       // output is 16 bytes

    bool Base64Encode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len);
    bool Base64Decode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len);

    void Initialize();
    void Finalize();
}

#endif // DM_DEFOLD_MBEDTLS_H
