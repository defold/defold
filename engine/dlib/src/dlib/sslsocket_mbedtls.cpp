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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/file_descriptor.h>
#include <dlib/time.h>
#include <dmsdk/dlib/socket.h>

#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

namespace
{
    #define MBED_DEBUG_LEVEL 1
    #define SSL_LOGW(MSG, RET) dmLogWarning(MSG  ": %s - %d (%c0x%04X)", ResultToString(RET), (RET), (RET) < 0 ? '-':' ', (RET)<0?-(RET):(RET))
    #define SSL_LOGE(MSG, RET) dmLogError(MSG  ": %s - %d (%c0x%04X)", ResultToString(RET), (RET), (RET) < 0 ? '-':' ', (RET)<0?-(RET):(RET))

    struct Context
    {
        mbedtls_x509_crt* m_x509CertChain;
    } g_MbedTlsContext;

    const char* ResultToString(int ret);
}

#if defined(MBEDTLS_DEBUG_C)
static void mbedtls_debug(void* ctx, int level, const char* file, int line, const char* str)
{
    switch (level)
    {
        case 0: dmLogError("%s:%d: %s", file, line, str); return;
        case 1: dmLogWarning("%s:%d: %s", file, line, str); return;
        case 2: dmLogDebug("%s:%d: %s", file, line, str); return;
    }
    dmLogInfo("%s:%d: %s", file, line, str);
}
#endif

struct CustomNetContext
{
    mbedtls_net_context m_Context;
    uint64_t            m_Timeout;
};

namespace dmSSLSocket
{
    struct SSLSocket
    {
        mbedtls_ssl_config*       m_MbedConf;
        mbedtls_ssl_context*      m_SSLContext;
        CustomNetContext*         m_SSLNetContext;
        uint64_t                  m_TimeStart;
        uint64_t                  m_TimeLimit1;
        uint64_t                  m_TimeLimit2;
    };
}

static void TimingSetDelay(dmSSLSocket::SSLSocket* socket, uint32_t int_ms, uint32_t fin_ms)
{
    socket->m_TimeStart = dmTime::GetMonotonicTime();
    socket->m_TimeLimit1 = int_ms;
    socket->m_TimeLimit2 = fin_ms;
}

static int TimingGetDelay(dmSSLSocket::SSLSocket* socket)
{
    if (socket->m_TimeLimit2 == 0)
        return -1;

    uint64_t t = dmTime::GetMonotonicTime();
    uint64_t elapsed_ms = (t - socket->m_TimeStart) / 1000;

    if (elapsed_ms >= socket->m_TimeLimit2)
        return 2;
    if (elapsed_ms >= socket->m_TimeLimit1)
        return 1;
    return 0;
}

static void TimingSetDelayCallback(void* data, uint32_t int_ms, uint32_t fin_ms)
{
    TimingSetDelay((dmSSLSocket::SSLSocket*)data, int_ms, fin_ms);
}

static int TimingGetDelayCallback(void* data)
{
    return TimingGetDelay((dmSSLSocket::SSLSocket*)data);
}

static int RecvTimeout(void* _ctx, unsigned char* buf, size_t len, uint32_t timeout)
{
    CustomNetContext* ctx = (CustomNetContext*)_ctx;
    int fd = ctx->m_Context.fd;

    if (fd < 0)
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;

    if (timeout == 0 && ctx->m_Timeout != 0)
    {
        timeout = ctx->m_Timeout / 1000;
    }

    dmFileDescriptor::Poller poller;
    dmFileDescriptor::PollerSetEvent(&poller, dmFileDescriptor::EVENT_READ, fd);
    int ret = dmFileDescriptor::Wait(&poller, timeout == 0 ? -1 : timeout);

    if (ret == 0)
        return MBEDTLS_ERR_SSL_TIMEOUT;

    if (ret < 0)
    {
#if ( defined(_WIN32) || defined(_WIN32_WCE) ) && !defined(EFIX64) && !defined(EFI32)
        if (WSAGetLastError() == WSAEINTR)
            return MBEDTLS_ERR_SSL_WANT_READ;
#else
        if (errno == EINTR)
            return MBEDTLS_ERR_SSL_WANT_READ;
#endif
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    return mbedtls_net_recv(ctx, buf, len);
}

namespace
{

#define MBEDTLS_RESULT_TO_STRING_CASE(x) case x: return #x;

const char* ResultToString(int ret)
{
    switch (ret)
    {
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_SOCKET_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_CONNECT_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_BIND_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_LISTEN_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_ACCEPT_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_RECV_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_SEND_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_CONN_RESET);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_UNKNOWN_HOST);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_BUFFER_TOO_SMALL);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_INVALID_CONTEXT);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_POLL_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_NET_BAD_INPUT_DATA);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_FEATURE_UNAVAILABLE);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_UNKNOWN_OID);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_FORMAT);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_VERSION);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_SERIAL);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_ALG);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_NAME);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_DATE);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_SIGNATURE);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_INVALID_EXTENSIONS);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_UNKNOWN_VERSION);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_UNKNOWN_SIG_ALG);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_SIG_MISMATCH);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_BAD_INPUT_DATA);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_ALLOC_FAILED);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_FILE_IO_ERROR);
        MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_FATAL_ERROR);
        default:
            return "Unknown error";
    }
}

