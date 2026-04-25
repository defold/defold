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

#ifndef DM_MBEDTLS_USER_CONFIG_H
#define DM_MBEDTLS_USER_CONFIG_H

#if defined(DM_MBEDTLS_ERROR_STRERROR_DUMMY)
#undef MBEDTLS_ERROR_C
#if !defined(MBEDTLS_ERROR_STRERROR_DUMMY)
#define MBEDTLS_ERROR_STRERROR_DUMMY
#endif
#elif defined(DM_MBEDTLS_ERROR_C)
#if !defined(MBEDTLS_ERROR_C)
#define MBEDTLS_ERROR_C
#endif
#undef MBEDTLS_ERROR_STRERROR_DUMMY
#endif

#if defined(DM_MBEDTLS_TLS_CLIENT_ONLY)
#undef MBEDTLS_KEY_EXCHANGE_ECDHE_PSK_ENABLED
#undef MBEDTLS_KEY_EXCHANGE_PSK_ENABLED

#undef MBEDTLS_SSL_ALPN
#undef MBEDTLS_SSL_CACHE_C
#undef MBEDTLS_SSL_CONTEXT_SERIALIZATION
#undef MBEDTLS_SSL_COOKIE_C
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_KEYING_MATERIAL_EXPORT
#undef MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
#undef MBEDTLS_SSL_PROTO_DTLS
#undef MBEDTLS_SSL_RENEGOTIATION
#undef MBEDTLS_SSL_SESSION_TICKETS
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_TICKET_C
#undef MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED
#undef MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED

#undef MBEDTLS_PKCS7_C
#undef MBEDTLS_X509_CREATE_C
#undef MBEDTLS_X509_CRL_PARSE_C
#undef MBEDTLS_X509_CRT_WRITE_C
#undef MBEDTLS_X509_CSR_PARSE_C
#undef MBEDTLS_X509_CSR_WRITE_C
#endif

#endif
