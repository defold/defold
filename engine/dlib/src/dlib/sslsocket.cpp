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
#include "log.h"
#include "math.h"
#include "time.h"
#include "mbedtls_private.hpp"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <dlib/file_descriptor.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

#define MBED_DEBUG_LEVEL 1

#if defined(MBEDTLS_DEBUG_C)
static void mbedtls_debug( void* ctx, int level, const char* file, int line, const char* str )
{
    switch (level) {
    case 0: dmLogError("%s:%d: %s", file, line, str); return;
    case 1: dmLogWarning("%s:%d: %s", file, line, str); return;
    case 2: dmLogDebug("%s:%d: %s", file, line, str); return;
    }
    dmLogInfo("%s:%d: %s", file, line, str);
}
#endif


namespace dmSSLSocket
{

// Since mbedtls doesn't have a way to timeout on individual sockets
// we need to pass in that info somehow. So we use this custom context.
struct CustomNetContext
{
    mbedtls_net_context     m_Context;
    uint64_t                m_Timeout;
};

struct SSLSocket
{
    mbedtls_entropy_context*    m_MbedEntropy;
    mbedtls_ctr_drbg_context*   m_MbedCtrDrbg;
    mbedtls_ssl_config*         m_MbedConf;
    mbedtls_ssl_context*        m_SSLContext;
    CustomNetContext*           m_SSLNetContext;
    uint64_t                    m_TimeStart;  // for read timeouts
    uint64_t                    m_TimeLimit1;
    uint64_t                    m_TimeLimit2;
};

#define SSL_LOGW(MSG, RET) dmLogWarning(MSG  ": %s - %d (%c0x%04X)", dmMbedTls::ResultToString(RET), (RET), (RET) < 0 ? '-':' ', (RET)<0?-(RET):(RET));
#define SSL_LOGE(MSG, RET) dmLogError(MSG  ": %s - %d (%c0x%04X)", dmMbedTls::ResultToString(RET), (RET), (RET) < 0 ? '-':' ', (RET)<0?-(RET):(RET));

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

static void TimingSetDelay(void* data, uint32_t int_ms, uint32_t fin_ms)
{
    SSLSocket* socket = (SSLSocket*)data;
    socket->m_TimeStart = dmTime::GetMonotonicTime();
    socket->m_TimeLimit1 = int_ms; // intermediate limit
    socket->m_TimeLimit2 = fin_ms; // final limit
}

static int TimingGetDelay(void* data)
{
    SSLSocket* socket = (SSLSocket*)data;
    if( socket->m_TimeLimit2 == 0 )
        return -1;

    uint64_t t = dmTime::GetMonotonicTime();
    uint64_t elapsed_ms = (t - socket->m_TimeStart) / 1000;

    if( elapsed_ms >= socket->m_TimeLimit2 )
        return 2;
    if( elapsed_ms >= socket->m_TimeLimit1 )
        return 1;
    return 0;
}

// we're using our own version of mbedtls_net_recv_timeout since we need to handle the timeouts
// on a per connection basis (as opposed to globally)
static int RecvTimeout( void* _ctx, unsigned char *buf, size_t len, uint32_t timeout )
{
    int ret;

    CustomNetContext* ctx = (CustomNetContext*)_ctx;
    int fd = ctx->m_Context.fd;

    if( fd < 0 )
        return( MBEDTLS_ERR_NET_INVALID_CONTEXT );

    if (timeout == 0 && ctx->m_Timeout != 0) {
        timeout = ctx->m_Timeout / 1000;
    }

    dmFileDescriptor::Poller poller;
    dmFileDescriptor::PollerSetEvent(&poller, dmFileDescriptor::EVENT_READ, fd);
    ret = dmFileDescriptor::Wait(&poller, timeout == 0 ? -1 : timeout);

    // Zero fds ready means we timed out
    if( ret == 0 )
        return( MBEDTLS_ERR_SSL_TIMEOUT );

    if( ret < 0 )
    {
#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && \
    !defined(EFI32)
        if( WSAGetLastError() == WSAEINTR )
            return( MBEDTLS_ERR_SSL_WANT_READ );
#else
        if( errno == EINTR )
            return( MBEDTLS_ERR_SSL_WANT_READ );
#endif

        return( MBEDTLS_ERR_NET_RECV_FAILED );
    }

    /* This call will not block */
    return( mbedtls_net_recv( ctx, buf, len ) );
}

Result Delete(SSLSocket* socket)
{
    if (socket)
    {
        mbedtls_ssl_close_notify( socket->m_SSLContext );
        // The underlying socket was created elsewhere and the socket file
        // descriptor is not owned by mbedtls (see New() above). We set it to -1
        // here to ensure that mbedtls_net_free doesn't close it when we called
        socket->m_SSLNetContext->m_Context.fd = -1;
        mbedtls_net_free( (mbedtls_net_context*)socket->m_SSLNetContext );
        mbedtls_ssl_free( socket->m_SSLContext );
        mbedtls_ssl_config_free( socket->m_MbedConf );
        mbedtls_ctr_drbg_free( socket->m_MbedCtrDrbg );
        mbedtls_entropy_free( socket->m_MbedEntropy );

        free(socket->m_MbedConf);
        free(socket->m_MbedCtrDrbg);
        free(socket->m_MbedEntropy);
        free(socket->m_SSLNetContext);
        free(socket->m_SSLContext);
        free(socket);
    }
    return RESULT_OK;
}
Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, SSLSocket** sslsocket)
{
    uint64_t handshakestart = dmTime::GetMonotonicTime();

    SSLSocket* c = (SSLSocket*)malloc(sizeof(SSLSocket));
    memset(c, 0, sizeof(SSLSocket));

#define MBED_CALLOC(_TYPE) (_TYPE*)calloc(1, sizeof(_TYPE))

    c->m_MbedConf       = MBED_CALLOC(mbedtls_ssl_config);
    c->m_MbedCtrDrbg    = MBED_CALLOC(mbedtls_ctr_drbg_context);
    c->m_MbedEntropy    = MBED_CALLOC(mbedtls_entropy_context);
    c->m_SSLContext     = MBED_CALLOC(mbedtls_ssl_context);
    c->m_SSLNetContext  = MBED_CALLOC(CustomNetContext);

#undef MBED_CALLOC

    mbedtls_ssl_config_init( c->m_MbedConf );
    mbedtls_ctr_drbg_init( c->m_MbedCtrDrbg );
    mbedtls_entropy_init( c->m_MbedEntropy );

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold( MBED_DEBUG_LEVEL );
    mbedtls_ssl_conf_dbg( c->m_MbedConf, mbedtls_debug, 0 );
#endif
    int ret = 0;

    const char* pers = "defold_ssl_client";
    if( ( ret = mbedtls_ctr_drbg_seed( c->m_MbedCtrDrbg, mbedtls_entropy_func, c->m_MbedEntropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        SSL_LOGE("mbedtls_ctr_drbg_seed failed", ret);
        return RESULT_SSL_INIT_FAILED;
    }

    if( ( ret = mbedtls_ssl_config_defaults( c->m_MbedConf,
                    MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        SSL_LOGE("mbedtls_ssl_config_defaults failed", ret);
        return RESULT_SSL_INIT_FAILED;
    }

    mbedtls_ssl_conf_rng( c->m_MbedConf, mbedtls_ctr_drbg_random, c->m_MbedCtrDrbg );
    mbedtls_ssl_conf_authmode( c->m_MbedConf, MBEDTLS_SSL_VERIFY_NONE );

    // In order to not have it block (unless timeout == 0)
    dmSocket::SetSendTimeout(socket, (int)timeout);
    dmSocket::SetReceiveTimeout(socket, (int)timeout);

    if (timeout != 0)
    {
        int mbed_ssl_timeout = dmMath::Max((int)timeout, dmSocket::SOCKET_TIMEOUT) / 1000;
        mbed_ssl_timeout = dmMath::Max(1, mbed_ssl_timeout);
        // Never go below 1 second, since that's the lowest supported by this function anyways
        mbedtls_ssl_conf_handshake_timeout(c->m_MbedConf, 1, mbed_ssl_timeout);
    }

    // See comment about timeout in c->m_SSLNetContext
    c->m_SSLNetContext->m_Timeout = timeout;
    mbedtls_ssl_init( c->m_SSLContext );


    dmMbedTls::ConfigureCertificateChain(c->m_MbedConf);

    if( ( ret = mbedtls_ssl_setup( c->m_SSLContext, c->m_MbedConf ) ) != 0 )
    {
        SSL_LOGE("mbedtls_ssl_setup failed", ret);
        return RESULT_HANDSHAKE_FAILED;
    }

    if( ( ret = mbedtls_ssl_set_hostname( c->m_SSLContext, host) ) != 0 )
    {
        SSL_LOGE("mbedtls_ssl_set_hostname failed", ret);
        return RESULT_HANDSHAKE_FAILED;
    }

    // Normally, in mbedtls, you'd call mbedtls_net_connect
    // but we already have our socket created and setup
    mbedtls_net_init( (mbedtls_net_context*)c->m_SSLNetContext );
    c->m_SSLNetContext->m_Context.fd = dmSocket::GetFD(socket);

    mbedtls_ssl_set_bio(c->m_SSLContext, c->m_SSLNetContext, mbedtls_net_send, NULL, RecvTimeout);
    mbedtls_ssl_set_timer_cb(c->m_SSLContext, c, TimingSetDelay, TimingGetDelay);

    do
    {
        ret = mbedtls_ssl_handshake( c->m_SSLContext );
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
             ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    uint64_t currenttime = dmTime::GetMonotonicTime();
    if( timeout > 0 && int(currenttime - handshakestart) > timeout )
    {
        ret = MBEDTLS_ERR_SSL_TIMEOUT;
    }

    Result result = RESULT_OK;
    int flags = 0;
    char vrfy_buf[512];

    if (ret != 0)
    {
        char buffer[512] = "";
        dmMbedTls::FormatError(ret, buffer, sizeof(buffer));
        dmLogError("SSLSocket mbedtls_ssl_handshake: %d - %s  %p",ret, buffer, c);
        if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
        {
            dmLogError("Unable to verify the server's certificate.");
            result = RESULT_CONNREFUSED;
            goto socket_new_fail;
        }
        else if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
        {
            dmLogError("SSL handshake timeout");
            result = RESULT_WOULDBLOCK;
            goto socket_new_fail;
        }
        result = RESULT_HANDSHAKE_FAILED;
        goto socket_new_fail;
    }

    if( ( flags = mbedtls_ssl_get_verify_result( c->m_SSLContext ) ) != 0 )
    {
        mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags );
        dmLogError("mbedtls_ssl_get_verify_result failed:\n    %s\n", vrfy_buf );
        result = RESULT_HANDSHAKE_FAILED;
        goto socket_new_fail;
    }

    *sslsocket = c;
    return RESULT_OK;

socket_new_fail:
    Delete(c);
    *sslsocket = 0;
    return result;
}



dmSocket::Result Send(SSLSocket* socket, const void* buffer, int length, int* sent_bytes)
{
    int r = mbedtls_ssl_write(socket->m_SSLContext, (const uint8_t*) buffer, length);

    if (r == MBEDTLS_ERR_SSL_WANT_WRITE ||
        r == MBEDTLS_ERR_SSL_WANT_READ) {
        return dmSocket::RESULT_TRY_AGAIN;
    }

    if (r < 0) {
        mbedtls_ssl_session_reset(socket->m_SSLContext);
        dmSocket::Result socket_result = dmMbedTls::SocketResultFromSSL(r);
        if (socket_result == dmSocket::RESULT_UNKNOWN)
            SSL_LOGW("Unhandled ssl status code", r);
        return socket_result;
    }

    *sent_bytes = r;
    return dmSocket::RESULT_OK;
}

dmSocket::Result Receive(SSLSocket* socket, void* buffer, int length, int* received_bytes)
{
    int ret = mbedtls_ssl_read(socket->m_SSLContext, (unsigned char*)buffer, length-1 );
    if( ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS )
    {
        return dmSocket::RESULT_WOULDBLOCK;
    }

    if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
        return dmSocket::RESULT_WOULDBLOCK;
    }

    if( ret <= 0 )
    {
        mbedtls_ssl_session_reset(socket->m_SSLContext);
        dmSocket::Result socket_result = dmMbedTls::SocketResultFromSSL(ret);
        if (socket_result == dmSocket::RESULT_UNKNOWN)
            SSL_LOGW("Unhandled ssl status code", ret);
        return socket_result;
    }

    ((uint8_t*)buffer)[ret] = 0;

    *received_bytes = ret;
    return dmSocket::RESULT_OK;
}

dmSocket::Result SetReceiveTimeout(Socket socket, uint64_t timeout)
{
    socket->m_SSLNetContext->m_Timeout = timeout;
    return dmSocket::RESULT_OK;
}

#undef SSL_LOGW
#undef SSL_LOGE

} // namespace