#undef MBEDTLS_RESULT_TO_STRING_CASE

void FormatError(int ret, char* buffer, uint32_t buffer_size)
{
    mbedtls_strerror(ret, buffer, buffer_size);
}

dmSocket::Result SocketResultFromSSL(int ret)
{
    switch (ret)
    {
        case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
        case MBEDTLS_ERR_NET_CONN_RESET:
        case MBEDTLS_ERR_SSL_CLIENT_RECONNECT:
            return dmSocket::RESULT_CONNRESET;
        case MBEDTLS_ERR_SSL_TIMEOUT:
            return dmSocket::RESULT_WOULDBLOCK;
        case MBEDTLS_ERR_NET_RECV_FAILED:
            return dmSocket::RESULT_TRY_AGAIN;
        default:
            return dmSocket::RESULT_UNKNOWN;
    }
}

void ClearCertificateChain()
{
    if (g_MbedTlsContext.m_x509CertChain)
    {
        mbedtls_x509_crt_free(g_MbedTlsContext.m_x509CertChain);
        free((void*)g_MbedTlsContext.m_x509CertChain);
        g_MbedTlsContext.m_x509CertChain = 0;
    }
}

void ConfigureCertificateChain(mbedtls_ssl_config* conf)
{
    if (g_MbedTlsContext.m_x509CertChain == 0)
        return;

    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(conf, g_MbedTlsContext.m_x509CertChain, 0);
}

dmSSLSocket::Result InitializePSA()
{
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        dmLogError("psa_crypto_init failed: %d", (int)status);
        return dmSSLSocket::RESULT_SSL_INIT_FAILED;
    }
    return dmSSLSocket::RESULT_OK;
}

} // namespace

namespace dmSSLSocket
{

Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen)
{
    ClearCertificateChain();

    g_MbedTlsContext.m_x509CertChain = (mbedtls_x509_crt*)calloc(1, sizeof(mbedtls_x509_crt));
    if (!g_MbedTlsContext.m_x509CertChain)
    {
        return RESULT_UNKNOWN;
    }

    int ret = mbedtls_x509_crt_parse(g_MbedTlsContext.m_x509CertChain, key, keylen + 1);
    if (ret != 0)
    {
        char buffer[512] = "";
        FormatError(ret, buffer, sizeof(buffer));
        dmLogError("SSLSocket mbedtls_x509_crt_parse: %s0x%04x - %s", ret < 0 ? "-":"", ret < 0 ? -ret:ret, buffer);
        ClearCertificateChain();
        return RESULT_SSL_INIT_FAILED;
    }
    return RESULT_OK;
}

Result Delete(Socket socket)
{
    if (socket)
    {
        mbedtls_ssl_close_notify(socket->m_SSLContext);
        socket->m_SSLNetContext->m_Context.fd = -1;
        mbedtls_net_free((mbedtls_net_context*)socket->m_SSLNetContext);
        mbedtls_ssl_free(socket->m_SSLContext);
        mbedtls_ssl_config_free(socket->m_MbedConf);

        free(socket->m_MbedConf);
        free(socket->m_SSLNetContext);
        free(socket->m_SSLContext);
        free(socket);
    }
    return RESULT_OK;
}

