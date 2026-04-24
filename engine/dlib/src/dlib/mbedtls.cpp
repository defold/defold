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

#include "mbedtls_private.hpp"

#include <string.h>
#include <stdlib.h>

#include <mbedtls/base64.h>
#include <mbedtls/error.h>
#include <mbedtls/md5.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include "log.h"

namespace
{
    struct Context
    {
        mbedtls_x509_crt* m_x509CertChain;
    } g_MbedTlsContext;
}

namespace dmMbedTls
{

void HashSha1(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
{
    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx, (const unsigned char*)buf, (size_t)buflen);
    int ret = mbedtls_sha1_finish(&ctx, (unsigned char*)digest);
    mbedtls_sha1_free(&ctx);
    if (ret != 0) {
        memset(digest, 0, 20);
    }
}

void HashSha256(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
{
    int ret = mbedtls_sha256((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest, 0);
    if (ret != 0) {
        memset(digest, 0, 32);
    }
}

void HashSha512(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
{
    int ret = mbedtls_sha512((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest, 0);
    if (ret != 0) {
        memset(digest, 0, 64);
    }
}

void HashMd5(const uint8_t* buf, uint32_t buflen, uint8_t* digest)
{
    int ret = mbedtls_md5((const unsigned char*)buf, (size_t)buflen, (unsigned char*)digest);
    if (ret != 0) {
        memset(digest, 0, 16);
    }
}

bool Base64Encode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
{
    size_t out_len = 0;
    int r = mbedtls_base64_encode(dst, *dst_len, &out_len, src, src_len);
    if (r != 0)
    {
        if (*dst_len == 0)
            *dst_len = (uint32_t)out_len; // Seems to return 1 more than necessary, but better to err on the safe side! (see test_crypt.cpp)
        else
            *dst_len = 0xFFFFFFFF;
        return false;
    }
    *dst_len = (uint32_t)out_len;
    return true;
}

bool Base64Decode(const uint8_t* src, uint32_t src_len, uint8_t* dst, uint32_t* dst_len)
{
    size_t out_len = 0;
    int r = mbedtls_base64_decode(dst, *dst_len, &out_len, src, src_len);
    if (r != 0)
    {
        if (*dst_len == 0)
            *dst_len = (uint32_t)out_len;
        else
            *dst_len = 0xFFFFFFFF;
        return false;
    }
    *dst_len = (uint32_t)out_len;
    return true;
}

#define MBEDTLS_RESULT_TO_STRING_CASE(x) case x: return #x;

const char* ResultToString(int ret)
{
    switch(ret)
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
    MBEDTLS_RESULT_TO_STRING_CASE(MBEDTLS_ERR_X509_BUFFER_TOO_SMALL);
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
    switch (ret) {
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

dmSSLSocket::Result SetSslPublicKeys(const uint8_t* key, uint32_t keylen)
{
    ClearCertificateChain();

    g_MbedTlsContext.m_x509CertChain = (mbedtls_x509_crt*)calloc(1, sizeof(mbedtls_x509_crt));
    if (!g_MbedTlsContext.m_x509CertChain)
    {
        return dmSSLSocket::RESULT_UNKNOWN;
    }

    int ret = mbedtls_x509_crt_parse(g_MbedTlsContext.m_x509CertChain, key, keylen + 1);
    if (ret != 0)
    {
        char buffer[512] = "";
        FormatError(ret, buffer, sizeof(buffer));
        dmLogError("SSLSocket mbedtls_x509_crt_parse: %s0x%04x - %s", ret < 0 ? "-":"", ret < 0 ? -ret:ret, buffer);
        ClearCertificateChain();
        return dmSSLSocket::RESULT_SSL_INIT_FAILED;
    }
    return dmSSLSocket::RESULT_OK;
}

void ConfigureCertificateChain(void* ssl_config)
{
    if (g_MbedTlsContext.m_x509CertChain == 0)
        return;

    mbedtls_ssl_config* conf = (mbedtls_ssl_config*)ssl_config;
    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(conf, g_MbedTlsContext.m_x509CertChain, 0);
}

} // namespace dmMbedTls

#if defined(MBEDTLS_THREADING_ALT)

#include <mbedtls/threading.h>
#include <dlib/mutex.h>

static void dm_mbedtls_mutex_init(mbedtls_threading_mutex_t *mutex_wrapper)
{
    mutex_wrapper->mutex = dmMutex::New();
}

static void dm_mbedtls_mutex_free(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper->mutex != 0x0)
    {
        dmMutex::Delete((dmMutex::HMutex)mutex_wrapper->mutex);
    }
}

static int dm_mbedtls_mutex_lock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Lock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

static int dm_mbedtls_mutex_unlock(mbedtls_threading_mutex_t *mutex_wrapper)
{
    if (mutex_wrapper == 0x0 || mutex_wrapper->mutex == 0x0)
    {
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    dmMutex::Unlock((dmMutex::HMutex)mutex_wrapper->mutex);
    return 0;
}

namespace dmMbedTls
{

void Initialize()
{
    mbedtls_threading_set_alt(
        dm_mbedtls_mutex_init,
        dm_mbedtls_mutex_free,
        dm_mbedtls_mutex_lock,
        dm_mbedtls_mutex_unlock
    );
}

void Finalize()
{
    ClearCertificateChain();
    mbedtls_threading_free_alt();
}

} // namespace


#else

namespace dmMbedTls
{

void Initialize()
{
}

void Finalize()
{
    ClearCertificateChain();
}

} // namespace


#endif // MBEDTLS_THREADING_ALT
