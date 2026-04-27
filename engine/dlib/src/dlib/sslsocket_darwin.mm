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
#include <Foundation/Foundation.h>
#include <Security/Security.h>

namespace
{
    CFArrayRef g_TrustedCertificates = 0;

    static void ClearTrustedCertificates()
    {
        if (g_TrustedCertificates != 0)
        {
            CFRelease(g_TrustedCertificates);
            g_TrustedCertificates = 0;
        }
    }

    static SecCertificateRef CreateCertificateFromData(NSData* data)
    {
        if (data == nil || [data length] == 0)
        {
            return 0;
        }
        return SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)data);
    }

    static bool AddPEMCertificates(CFMutableArrayRef certificates, NSString* text)
    {
        static NSString* begin_marker = @"-----BEGIN CERTIFICATE-----";
        static NSString* end_marker = @"-----END CERTIFICATE-----";

        bool added = false;
        NSRange search_range = NSMakeRange(0, [text length]);
        while (search_range.location < [text length])
        {
            NSRange begin_range = [text rangeOfString:begin_marker options:0 range:search_range];
            if (begin_range.location == NSNotFound)
            {
                break;
            }

            NSUInteger content_start = begin_range.location + begin_range.length;
            NSRange end_search_range = NSMakeRange(content_start, [text length] - content_start);
            NSRange end_range = [text rangeOfString:end_marker options:0 range:end_search_range];
            if (end_range.location == NSNotFound)
            {
                break;
            }

            NSString* base64 = [text substringWithRange:NSMakeRange(content_start, end_range.location - content_start)];
            NSArray* parts = [base64 componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            NSString* compact_base64 = [parts componentsJoinedByString:@""];
            NSData* der = [[[NSData alloc] initWithBase64EncodedString:compact_base64 options:NSDataBase64DecodingIgnoreUnknownCharacters] autorelease];
            SecCertificateRef certificate = CreateCertificateFromData(der);
            if (certificate != 0)
            {
                CFArrayAppendValue(certificates, certificate);
                CFRelease(certificate);
                added = true;
            }

            NSUInteger next_location = end_range.location + end_range.length;
            search_range = NSMakeRange(next_location, [text length] - next_location);
        }

        return added;
    }

    static CFArrayRef CreateCertificatesFromBuffer(const uint8_t* key, uint32_t keylen)
    {
        @autoreleasepool
        {
            CFMutableArrayRef certificates = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            if (certificates == 0)
            {
                return 0;
            }

            NSString* text = [[[NSString alloc] initWithBytes:key length:keylen encoding:NSASCIIStringEncoding] autorelease];
            if (text != nil)
            {
                AddPEMCertificates(certificates, text);
            }

            if (CFArrayGetCount(certificates) == 0)
            {
                NSData* der = [NSData dataWithBytes:key length:keylen];
                SecCertificateRef certificate = CreateCertificateFromData(der);
                if (certificate != 0)
                {
                    CFArrayAppendValue(certificates, certificate);
                    CFRelease(certificate);
                }
            }

            if (CFArrayGetCount(certificates) == 0)
            {
                CFRelease(certificates);
                return 0;
            }

            return certificates;
        }
    }

    static bool ValidatePeerCertificateChain(CFReadStreamRef read_stream)
    {
        if (g_TrustedCertificates == 0)
        {
            return true;
        }

        SecTrustRef trust = (SecTrustRef)CFReadStreamCopyProperty(read_stream, kCFStreamPropertySSLPeerTrust);
        if (trust == 0)
        {
            return false;
        }

        OSStatus status = SecTrustSetAnchorCertificates(trust, g_TrustedCertificates);
        if (status == errSecSuccess)
        {
            status = SecTrustSetAnchorCertificatesOnly(trust, true);
        }

        bool trusted = status == errSecSuccess && SecTrustEvaluateWithError(trust, 0);
        CFRelease(trust);
        return trusted;
    }
}

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
        else if (error.domain == kCFStreamErrorDomainSSL)
        {
            switch (error.error)
            {
                case errSSLWouldBlock:
                    return dmSocket::RESULT_WOULDBLOCK;
                case errSSLClosedAbort:
                case errSSLClosedNoNotify:
                case errSSLClosedGraceful:
                    return dmSocket::RESULT_CONNRESET;
                default:
                    break;
            }
        }
        return dmSocket::RESULT_UNKNOWN;
    }

    static Result StreamErrorToSSLResult(CFStreamError error)
    {
        dmSocket::Result socket_result = StreamErrorToSocketResult(error);
        if (socket_result == dmSocket::RESULT_WOULDBLOCK)
        {
            return RESULT_WOULDBLOCK;
        }
        if (socket_result == dmSocket::RESULT_CONNRESET || socket_result == dmSocket::RESULT_PIPE)
        {
            return RESULT_CONNREFUSED;
        }
        return RESULT_HANDSHAKE_FAILED;
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
        ClearTrustedCertificates();
        return RESULT_OK;
    }

    Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen)
    {
        ClearTrustedCertificates();

        g_TrustedCertificates = CreateCertificatesFromBuffer(key, keylen);
        if (g_TrustedCertificates == 0)
        {
            return RESULT_SSL_INIT_FAILED;
        }

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
        const void* keys[] = { kCFStreamSSLLevel, kCFStreamSSLPeerName, kCFStreamSSLValidatesCertificateChain };
        const void* values[] = { kCFStreamSocketSecurityLevelNegotiatedSSL, peer_name != 0 ? (CFTypeRef)peer_name : (CFTypeRef)kCFNull, kCFBooleanFalse };
        CFDictionaryRef settings = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

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

        dmSocket::SetSendTimeout(socket, timeout);
        dmSocket::SetReceiveTimeout(socket, timeout);
        dmSocket::SetBlocking(socket, false);

        bool read_opened = CFReadStreamOpen(read_stream);
        bool write_opened = CFWriteStreamOpen(write_stream);
        if (!read_opened || !write_opened)
        {
            Result result = StreamErrorToSSLResult(read_opened ? CFWriteStreamGetError(write_stream) : CFReadStreamGetError(read_stream));
            CFRelease(read_stream);
            CFRelease(write_stream);
            return result;
        }

        if (!ValidatePeerCertificateChain(read_stream))
        {
            CFReadStreamClose(read_stream);
            CFWriteStreamClose(write_stream);
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