Result New(dmSocket::Socket socket, const char* host, uint64_t timeout, Socket* sslsocket)
{
    uint64_t handshakestart = dmTime::GetMonotonicTime();

    Socket c = (Socket)malloc(sizeof(SSLSocket));
    memset(c, 0, sizeof(SSLSocket));

#define MBED_CALLOC(_TYPE) (_TYPE*)calloc(1, sizeof(_TYPE))
    c->m_MbedConf      = MBED_CALLOC(mbedtls_ssl_config);
    c->m_SSLContext    = MBED_CALLOC(mbedtls_ssl_context);
    c->m_SSLNetContext = MBED_CALLOC(CustomNetContext);
#undef MBED_CALLOC

    mbedtls_ssl_config_init(c->m_MbedConf);

#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(MBED_DEBUG_LEVEL);
    mbedtls_ssl_conf_dbg(c->m_MbedConf, mbedtls_debug, 0);
#endif

    int ret = 0;
    Result result = InitializePSA();
    if (result != RESULT_OK)
    {
        Delete(c);
        return result;
    }

    if ((ret = mbedtls_ssl_config_defaults(c->m_MbedConf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        SSL_LOGE("mbedtls_ssl_config_defaults failed", ret);
        Delete(c);
        return RESULT_SSL_INIT_FAILED;
    }

    mbedtls_ssl_conf_authmode(c->m_MbedConf, MBEDTLS_SSL_VERIFY_NONE);

    dmSocket::SetSendTimeout(socket, (int)timeout);
    dmSocket::SetReceiveTimeout(socket, (int)timeout);

#if defined(MBEDTLS_SSL_PROTO_DTLS)
    if (timeout != 0)
    {
        int mbed_ssl_timeout = dmMath::Max((int)timeout, dmSocket::SOCKET_TIMEOUT) / 1000;
        mbed_ssl_timeout = dmMath::Max(1, mbed_ssl_timeout);
        mbedtls_ssl_conf_handshake_timeout(c->m_MbedConf, 1, mbed_ssl_timeout);
    }
#endif

    c->m_SSLNetContext->m_Timeout = timeout;
    mbedtls_ssl_init(c->m_SSLContext);

    ConfigureCertificateChain(c->m_MbedConf);

    if ((ret = mbedtls_ssl_setup(c->m_SSLContext, c->m_MbedConf)) != 0)
    {
        SSL_LOGE("mbedtls_ssl_setup failed", ret);
        Delete(c);
        return RESULT_HANDSHAKE_FAILED;
    }

    if ((ret = mbedtls_ssl_set_hostname(c->m_SSLContext, host)) != 0)
    {
        SSL_LOGE("mbedtls_ssl_set_hostname failed", ret);
        Delete(c);
        return RESULT_HANDSHAKE_FAILED;
    }

    mbedtls_net_init((mbedtls_net_context*)c->m_SSLNetContext);
    c->m_SSLNetContext->m_Context.fd = dmSocket::GetFD(socket);

    mbedtls_ssl_set_bio(c->m_SSLContext, c->m_SSLNetContext, mbedtls_net_send, 0, RecvTimeout);
    mbedtls_ssl_set_timer_cb(c->m_SSLContext, c, TimingSetDelayCallback, TimingGetDelayCallback);

    do
    {
        ret = mbedtls_ssl_handshake(c->m_SSLContext);
    } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    uint64_t currenttime = dmTime::GetMonotonicTime();
    if (timeout > 0 && (int)(currenttime - handshakestart) > timeout)
    {
        ret = MBEDTLS_ERR_SSL_TIMEOUT;
    }

    if (ret != 0)
    {
        char buffer[512] = "";
        FormatError(ret, buffer, sizeof(buffer));
        dmLogError("SSLSocket mbedtls_ssl_handshake: %d - %s  %p", ret, buffer, c);
        Result result = RESULT_HANDSHAKE_FAILED;
        if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
        {
            dmLogError("Unable to verify the server's certificate.");
            result = RESULT_CONNREFUSED;
        }
        else if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
        {
            dmLogError("SSL handshake timeout");
            result = RESULT_WOULDBLOCK;
        }
        Delete(c);
        *sslsocket = 0;
        return result;
    }

    int flags = mbedtls_ssl_get_verify_result(c->m_SSLContext);
    if (flags != 0)
    {
        char vrfy_buf[512];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        dmLogError("mbedtls_ssl_get_verify_result failed:\n    %s\n", vrfy_buf);
        Delete(c);
        *sslsocket = 0;
        return RESULT_HANDSHAKE_FAILED;
    }

    *sslsocket = c;
    return RESULT_OK;
}

dmSocket::Result Send(Socket socket, const void* buffer, int length, int* sent_bytes)
{
    int r = mbedtls_ssl_write(socket->m_SSLContext, (const uint8_t*)buffer, length);

    if (r == MBEDTLS_ERR_SSL_WANT_WRITE || r == MBEDTLS_ERR_SSL_WANT_READ)
        return dmSocket::RESULT_TRY_AGAIN;

    if (r < 0)
    {
        mbedtls_ssl_session_reset(socket->m_SSLContext);
        dmSocket::Result socket_result = SocketResultFromSSL(r);
        if (socket_result == dmSocket::RESULT_UNKNOWN)
            SSL_LOGW("Unhandled ssl status code", r);
        return socket_result;
    }

    *sent_bytes = r;
    return dmSocket::RESULT_OK;
}

dmSocket::Result Receive(Socket socket, void* buffer, int length, int* received_bytes)
{
    int ret = mbedtls_ssl_read(socket->m_SSLContext, (unsigned char*)buffer, length - 1);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
        ret == MBEDTLS_ERR_SSL_WANT_WRITE ||
        ret == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS)
    {
        return dmSocket::RESULT_WOULDBLOCK;
    }

    if (ret == MBEDTLS_ERR_SSL_TIMEOUT)
        return dmSocket::RESULT_WOULDBLOCK;

    if (ret <= 0)
    {
        mbedtls_ssl_session_reset(socket->m_SSLContext);
        dmSocket::Result socket_result = SocketResultFromSSL(ret);
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

} // namespace dmSSLSocket

#if defined(MBEDTLS_THREADING_ALT)

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <mbedtls/threading.h>

static int dm_mbedtls_mutex_init(mbedtls_platform_mutex_t* mutex_wrapper)
{
    mutex_wrapper->mutex = dmMutex::New();
    return mutex_wrapper->mutex ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}

static void dm_mbedtls_mutex_free(mbedtls_platform_mutex_t* mutex_wrapper)
{
    if (mutex_wrapper->mutex != 0x0)
    {
        dmMutex::Delete((dmMutex::HMutex)mutex_wrapper->mutex);
        mutex_wrapper->mutex = 0x0;
    }
}

static int dm_mbedtls_mutex_lock(mbedtls_platform_mutex_t* mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    dmMutex::Lock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

static int dm_mbedtls_mutex_unlock(mbedtls_platform_mutex_t* mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    dmMutex::Unlock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

static int dm_mbedtls_condition_init(mbedtls_platform_condition_variable_t* condition_wrapper)
{
    condition_wrapper->condition = dmConditionVariable::New();
    return condition_wrapper->condition ? 0 : MBEDTLS_ERR_THREADING_USAGE_ERROR;
}

static void dm_mbedtls_condition_free(mbedtls_platform_condition_variable_t* condition_wrapper)
{
    if (condition_wrapper->condition != 0x0)
    {
        dmConditionVariable::Delete((dmConditionVariable::HConditionVariable)condition_wrapper->condition);
        condition_wrapper->condition = 0x0;
    }
}

static int dm_mbedtls_condition_signal(mbedtls_platform_condition_variable_t* condition_wrapper)
{
    if (condition_wrapper == 0x0 || condition_wrapper->condition == 0x0)
    {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    dmConditionVariable::Signal((dmConditionVariable::HConditionVariable)condition_wrapper->condition);
    return 0;
}

static int dm_mbedtls_condition_broadcast(mbedtls_platform_condition_variable_t* condition_wrapper)
{
    if (condition_wrapper == 0x0 || condition_wrapper->condition == 0x0)
    {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    dmConditionVariable::Broadcast((dmConditionVariable::HConditionVariable)condition_wrapper->condition);
    return 0;
}

static int dm_mbedtls_condition_wait(mbedtls_platform_condition_variable_t* condition_wrapper, mbedtls_platform_mutex_t* mutex_wrapper)
{
    if (condition_wrapper == 0x0 || condition_wrapper->condition == 0x0 || mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_USAGE_ERROR;
    }
    dmConditionVariable::Wait((dmConditionVariable::HConditionVariable)condition_wrapper->condition, (dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

namespace dmSSLSocket
{

Result Initialize()
{
    Result result = InitializePSA();
    if (result != RESULT_OK)
    {
        return result;
    }

    mbedtls_threading_set_alt(
        dm_mbedtls_mutex_init,
        dm_mbedtls_mutex_free,
        dm_mbedtls_mutex_lock,
        dm_mbedtls_mutex_unlock,
        dm_mbedtls_condition_init,
        dm_mbedtls_condition_free,
        dm_mbedtls_condition_signal,
        dm_mbedtls_condition_broadcast,
        dm_mbedtls_condition_wait
    );
    return RESULT_OK;
}

Result Finalize()
{
    ClearCertificateChain();
    mbedtls_threading_free_alt();
    return RESULT_OK;
}

} // namespace dmSSLSocket

#else

namespace dmSSLSocket
{

Result Initialize()
{
    return InitializePSA();
}

Result Finalize()
{
    ClearCertificateChain();
    return RESULT_OK;
}

} // namespace dmSSLSocket

#endif // MBEDTLS_THREADING_ALT

#undef SSL_LOGW
#undef SSL_LOGE
#undef MBED_DEBUG_LEVEL
