#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "math.h"
#include "./socket.h"
#include "./socks_proxy.h"
#include "http_client.h"
#include "http_client_private.h"
#include "log.h"
#include "sys.h"
#include "dstrings.h"
#include "uri.h"
#include "path.h"
#include "time.h"
#include "connection_pool.h"
#include "mutex.h"
#include "../axtls/ssl/os_port.h"
#include "../axtls/ssl/ssl.h"

namespace dmHttpClient
{
    // Be careful with this buffer. See comment in Receive about ssl_read.
    const int BUFFER_SIZE = 64 * 1024;

    const unsigned int HTTP_CLIENT_MAXIMUM_CACHE_AGE = 30U * 24U * 60U * 60U; // 30 days

    const uint32_t MAX_POOL_CONNECTIONS = 32;

    const int SOCKET_TIMEOUT = 500 * 1000;

    // TODO: This is not good. Singleton like stuff
    // that requires a lock for initialization
    // See comment in GetPool()
    struct PoolCreator
    {
        PoolCreator()
        {
            m_Pool = 0;
            m_Mutex = dmMutex::New();
        }

        ~PoolCreator()
        {
            dmMutex::Lock(m_Mutex);
            if (m_Pool) {
                dmConnectionPool::Delete(m_Pool);
            }
            dmMutex::Unlock(m_Mutex);
            dmMutex::Delete(m_Mutex);
        }

        // Create the pool lazily for two resons
        // 1. If dlib is imported from loaded from python it will crash and burn
        //    in OpenSSL as the we several symbol conflics, e.g. RSA_free, and
        //    the version from OpenSSL will be used instead of axTLS!
        // 2. Startup performance. Minor in this context
        dmConnectionPool::HPool GetPool()
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            if (m_Pool == 0) {
                dmConnectionPool::Params params;
                params.m_MaxConnections = MAX_POOL_CONNECTIONS;
                dmConnectionPool::Result r = dmConnectionPool::New(&params, &m_Pool);
                assert(r == dmConnectionPool::RESULT_OK);
            }
            return m_Pool;
        }

