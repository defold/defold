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
#include "mbedtls_private.hpp"

namespace dmSSLSocket
{

Result Initialize()
{
    return RESULT_OK;
}

Result Finalize()
{
    dmMbedTls::ClearCertificateChain();
    return RESULT_OK;
}

Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen)
{
    return dmMbedTls::SetSslPublicKeys(key, keylen);
}
Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, Socket* sslsocket)
{
    return dmMbedTls::NewSocket(socket, host, timeout, sslsocket);
}

Result Delete(Socket socket)
{
    return dmMbedTls::DeleteSocket(socket);
}

dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
{
    return dmMbedTls::SendSocket(socket, buffer, length, sent_bytes);
}

dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
{
    return dmMbedTls::ReceiveSocket(socket, buffer, length, received_bytes);
}

dmSocket::Result SetReceiveTimeout(Socket socket, uint64_t timeout)
{
    return dmMbedTls::SetSocketReceiveTimeout(socket, timeout);
}

} // namespace
