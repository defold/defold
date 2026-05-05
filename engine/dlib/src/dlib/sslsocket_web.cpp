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

#include "sslsocket.h"

namespace dmSSLSocket
{
    Result Initialize()
    {
        return RESULT_OK;
    }

    Result Finalize()
    {
        return RESULT_OK;
    }

    Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen)
    {
        (void) key;
        (void) keylen;
        return RESULT_SSL_INIT_FAILED;
    }

    Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, Socket* sslsocket)
    {
        (void) socket;
        (void) host;
        (void) timeout;
        if (sslsocket)
            *sslsocket = INVALID_SOCKET_HANDLE;
        return RESULT_SSL_INIT_FAILED;
    }

    Result Delete(Socket socket)
    {
        (void) socket;
        return RESULT_OK;
    }

    dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
    {
        (void) socket;
        (void) buffer;
        (void) length;
        if (sent_bytes)
            *sent_bytes = 0;
        return dmSocket::RESULT_OPNOTSUPP;
    }

    dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
    {
        (void) socket;
        (void) buffer;
        (void) length;
        if (received_bytes)
            *received_bytes = 0;
        return dmSocket::RESULT_OPNOTSUPP;
    }

    dmSocket::Result SetReceiveTimeout(Socket socket, uint64_t timeout)
    {
        (void) socket;
        (void) timeout;
        return dmSocket::RESULT_OPNOTSUPP;
    }
}