        dmConnectionPool::HPool GetPoolNoCreate()
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            return m_Pool;
        }

    private:
        dmConnectionPool::HPool m_Pool;
        dmMutex::Mutex          m_Mutex;
    };

    PoolCreator g_PoolCreator;

    struct Response
    {
        HClient m_Client;
        int m_Major;
        int m_Minor;
        int m_Status;

        // Offset to actual content in Client.m_Buffer,
        // ie after meta-data such as http-headers or chunk-size for transferring data with chunked encoding.
        int m_ContentOffset;
        // Total amount of data received in Client.m_Buffer
        int m_TotalReceived;

        // Headers
        int      m_ContentLength;
        char     m_ETag[64];
        uint32_t m_Chunked : 1;
        uint32_t m_CloseConnection : 1;
        uint32_t m_MaxAge;

        // Cache
        dmHttpCache::HCacheCreator m_CacheCreator;

        // Connection
        dmConnectionPool::HPool         m_Pool;
        dmConnectionPool::HConnection   m_Connection;
        dmSocket::Socket                m_Socket;
        SSL*                            m_SSLConnection;

        Response(HClient client)
        {
            m_Client = client;
            m_Major = 0;
            m_Minor = 0;
            m_Status = 0;
            m_ContentLength = -1;
            m_ETag[0] = '\0';
            m_ContentOffset = -1;
            m_TotalReceived = 0;
            m_Chunked = 0;
            m_CloseConnection = 0;
            m_MaxAge = 0;
            m_CacheCreator = 0;
            m_Pool = 0;
            m_Connection = 0;
            m_Socket = 0;
            m_SSLConnection = 0;
        }
        Result Connect(const char* host, uint16_t port, bool secure, int timeout);
        ~Response();
    };

    /*
     Client.m_Buffer layout

     |-------------------------------------------------------------------------|
     |                    |                               |                    |
     |      Meta-data     |        Content                |                    |X (extra byte for null termination)
     |                    |                               |                    |
     |-------------------------------------------------------------------------|
         m_ContentOffset ->             m_TotalReceived ->          BUFFER_SIZE->
     */

    struct Client
    {
        char*               m_Hostname;
        char                m_URI[1024];
        dmSocket::Result    m_SocketResult;

        void*               m_Userdata;
        HttpContent         m_HttpContent;
        HttpHeader          m_HttpHeader;
        HttpSendContentLength m_HttpSendContentLength;
        HttpWrite           m_HttpWrite;
        HttpWriteHeaders    m_HttpWriteHeaders;
        int                 m_MaxGetRetries;
        int                 m_RequestTimeout;
        uint64_t            m_RequestStart;
        Statistics          m_Statistics;

        dmHttpCache::HCache m_HttpCache;

        bool                m_Secure;
        uint16_t            m_Port;

        // Used both for reading header and content. NOTE: Extra byte for null-termination
        char                m_Buffer[BUFFER_SIZE + 1];
    };

    Result Response::Connect(const char* host, uint16_t port, bool secure, int timeout)
    {
        m_Pool = g_PoolCreator.GetPool();
        dmConnectionPool::Result r = dmConnectionPool::Dial(m_Pool, host, port, secure, timeout, &m_Connection, &m_Client->m_SocketResult);

        if (r == dmConnectionPool::RESULT_OK) {

            m_Socket = dmConnectionPool::GetSocket(m_Pool, m_Connection);
            m_SSLConnection = (SSL*) dmConnectionPool::GetSSLConnection(m_Pool, m_Connection);

            dmSocket::SetSendTimeout(m_Socket, SOCKET_TIMEOUT);
            dmSocket::SetReceiveTimeout(m_Socket, SOCKET_TIMEOUT);

            return RESULT_OK;
        } else {
            return RESULT_SOCKET_ERROR;
        }
    }

    Response::~Response()
    {
        if (m_Connection) {
            if (m_CloseConnection || m_Client->m_SocketResult != dmSocket::RESULT_OK) {
                dmConnectionPool::Close(m_Pool, m_Connection);
            } else {
                dmConnectionPool::Return(m_Pool, m_Connection);
            }
        }
    }

    static void DefaultHttpContentData(HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        (void) response;
        (void) user_data;
        (void) status_code;
        (void) content_data;
        (void) content_data_size;
    }

    void SetDefaultParams(NewParams* params)
    {
        params->m_HttpContent = &DefaultHttpContentData;
        params->m_HttpHeader = 0;
        params->m_HttpSendContentLength = 0;
        params->m_HttpWrite = 0;
        params->m_HttpWriteHeaders = 0;
        params->m_HttpCache = 0;
    }

    HClient New(const NewParams* params, const char* hostname, uint16_t port, bool secure)
    {
        dmSocket::Address address;
        dmSocket::Result r = dmSocket::GetHostByName(hostname, &address);
        if (r != dmSocket::RESULT_OK)
            return 0;

        Client* client = new Client();

        client->m_Hostname = strdup(hostname);
        client->m_SocketResult = dmSocket::RESULT_OK;

        client->m_Userdata = params->m_Userdata;
        client->m_HttpContent = params->m_HttpContent;
        client->m_HttpHeader = params->m_HttpHeader;
        client->m_HttpSendContentLength = params->m_HttpSendContentLength;
        client->m_HttpWrite = params->m_HttpWrite;
        client->m_HttpWriteHeaders = params->m_HttpWriteHeaders;
        client->m_MaxGetRetries = 1;
        client->m_RequestTimeout = 0;
        client->m_RequestStart = 0;
        memset(&client->m_Statistics, 0, sizeof(client->m_Statistics));
        client->m_HttpCache = params->m_HttpCache;
        client->m_Secure = secure;
        client->m_Port = port;

        return client;
    }

    HClient New(const NewParams* params, const char* hostname, uint16_t port)
    {
        return New(params, hostname, port, false);
    }

    Result SetOptionInt(HClient client, Option option, int64_t value)
    {
        switch (option) {
            case OPTION_MAX_GET_RETRIES:
                if (value < 1)
                    return RESULT_INVAL_ERROR;
                client->m_MaxGetRetries = (int) value;
                break;
            case OPTION_REQUEST_TIMEOUT:
                client->m_RequestTimeout = (int) value;
                break;
            default:
                return RESULT_INVAL_ERROR;
        }
        return RESULT_OK;
    }

    void Delete(HClient client)
    {
        free(client->m_Hostname);
        delete client;
    }

    dmSocket::Result GetLastSocketResult(HClient client)
    {
        return client->m_SocketResult;
    }

    static bool HasRequestTimedOut(HClient client)
    {
        if( client->m_RequestTimeout == 0 )
            return false;
        uint64_t currenttime = dmTime::GetTime();
        return int(currenttime - client->m_RequestStart) >= client->m_RequestTimeout;
    }

    static dmSocket::Result SSLToSocket(int r) {
        // Currently a very limited list but
        // SSL_ERROR_CONN_LOST -> RESULT_CONNRESET is essential
        // for the reconnection functionality/logic.
        // The majority of the error codes in axTLS can't
        // be translated into dmSocket::Result and must be
        // handled specifically, e.g. ssl_handshake_status and RESULT_HANDSHAKE_FAILED
        // above.
        switch (r) {
            case SSL_CLOSE_NOTIFY:
            case SSL_ERROR_CONN_LOST:
                return dmSocket::RESULT_CONNRESET;
            default:
                dmLogWarning("Unhandled ssl status code: %d", r);
                // We interpret dmSocket::RESULT_UNKNOWN as something unexpected
                // a abort the request
                return dmSocket::RESULT_UNKNOWN;
        }
    }

    static dmSocket::Result SendAll(Response* response, const char* buffer, int length)
    {
        if (response->m_SSLConnection != 0) {
            int r = ssl_write(response->m_SSLConnection, (const uint8_t*) buffer, length);

            // In order to mimic the http code path, we return the same error number
            if( (r == length) && HasRequestTimedOut(response->m_Client) )
            {
                return dmSocket::RESULT_WOULDBLOCK;
            }

            if (r != length) {
                return SSLToSocket(r);
            }
            return dmSocket::RESULT_OK;
        } else {
            int total_sent_bytes = 0;
            int sent_bytes = 0;

            while (total_sent_bytes < length) {
                dmSocket::Result r = dmSocket::Send(response->m_Socket, buffer + total_sent_bytes, length - total_sent_bytes, &sent_bytes);

                if( r == dmSocket::RESULT_WOULDBLOCK )
                {
                    r = dmSocket::RESULT_TRY_AGAIN;
                }
                if( (r == dmSocket::RESULT_OK || r == dmSocket::RESULT_TRY_AGAIN) && HasRequestTimedOut(response->m_Client) )
                {
                    r = dmSocket::RESULT_WOULDBLOCK;
                }

                if (r == dmSocket::RESULT_TRY_AGAIN)
                    continue;

                if (r != dmSocket::RESULT_OK) {
                    return r;
                }

                total_sent_bytes += sent_bytes;
            }
            return dmSocket::RESULT_OK;
        }
    }

    static dmSocket::Result Receive(Response* response, void* buffer, int length, int* received_bytes)
    {
        if (response->m_SSLConnection != 0) {
            uint8_t* buf = 0;

            int r;
            do {
                // NOTE: "Manual" implementation of timeout as ssl_read() in axTLS
                // returns 0 (SSL_OK) when either
                // a) Too little ssl protocol data is available
                // b) The underlying socket returns EAGAIN (EWOULDBLOCK)
                // Instead of changing axTLS we measure actual time here.
                // ssl_read should return a code that represents EAGAIN (EWOULDBLOCK)
                uint64_t start = dmTime::GetTime();
                r = ssl_read(response->m_SSLConnection, &buf);
                uint64_t end = dmTime::GetTime();

                int timeout = response->m_Client->m_RequestTimeout;
                if (timeout > 0 && (end - start) > (uint64_t)SOCKET_TIMEOUT) {
                    return dmSocket::RESULT_WOULDBLOCK;
                }
            } while (r == SSL_OK);

            if (r >= 0) {
                if (r > length) {
                    // NOTE: This might be fragile but ssl_read doens't
                    // have a max-to-read parameter.
                    // Given that the internal buffer size (BUFFER_SIZE) is
                    // set to 64k this shouldn't be an issue in practice
                    // as we consume the buffer as fast as we can.
                    // buf must be less than sizeof(ssl->bm_all_data)
                    // which is 17408
                    dmLogError("ssl_read() returned a too large buffer");
                    return dmSocket::RESULT_UNKNOWN;
                }
                int to_copy = r > length ? length : r;
                *received_bytes = to_copy;
                memcpy(buffer, buf, to_copy);
                return dmSocket::RESULT_OK;
            } else {
                return SSLToSocket(r);
            }
        } else {
            return dmSocket::Receive(response->m_Socket, buffer, length, received_bytes);
        }
    }

    static void HandleVersion(void* user_data, int major, int minor, int status, const char* status_str)
    {
        Response* resp = (Response*) user_data;
        resp->m_Major = major;
        resp->m_Minor = minor;
        resp->m_Status = status;

        if ((major << 16 | minor) < (1 << 16 | 1))
        {
            // Close connection for HTTP protocol version < 1.1
            resp->m_CloseConnection = 1;
        }
    }

    static void HandleHeader(void* user_data, const char* key, const char* value)
    {
        Response* resp = (Response*) user_data;

        if (dmStrCaseCmp(key, "Content-Length") == 0)
        {
            resp->m_ContentLength = strtol(value, 0, 10);
        }
        else if (dmStrCaseCmp(key, "Transfer-Encoding") == 0 && dmStrCaseCmp(value, "chunked") == 0)
        {
            resp->m_Chunked = 1;
        }
        else if (dmStrCaseCmp(key, "Connection") == 0 && dmStrCaseCmp(value, "close") == 0)
        {
            resp->m_CloseConnection = 1;
        }
        else if (dmStrCaseCmp(key, "ETag") == 0)
        {
            dmStrlCpy(resp->m_ETag, value, sizeof(resp->m_ETag));
        }
        else if (dmStrCaseCmp(key, "Cache-Control") == 0)
        {
            const char* substr = "max-age=";
            const char* max_age = strstr(value, "max-age=");
            if (max_age) {
                max_age += strlen(substr);
                resp->m_MaxAge = dmMath::Max(0, atoi(max_age));
                if (resp->m_MaxAge > HTTP_CLIENT_MAXIMUM_CACHE_AGE)
                {
                    resp->m_MaxAge = HTTP_CLIENT_MAXIMUM_CACHE_AGE;
                }
            }
        }

        HClient c = resp->m_Client;
        if (c->m_HttpHeader)
        {
            resp->m_Client->m_HttpHeader(resp, c->m_Userdata, resp->m_Status, key, value);
        }
    }

    static void HandleContent(void* user_data, int offset)
    {
        Response* resp = (Response*) user_data;
        resp->m_ContentOffset = offset;
    }

    static Result RecvAndParseHeaders(HClient client, Response* response)
    {
        response->m_TotalReceived = 0;

        while (1)
        {
            int max_to_recv = BUFFER_SIZE - response->m_TotalReceived;

            if (max_to_recv <= 0)
            {
                return RESULT_HTTP_HEADERS_ERROR;
            }

            int recv_bytes;
            dmSocket::Result r = Receive(response, client->m_Buffer + response->m_TotalReceived, max_to_recv, &recv_bytes);

            if( r == dmSocket::RESULT_WOULDBLOCK )
            {
                r = dmSocket::RESULT_TRY_AGAIN;
            }
            if( (r == dmSocket::RESULT_OK || r == dmSocket::RESULT_TRY_AGAIN) && HasRequestTimedOut(client) )
            {
                r = dmSocket::RESULT_WOULDBLOCK;
            }

            if (r == dmSocket::RESULT_TRY_AGAIN)
                continue;

            if (r != dmSocket::RESULT_OK)
            {
                client->m_SocketResult = r;
                return RESULT_SOCKET_ERROR;
            }

            response->m_TotalReceived += recv_bytes;

            // NOTE: We have an extra byte for null-termination so no buffer overrun here.
            client->m_Buffer[response->m_TotalReceived] = '\0';

            // According to the HTTP spec, all responses should end with double line feed to indicate end of headers
            // Unfortunately some server implementations only end with one linefeed if the response is '204 No Content' so we take special measures
            // to force parsing of headers if we have received no more data and the last time we tried we ended up with a 204 status
            bool force_parsing_of_headers = response->m_Status == 204 && recv_bytes == 0;

            dmHttpClientPrivate::ParseResult parse_res;
            parse_res = dmHttpClientPrivate::ParseHeader(client->m_Buffer, response, force_parsing_of_headers, &HandleVersion, &HandleHeader, &HandleContent);
            if (parse_res == dmHttpClientPrivate::PARSE_RESULT_NEED_MORE_DATA)
            {
                if (recv_bytes == 0)
                {
                    dmLogWarning("Unexpected eof for socket connection.");
                    return RESULT_UNEXPECTED_EOF;
                }
                continue;
            }
            else if (parse_res == dmHttpClientPrivate::PARSE_RESULT_SYNTAX_ERROR)
            {
                return RESULT_HTTP_HEADERS_ERROR;
            }
            else if (parse_res == dmHttpClientPrivate::PARSE_RESULT_OK)
            {
                break;
            }
            else
            {
                assert(0);
            }
        }

        return RESULT_OK;
    }

    Result Write(HResponse response, const void* buffer, uint32_t buffer_size)
    {
        HClient client = response->m_Client;
        if (client->m_SocketResult != dmSocket::RESULT_OK) {
            return RESULT_SOCKET_ERROR;
        }
        dmSocket::Result sock_res = SendAll(response, (const char*) buffer, buffer_size);
        if (sock_res != dmSocket::RESULT_OK)
        {
            client->m_SocketResult = sock_res;
            return RESULT_SOCKET_ERROR;
        }

        return RESULT_OK;
    }

    Result WriteHeader(HResponse response, const char* name, const char* value)
    {
        HClient client = response->m_Client;
        if (client->m_SocketResult != dmSocket::RESULT_OK) {
            return RESULT_SOCKET_ERROR;
        }
        dmSocket::Result sock_res;

        // DEF-2889 most webservers have a header length limit of 8096 bytes
        char buf[8096];
        const int bufsize = sizeof(buf);
        if(DM_SNPRINTF(buf, bufsize, "%s: %s\r\n", name, value) > bufsize) {
            dmLogWarning("Truncated HTTP request header %s since it was larger than %d", name, bufsize);
        }

        sock_res = SendAll(response, buf, strlen(buf));
        if (sock_res != dmSocket::RESULT_OK) {
            client->m_SocketResult = sock_res;
            return RESULT_SOCKET_ERROR;
        }
        return RESULT_OK;
    }

