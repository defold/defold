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

#ifndef DM_SSLSOCKET_H
#define DM_SSLSOCKET_H

#include <dmsdk/dlib/sslsocket.h>

/**
 * Secure socket abstraction
 * @note For Recv* and Send* function ETIMEDOUT is translated to EWOULDBLOCK
 * on win32 for compatibility with BSD sockets.
 */
namespace dmSSLSocket
{
    /**
     * Initialize secure socket system. Network initialization is required on some platforms.
     * @name dmSSLSocket::Initialize
     * @return RESULT_OK on success
     */
    Result Initialize();

    /**
     * Finalize socket system.
     * @name dmSSLSocket::Initialize
     * @return RESULT_OK on success
     */
    Result Finalize();

    /**
     * Set keys from buffer and use it for verification.
     * @param key key
     * @param keylen key length
     * @name dmSSLSocket::SetSslPublicKeys
     * @return RESULT_OK on success
     */
    Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen);
}

#endif // DM_SSLSOCKET_H