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

#ifndef DM_CRYPT_H
#define DM_CRYPT_H

#include <stdint.h>
#include <dmsdk/dlib/crypt.h>

namespace dmCrypt
{
    enum HashAlgorithm
    {
        HASH_ALGORITHM_NONE,
        HASH_ALGORITHM_MD5,
        HASH_ALGORITHM_SHA1,
        HASH_ALGORITHM_SHA256,
        HASH_ALGORITHM_SHA512
    };

    /**
     * Verify crypthographic signature for data with public key
     * @param algorithm Hash algorithm used for data
     * @param key The public key
     * @param keylen The public key length
     * @param data Hashed data for verification
     * @param data_len Hashed data length
     * @param expected_signature Expected signature
     * @param expected_signature_len Expected signature length
     * @return RESULT_OK if verification went ok
     */
    Result Verify(HashAlgorithm algorithm, const uint8_t* key, uint32_t keylen, const uint8_t* data, uint32_t data_len, unsigned const char* expected_signature, uint32_t expected_signature_len);
}

#endif /* DM_CRYPT_H */