#define HTTP_CLIENT_SENDALL_AND_BAIL(s) \
sock_res = SendAll(response, s, strlen(s));\
if (sock_res != dmSocket::RESULT_OK)\
{\
    client->m_SocketResult = sock_res;\
    goto bail;\
}\


    static dmSocket::Result SendRequest(HClient client, HResponse response, const char* encoded_path, const char* method)
    {
        dmSocket::Result sock_res;
        uint32_t send_content_length = 0;

        HTTP_CLIENT_SENDALL_AND_BAIL(method);
        HTTP_CLIENT_SENDALL_AND_BAIL(" ")
        HTTP_CLIENT_SENDALL_AND_BAIL(encoded_path)
        HTTP_CLIENT_SENDALL_AND_BAIL(" HTTP/1.1\r\n")
        HTTP_CLIENT_SENDALL_AND_BAIL("Host: ");
        HTTP_CLIENT_SENDALL_AND_BAIL(client->m_Hostname);
        HTTP_CLIENT_SENDALL_AND_BAIL("\r\n");
        if (client->m_HttpWriteHeaders) {
            Result header_result = client->m_HttpWriteHeaders(response, client->m_Userdata);
            if (header_result != RESULT_OK) {
                goto bail;
            }
        }
        if (client->m_HttpCache)
        {
            char etag[64];
            dmHttpCache::Result cache_result = dmHttpCache::GetETag(client->m_HttpCache, client->m_URI, etag, sizeof(etag));
            if (cache_result == dmHttpCache::RESULT_OK)
            {
                HTTP_CLIENT_SENDALL_AND_BAIL("If-None-Match: ");
                HTTP_CLIENT_SENDALL_AND_BAIL(etag);
                HTTP_CLIENT_SENDALL_AND_BAIL("\r\n");
            }
        }

        if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0) {
            send_content_length = client->m_HttpSendContentLength(response, client->m_Userdata);
            HTTP_CLIENT_SENDALL_AND_BAIL("Content-Length: ");
            char buf[64];
            DM_SNPRINTF(buf, sizeof(buf), "%d", send_content_length);
            HTTP_CLIENT_SENDALL_AND_BAIL(buf);
            HTTP_CLIENT_SENDALL_AND_BAIL("\r\n");
        }

        HTTP_CLIENT_SENDALL_AND_BAIL("\r\n")

        if (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0)
        {
            Result post_result = client->m_HttpWrite(response, client->m_Userdata);
            if (post_result != RESULT_OK) {
                goto bail;
            }
        }

        return client->m_SocketResult;
