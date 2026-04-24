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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>

namespace dmSSLSocket
{
    struct SSLSocket
    {
        dmSocket::Socket m_Socket;
        CFReadStreamRef  m_ReadStream;
        CFWriteStreamRef m_WriteStream;
    };

    static dmSocket::Result StreamErrorToSocketResult(CFStreamError error)
    {
        if (error.domain == kCFStreamErrorDomainPOSIX)
        {
            switch (error.error)
            {
                case EAGAIN:
#if EWOULDBLOCK != EAGAIN
                case EWOULDBLOCK:
#endif
                case ETIMEDOUT:
                    return dmSocket::RESULT_WOULDBLOCK;
                case ECONNRESET:
                    return dmSocket::RESULT_CONNRESET;
                case EPIPE:
                    return dmSocket::RESULT_PIPE;
                default:
                    break;
            }
        }
        return dmSocket::RESULT_UNKNOWN;
    }

    static bool SetStreamProperty(CFReadStreamRef read_stream, CFWriteStreamRef write_stream, CFStringRef property, CFTypeRef value)
    {
        return CFReadStreamSetProperty(read_stream, property, value) && CFWriteStreamSetProperty(write_stream, property, value);
    }

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
        return RESULT_OK;
    }

    Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, Socket* sslsocket)
    {
        if (sslsocket == 0)
        {
            return RESULT_UNKNOWN;
        }
        *sslsocket = INVALID_SOCKET_HANDLE;

        CFReadStreamRef read_stream = 0;
        CFWriteStreamRef write_stream = 0;
        CFStreamCreatePairWithSocket(kCFAllocatorDefault, (CFSocketNativeHandle)socket, &read_stream, &write_stream);
        if (read_stream == 0 || write_stream == 0)
        {
            if (read_stream != 0)
            {
                CFRelease(read_stream);
            }
            if (write_stream != 0)
            {
                CFRelease(write_stream);
            }
            return RESULT_SSL_INIT_FAILED;
        }

        CFStringRef peer_name = host != 0 ? CFStringCreateWithCString(kCFAllocatorDefault, host, kCFStringEncodingUTF8) : 0;
        const void* keys[] = { kCFStreamSSLLevel, kCFStreamSSLPeerName };
        const void* values[] = { kCFStreamSocketSecurityLevelNegotiatedSSL, peer_name != 0 ? (CFTypeRef)peer_name : (CFTypeRef)kCFNull };
        CFDictionaryRef settings = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        bool ok = settings != 0 &&
                  SetStreamProperty(read_stream, write_stream, kCFStreamPropertyShouldCloseNativeSocket, kCFBooleanFalse) &&
                  SetStreamProperty(read_stream, write_stream, kCFStreamPropertySSLSettings, settings);

        if (settings != 0)
        {
            CFRelease(settings);
        }
        if (peer_name != 0)
        {
            CFRelease(peer_name);
        }

        if (!ok)
        {
            CFRelease(read_stream);
            CFRelease(write_stream);
            return RESULT_SSL_INIT_FAILED;
        }

        dmSocket::SetReceiveTimeout(socket, timeout);
        if (!CFReadStreamOpen(read_stream) || !CFWriteStreamOpen(write_stream))
        {
            CFRelease(read_stream);
            CFRelease(write_stream);
            return RESULT_HANDSHAKE_FAILED;
        }

        Socket ssl_socket = (Socket)malloc(sizeof(SSLSocket));
        if (ssl_socket == 0)
        {
            CFReadStreamClose(read_stream);
            CFWriteStreamClose(write_stream);
            CFRelease(read_stream);
            CFRelease(write_stream);
            return RESULT_SSL_INIT_FAILED;
        }

        memset(ssl_socket, 0, sizeof(SSLSocket));
        ssl_socket->m_Socket      = socket;
        ssl_socket->m_ReadStream  = read_stream;
        ssl_socket->m_WriteStream = write_stream;
        *sslsocket = ssl_socket;
        return RESULT_OK;
    }

    Result Delete(Socket socket)
    {
        if (socket == INVALID_SOCKET_HANDLE)
        {
            return RESULT_OK;
        }
        CFReadStreamClose(socket->m_ReadStream);
        CFWriteStreamClose(socket->m_WriteStream);
        CFRelease(socket->m_ReadStream);
        CFRelease(socket->m_WriteStream);
        free(socket);
        return RESULT_OK;
    }

    dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
    {
        if (sent_bytes != 0)
        {
            *sent_bytes = 0;
        }

        CFIndex result = CFWriteStreamWrite(socket->m_WriteStream, (const UInt8*)buffer, length);
        if (result > 0)
        {
            if (sent_bytes != 0)
            {
                *sent_bytes = (int)result;
            }
            return dmSocket::RESULT_OK;
        }
        if (result == 0)
        {
            return dmSocket::RESULT_WOULDBLOCK;
        }
        return StreamErrorToSocketResult(CFWriteStreamGetError(socket->m_WriteStream));
    }

    dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
    {
        if (received_bytes != 0)
        {
            *received_bytes = 0;
        }

        CFIndex result = CFReadStreamRead(socket->m_ReadStream, (UInt8*)buffer, length);
        if (result > 0)
        {
            if (received_bytes != 0)
            {
                *received_bytes = (int)result;
            }
            return dmSocket::RESULT_OK;
        }
        if (result == 0)
        {
            return dmSocket::RESULT_CONNRESET;
        }
        return StreamErrorToSocketResult(CFReadStreamGetError(socket->m_ReadStream));
    }

    dmSocket::Result SetReceiveTimeout(Socket socket, uint64_t timeout)
    {
        return dmSocket::SetReceiveTimeout(socket->m_Socket, timeout);
    }
}
