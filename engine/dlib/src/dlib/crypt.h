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

#ifndef DM_CRYPT_H
#define DM_CRYPT_H

#include <stdint.h>
#include <dmsdk/dlib/crypt.h>

namespace dmCrypt
{
    struct HashSha256Context;
    typedef HashSha256Context* HHashSha256;

    // Creates an initialized streaming SHA-256 context, or returns 0 on failure.
    HHashSha256 NewHashSha256();

    // Appends a chunk. A zero-length chunk is a no-op and may have a null buffer.
    // After an error, the context must be deleted.
    Result UpdateHashSha256(HHashSha256 context, const uint8_t* buf, uint32_t buflen);

    // Writes the 32-byte digest on success. The context must then be deleted.
    Result FinalHashSha256(HHashSha256 context, uint8_t* digest);

    // Releases a context, including an unfinished one. Accepts 0.
    void DeleteHashSha256(HHashSha256 context);
}

#endif /* DM_CRYPT_H */
