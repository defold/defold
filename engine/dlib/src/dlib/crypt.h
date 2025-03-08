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
    /**
     * Decrypt data with a public hash
     * @param key The public key
     * @param keylen
     * @param data The input data
     * @param datalen
     * @param output [out] The output. If the call was successful, the caller needs to free() this memory
     * @param outputlen [out]
     * @return RESULT_OK if decrypting went ok.
     */
    Result Decrypt(const uint8_t* key, uint32_t keylen, const uint8_t* data, uint32_t datalen, uint8_t** output, uint32_t* outputlen);
}

#endif /* DM_CRYPT_H */