bail:
        return client->m_SocketResult;
    }

    static Result DoTransfer(HClient client, Response* response, int to_transfer, HttpContent http_content, bool add_to_cache)
    {
        // to_transfer can be set to -1 when the "Content-Length" is unknown
        int total_transferred = 0;

        while (true)
        {
            int n;
            if (to_transfer == -1) {
                // Unknown "Content-Length". Read as much as we can.
                n = response->m_TotalReceived - response->m_ContentOffset;
            } else {
                n = dmMath::Min(to_transfer - total_transferred, response->m_TotalReceived - response->m_ContentOffset);
            }
            http_content(response, client->m_Userdata, response->m_Status, client->m_Buffer + response->m_ContentOffset, n);

            if (response->m_CacheCreator && add_to_cache)
            {
                dmHttpCache::Add(client->m_HttpCache, response->m_CacheCreator, client->m_Buffer + response->m_ContentOffset, n);
            }

            total_transferred += n;
            assert(total_transferred <= to_transfer || to_transfer == -1);
            response->m_ContentOffset += n;

            if (total_transferred == to_transfer)
            {
                // Move "extra" bytes to buffer start
                memmove(client->m_Buffer, client->m_Buffer + response->m_ContentOffset, response->m_TotalReceived - response->m_ContentOffset);
                response->m_TotalReceived = response->m_TotalReceived - response->m_ContentOffset;
                response->m_ContentOffset = 0;
                break;
            }

            assert(response->m_TotalReceived - response->m_ContentOffset == 0);
            response->m_ContentOffset = 0;
            response->m_TotalReceived = 0;

            int recv_bytes;
            dmSocket::Result sock_res = Receive(response, client->m_Buffer, BUFFER_SIZE, &recv_bytes);

            if( sock_res == dmSocket::RESULT_WOULDBLOCK )
            {
                sock_res = dmSocket::RESULT_TRY_AGAIN;
            }
            if( (sock_res == dmSocket::RESULT_OK || sock_res == dmSocket::RESULT_TRY_AGAIN) && HasRequestTimedOut(client) )
            {
                sock_res = dmSocket::RESULT_WOULDBLOCK;
            }

            if (sock_res == dmSocket::RESULT_OK)
            {
                if (recv_bytes == 0)
                {
                    break;
                }
                else
                {
                    response->m_TotalReceived = recv_bytes;
                }
            }
            else if (sock_res == dmSocket::RESULT_TRY_AGAIN)
            {
                // Continue
            }
            else if (sock_res == dmSocket::RESULT_CONNRESET)
            {
                // Break out of loop and handle below
                break;
            }
            else
            {
                return RESULT_SOCKET_ERROR;
            }
        }
        assert(total_transferred <= to_transfer || to_transfer == -1);

        if (total_transferred != to_transfer && to_transfer != -1)
        {
            return RESULT_PARTIAL_CONTENT;
        }
        else
        {
            return RESULT_OK;
        }
    }

    static void HttpContentConsume(HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        (void) response;
        (void) user_data;
        (void) status_code;
        (void) content_data;
        (void) content_data_size;
    }

    static Result HandleCached(HClient client, const char* path, Response* response)
    {
        client->m_Statistics.m_CachedResponses++;

        if (client->m_HttpCache == 0)
        {
            dmLogFatal("Got HTTP response NOT MODIFIED (304) but no cache present. Server error?");
            return RESULT_IO_ERROR;
        }
        dmHttpCache::Result cache_result;

        char cache_etag[64];
        cache_etag[0] = '\0';
        cache_result = dmHttpCache::GetETag(client->m_HttpCache, client->m_URI, cache_etag, sizeof(cache_etag));

        if (cache_result != dmHttpCache::RESULT_OK)
        {
            dmLogFatal("Got HTTP response NOT MODIFIED (304) but no ETag present. Server error?");
            return RESULT_IO_ERROR;
        }

        if (response->m_ETag[0] != '\0')
        {
            // The Entity Tag (ETag) is optional in HTTP 1.1.
            // It is the servers responsibility to verify the ETag in case it is included. The
            // ETag is a mechanism to reduce the bandwidth required by the server, not
            // by the client. There is no guarantee that an ETag will be included in a
            // 403 response even though it's been sent with a previous 200 response.
            // Even though it might be possible at times, the client has no formal responsibility
            // to perform this verification.
            if (strcmp(cache_etag, response->m_ETag) != 0)
            {
                dmLogFatal("ETag mismatch (%s vs %s)", cache_etag, response->m_ETag);
                return RESULT_IO_ERROR;
            }
        }

        FILE* file = 0;
        uint64_t checksum;
        cache_result = dmHttpCache::Get(client->m_HttpCache, client->m_URI, cache_etag, &file, &checksum);
        if (cache_result == dmHttpCache::RESULT_OK)
        {
            // NOTE: We have an extra byte for null-termination so no buffer overrun here.
            size_t nread;
            do
            {
                nread = fread(client->m_Buffer, 1, BUFFER_SIZE, file);
                client->m_Buffer[nread] = '\0';
                client->m_HttpContent(response, client->m_Userdata, response->m_Status, client->m_Buffer, nread);
            }
            while (nread > 0);
            dmHttpCache::Release(client->m_HttpCache, client->m_URI, cache_etag, file);
        }
        else
        {
            return RESULT_IO_ERROR;
        }

        dmHttpCache::SetVerified(client->m_HttpCache, client->m_URI, true);

        return RESULT_OK;
    }

    static Result HandleResponse(HClient client, const char* path, const char* method, Response* response)
    {
        Result r = RESULT_OK;

        client->m_HttpContent(response, client->m_Userdata, response->m_Status, 0, 0);

        if (strcmp(method, "HEAD") == 0) {
            // A response from a HEAD request should not attempt to read any body despite
            // content length being non-zero, but we still call DoTransfer (with a
            // content length of 0) to ensure that the response is setup properly
            r = DoTransfer(client, response, 0, client->m_HttpContent, true);
        }
        else if (response->m_Chunked)
        {
            // Chunked encoding
            // Move actual data to the beginning of the buffer
            memmove(client->m_Buffer, client->m_Buffer + response->m_ContentOffset, response->m_TotalReceived - response->m_ContentOffset);

            response->m_TotalReceived = response->m_TotalReceived - response->m_ContentOffset;
            response->m_ContentOffset = 0;

            int chunk_size;
            int chunk_number = 0;
            while(true)
            {
                chunk_size = 0;
                // NOTE: We have an extra byte for null-termination so no buffer overrun here.
                client->m_Buffer[response->m_TotalReceived] = '\0';

                char* chunk_size_end = strstr(client->m_Buffer, "\r\n");
                if (chunk_size_end)
                {
                    // We found a chunk
                    sscanf(client->m_Buffer, "%x", &chunk_size);
                    chunk_size_end += 2; // "\r\n"

                    // Move content-offset after chunk termination, ie after "\r\n"
                    response->m_ContentOffset = chunk_size_end - client->m_Buffer;
                    r = DoTransfer(client, response, chunk_size, client->m_HttpContent, true);
                    if (r != RESULT_OK)
                        break;

                    // Consume \r\n"
                    // NOTE: *not* added to cache
                    r = DoTransfer(client, response, 2, &HttpContentConsume, false);
                    if (r != RESULT_OK)
                        break;

                    if (chunk_size == 0)
                    {
                        r = RESULT_OK;
                        break;
                    }

                    ++chunk_number;
                }
                else
                {
                    // We need more data
                    int max_to_recv = BUFFER_SIZE - response->m_TotalReceived;

                    if (max_to_recv <= 0)
                    {
                        return RESULT_HTTP_HEADERS_ERROR;
                    }

                    int recv_bytes;
                    dmSocket::Result sock_r = Receive(response, client->m_Buffer + response->m_TotalReceived, max_to_recv, &recv_bytes);

                    if( sock_r == dmSocket::RESULT_WOULDBLOCK )
                    {
                        sock_r = dmSocket::RESULT_TRY_AGAIN;
                    }
                    if( (sock_r == dmSocket::RESULT_OK || sock_r == dmSocket::RESULT_TRY_AGAIN) && HasRequestTimedOut(client) )
                    {
                        sock_r = dmSocket::RESULT_WOULDBLOCK;
                    }

                    if (sock_r == dmSocket::RESULT_TRY_AGAIN)
                        continue;

                    if (sock_r != dmSocket::RESULT_OK)
                    {
                        return RESULT_SOCKET_ERROR;
                    }
                    response->m_TotalReceived += recv_bytes;
                }
            }
        }
        else
        {
            // "Regular" transfer, single chunk
            assert(response->m_ContentOffset != -1);
            r = DoTransfer(client, response, response->m_ContentLength, client->m_HttpContent, true);
        }

        return r;
    }

    static Result DoDoRequest(HClient client, Response& response, const char* path, const char* method)
    {
        dmSocket::Result sock_res;

        sock_res = SendRequest(client, &response, path, method);

        if (sock_res != dmSocket::RESULT_OK)
        {
            return RESULT_SOCKET_ERROR;
        }

        Result r = RecvAndParseHeaders(client, &response);
        if (r != RESULT_OK)
        {
            response.m_CloseConnection = 1;
            return r;
        }

        if (response.m_Chunked)
        {
            // Ok
        }
        else if (response.m_ContentLength == -1 && response.m_Status != 304)
        {
            // When not chunked (and not 304) we used to require Content-Length attribute to be set
            // but changed to support this behavior in order to be compatible with old/exotic web-servers.
            // The connection is however closed as keep-alive isn't possible for this case.
            response.m_CloseConnection = 1;
        }

        if (response.m_Status == 304 /* NOT MODIFIED */)
        {
            // Use cached version
            if (response.m_ContentLength == 0 || response.m_ContentLength == -1)
            {
                r = HandleCached(client, path, &response);
                response.m_TotalReceived = 0;
            }
            else
            {
                // Cached version can't have payload
                if (response.m_ContentLength != -1)
                    dmLogWarning("Unexpected Content-Length: %d for NOT MODIFIED response (304)", response.m_ContentLength);
                r = RESULT_INVALID_RESPONSE;
            }
        }
        else
        {
            // Non-cached response
            if (client->m_HttpCache && response.m_Status == 200 /* OK */)
            {
                if (response.m_ETag[0] != '\0') {
                    dmHttpCache::Begin(client->m_HttpCache, client->m_URI, response.m_ETag, &response.m_CacheCreator);
                } else if (response.m_MaxAge > 0) {
                    dmHttpCache::Begin(client->m_HttpCache, client->m_URI, response.m_MaxAge, &response.m_CacheCreator);
                }
            }

            r = HandleResponse(client, path, method, &response);

            if (response.m_CacheCreator)
            {
                dmHttpCache::End(client->m_HttpCache, response.m_CacheCreator);
                response.m_CacheCreator = 0;
            }
        }

        assert(response.m_TotalReceived == 0);

        if (r == RESULT_OK)
        {
            if (response.m_Status == 200)
                return RESULT_OK;
            else
                return RESULT_NOT_200_OK;
        }
        return r;
    }

    static Result DoRequest(HClient client, const char* path, const char* method)
    {
        // Theoretically we can be in a state where every
        // connections in the pool is closed by the remote peer
        // but not yet closed on the client side (max-keep-alive)
        // Therefore we must loop MAX_POOL_CONNECTIONS + 1 times.
        // The assumption holds only for a single-threaded environment though
        // but as network errors will occur in practice the heuristic is probably
        // "good enough".
        for (uint32_t i = 0; i < MAX_POOL_CONNECTIONS + 1; ++i) {
            Response response(client);
            client->m_Statistics.m_Responses++;

            client->m_SocketResult = dmSocket::RESULT_OK;
            Result r = response.Connect(client->m_Hostname, client->m_Port, client->m_Secure, client->m_RequestTimeout);
            if (r != RESULT_OK) {
                return r;
            }

            if( HasRequestTimedOut(client) )
            {
                return r;
            }

            r = DoDoRequest(client, response, path, method);
            if (r != RESULT_OK && r != RESULT_NOT_200_OK) {

                if( HasRequestTimedOut(client) )
                {
                    return r;
                }

                response.m_CloseConnection = 1;
                uint32_t count = dmConnectionPool::GetReuseCount(response.m_Pool, response.m_Connection);

                if (count > 0 && response.m_TotalReceived == 0) {
                    client->m_Statistics.m_Reconnections++;
                    // We assume that the connection was closed by remote peer
                    // as the total received data is zero bytes.
                    // Moreover, the case when the connection was closed by the remote
                    // peer is only interesting if the connection is reused, i.e. count > 0
                    // Otherwise, we have to regard this type of connection shutdown as
                    // an error.

                    // implicit continue here
                } else {
                    return r;
                }
            } else {
                return r;
            }
        }
        dmLogWarning("All connection attempts to remote host are prematurely closed. This error is very unlikely.");
        return RESULT_UNKNOWN;
    }

    static Result HandleCachedVerified(HClient client, const dmHttpCache::EntryInfo* info)
    {
        Response response(client);
        client->m_Statistics.m_DirectFromCache++;

        FILE* file = 0;
        uint64_t checksum;

        dmHttpCache::Result cache_result = dmHttpCache::Get(client->m_HttpCache, client->m_URI, info->m_ETag, &file, &checksum);
        if (cache_result == dmHttpCache::RESULT_OK)
        {
            // NOTE: We have an extra byte for null-termination so no buffer overrun here.
            size_t nread;
            do
            {
                nread = fread(client->m_Buffer, 1, BUFFER_SIZE, file);
                client->m_Buffer[nread] = '\0';
                client->m_HttpContent(&response, client->m_Userdata, 304, client->m_Buffer, nread);
            }
            while (nread > 0);
            dmHttpCache::Release(client->m_HttpCache, client->m_URI, info->m_ETag, file);
            return RESULT_NOT_200_OK;
        }
        else
        {
            return RESULT_IO_ERROR;
        }
    }

    Result Get(HClient client, const char* path)
    {
        DM_SNPRINTF(client->m_URI, sizeof(client->m_URI), "%s://%s:%d/%s", client->m_Secure ? "https" : "http", client->m_Hostname, (int) client->m_Port, path);
        client->m_RequestStart = dmTime::GetTime();

        Result r;

        if (client->m_HttpCache)
        {
            dmHttpCache::ConsistencyPolicy policy = dmHttpCache::GetConsistencyPolicy(client->m_HttpCache);
            dmHttpCache::EntryInfo info;
            dmHttpCache::Result cache_r = dmHttpCache::GetInfo(client->m_HttpCache, client->m_URI, &info);
            if (cache_r == dmHttpCache::RESULT_OK) {
                bool ok_etag = info.m_Verified && policy == dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE;
                if ((ok_etag || info.m_Valid)) {
                    // We have a cache and trust the content of the cache
                    // OR
                    // the entry is valid in terms of max-age
                    r = HandleCachedVerified(client, &info);
                    if (r == RESULT_NOT_200_OK) {
                        return r;
                    }
                }
            }
        }

        for (int i = 0; i < client->m_MaxGetRetries; ++i)
        {
            r = DoRequest(client, path, "GET");
            if (r == RESULT_UNEXPECTED_EOF ||
               (r == RESULT_SOCKET_ERROR && (client->m_SocketResult == dmSocket::RESULT_CONNRESET
                                          || client->m_SocketResult == dmSocket::RESULT_WOULDBLOCK
                                          || client->m_SocketResult == dmSocket::RESULT_PIPE)))
            {
                // Try again
                if (i < client->m_MaxGetRetries - 1) {
                    client->m_Statistics.m_Reconnections++;
                    client->m_RequestStart = dmTime::GetTime();
                    dmLogInfo("HTTPCLIENT: Connection lost, reconnecting. (%d/%d)", i + 1, client->m_MaxGetRetries - 1);
                }
            }
            else
            {
                return r;
            }
        }
        return r;
    }

    Result Post(HClient client, const char* path)
    {
        return Request(client, "POST", path);
    }

    Result Request(HClient client, const char* method, const char* path)
    {
        if (strcmp(method, "GET") == 0) {
            return Get(client, path);
        } else {
            DM_SNPRINTF(client->m_URI, sizeof(client->m_URI), "%s://%s:%d/%s", client->m_Secure ? "https" : "http", client->m_Hostname, (int) client->m_Port, path);
            client->m_RequestStart = dmTime::GetTime();
            Result r = DoRequest(client, path, method);
            return r;
        }
    }

    void GetStatistics(HClient client, Statistics* statistics)
    {
        *statistics = client->m_Statistics;
    }

    dmHttpCache::HCache GetHttpCache(HClient client)
    {
        return client->m_HttpCache;
    }

    uint32_t ShutdownConnectionPool()
    {
        dmConnectionPool::HPool pool = g_PoolCreator.GetPoolNoCreate();
        if (pool) {
            return dmConnectionPool::Shutdown(pool, dmSocket::SHUTDOWNTYPE_READWRITE);
        }
        return 0;
    }

    void ReopenConnectionPool()
    {
        dmConnectionPool::HPool pool = g_PoolCreator.GetPool();
        dmConnectionPool::Reopen(pool);
    }

#undef HTTP_CLIENT_SENDALL_AND_BAIL

} // namespace dmHttpClient
