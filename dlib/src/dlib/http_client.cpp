#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "math.h"
#include "./socket.h"
#include "./socks_proxy.h"
#include "http_client.h"
#include "http_client_private.h"
#include "log.h"

namespace dmHttpClient
{
    const int BUFFER_SIZE = 64 * 1024;

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
        uint32_t m_Chunked : 1;
        uint32_t m_CloseConnection : 1;

        Response(HClient client)
        {
            m_Client = client;
            m_ContentLength = -1;
            m_ContentOffset = -1;
            m_TotalReceived = 0;
            m_Chunked = 0;
            m_CloseConnection = 0;
        }
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
        dmSocket::Socket    m_Socket;
        dmSocket::Address   m_Address;
        uint16_t            m_Port;
        bool                m_UseSocksProxy;
        dmSocket::Result    m_SocketResult;

        void*               m_Userdata;
        HttpContent         m_HttpContent;
        HttpHeader          m_HttpHeader;
        int                 m_MaxGetRetries;

        // Used both for reading header and content. NOTE: Extra byte for null-termination
        char                m_Buffer[BUFFER_SIZE + 1];
    };

    static void DefaultHttpContentData(HClient client, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        (void) client;
        (void) user_data;
        (void) status_code;
        (void) content_data;
        (void) content_data_size;
    }

    void SetDefaultParams(NewParams* params)
    {
        params->m_HttpContent = &DefaultHttpContentData;
        params->m_HttpHeader = 0;
    }

    HClient New(const NewParams* params, const char* hostname, uint16_t port)
    {
        dmSocket::Address address;
        dmSocket::Result r = dmSocket::GetHostByName(hostname, &address);
        if (r != dmSocket::RESULT_OK)
            return 0;

        char* socks_proxy = getenv("DMSOCKS_PROXY");
        dmSocket::Address proxy_address = 0;
        if (socks_proxy)
        {
            r = dmSocket::GetHostByName(socks_proxy, &proxy_address);
            if (r != dmSocket::RESULT_OK)
            {
                dmLogWarning("Unable to IP for socks proxy: %s", socks_proxy);
                return 0;
            }
        }

        Client* client = new Client();

        client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
        client->m_Address = address;
        client->m_Port = port;
        client->m_SocketResult = dmSocket::RESULT_OK;

        client->m_Userdata = params->m_Userdata;
        client->m_HttpContent = params->m_HttpContent;
        client->m_HttpHeader = params->m_HttpHeader;
        client->m_MaxGetRetries = 4;

        client->m_UseSocksProxy = getenv("DMSOCKS_PROXY") != 0;

        return client;
    }

    Result SetOptionInt(HClient client, Option option, int value)
    {
        switch (option) {
            case OPTION_MAX_GET_RETRIES:
                if (value < 1)
                    return RESULT_INVAL_ERROR;
                client->m_MaxGetRetries = value;
                break;
            default:
                return RESULT_INVAL_ERROR;
        }

        return RESULT_OK;
    }

    void Delete(HClient client)
    {
        if (client->m_Socket != dmSocket::INVALID_SOCKET_HANDLE)
        {
            dmSocket::Delete(client->m_Socket);
        }
        delete client;
    }

    dmSocket::Result GetLastSocketResult(HClient client)
    {
        return client->m_SocketResult;
    }

    static Result Connect(HClient client)
    {
        if (client->m_Socket == dmSocket::INVALID_SOCKET_HANDLE)
        {
            if (client->m_UseSocksProxy)
            {
                dmSocksProxy::Result socks_result = dmSocksProxy::Connect(client->m_Address, client->m_Port, &client->m_Socket, &client->m_SocketResult);
                if (socks_result != dmSocksProxy::RESULT_OK)
                {
                    return RESULT_SOCKET_ERROR;
                }
            }
            else
            {
                dmSocket::Result sock_result = dmSocket::New(dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &client->m_Socket);
                if (sock_result != dmSocket::RESULT_OK)
                {
                    client->m_SocketResult = sock_result;
                    return RESULT_SOCKET_ERROR;
                }

                sock_result = dmSocket::Connect(client->m_Socket, client->m_Address, client->m_Port);
                if (sock_result != dmSocket::RESULT_OK)
                {
                    client->m_SocketResult = sock_result;
                    dmSocket::Delete(client->m_Socket);
                    return RESULT_SOCKET_ERROR;
                }
            }
        }

        return RESULT_OK;
    }

    static dmSocket::Result SendAll(dmSocket::Socket socket, const char* buffer, int length)
    {
        int total_sent_bytes = 0;
        int sent_bytes = 0;

        while (total_sent_bytes < length)
        {
            dmSocket::Result r = dmSocket::Send(socket, buffer + total_sent_bytes, length - total_sent_bytes, &sent_bytes);
            if (r == dmSocket::RESULT_TRY_AGAIN)
                continue;

            if (r != dmSocket::RESULT_OK)
            {
                return r;
            }

            total_sent_bytes += sent_bytes;
        }

        return dmSocket::RESULT_OK;
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
        if (strcmp(key, "Content-Length") == 0)
        {
            resp->m_ContentLength = strtol(value, 0, 10);
        }
        else if (strcmp(key, "Transfer-Encoding") == 0 && strcmp(value, "chunked") == 0)
        {
            resp->m_Chunked = 1;
        }
        else if (strcmp(key, "Connection") == 0 && strcmp(value, "close") == 0)
        {
            resp->m_CloseConnection = 1;
        }

        HClient c = resp->m_Client;
        if (c->m_HttpHeader)
        {
            resp->m_Client->m_HttpHeader(c, c->m_Userdata, resp->m_Status, key, value);
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
        int total_recv = 0;

        while (1)
        {
            int max_to_recv = BUFFER_SIZE - total_recv;

            if (max_to_recv <= 0)
            {
                return RESULT_HTTP_HEADERS_ERROR;
            }

            int recv_bytes;
            dmSocket::Result r = dmSocket::Receive(client->m_Socket, client->m_Buffer + total_recv, max_to_recv, &recv_bytes);
            if (r == dmSocket::RESULT_TRY_AGAIN)
                continue;

            if (r != dmSocket::RESULT_OK)
            {
                client->m_SocketResult = r;
                return RESULT_SOCKET_ERROR;
            }

            total_recv += recv_bytes;

            // NOTE: We have an extra byte for null-termination so no buffer overrun here.
            client->m_Buffer[total_recv] = '\0';

            dmHttpClientPrivate::ParseResult parse_res;
            parse_res = dmHttpClientPrivate::ParseHeader(client->m_Buffer, response, &HandleVersion, &HandleHeader, &HandleContent);
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

        response->m_TotalReceived = total_recv;
        return RESULT_OK;
    }

#define HTTP_CLIENT_SENDALL_AND_BAIL(s) \
sock_res = SendAll(client->m_Socket, s, strlen(s));\
if (sock_res != dmSocket::RESULT_OK)\
{\
    client->m_SocketResult = sock_res;\
    goto bail;\
}\


    static dmSocket::Result SendRequest(HClient client, const char* path)
    {
        dmSocket::Result sock_res;
        HTTP_CLIENT_SENDALL_AND_BAIL("GET ")
        HTTP_CLIENT_SENDALL_AND_BAIL(path)
        HTTP_CLIENT_SENDALL_AND_BAIL(" HTTP/1.1\r\n")
        HTTP_CLIENT_SENDALL_AND_BAIL("Host: foo.com\r\n")
        HTTP_CLIENT_SENDALL_AND_BAIL("\r\n\r\n")

        return sock_res;
bail:
        dmSocket::Delete(client->m_Socket);
        client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
        return sock_res;
    }

    static Result DoTransfer(HClient client, Response* response, int to_transfer, HttpContent http_content)
    {
        int total_transferred = 0;

        while (true)
        {
            int n = dmMath::Min(to_transfer - total_transferred, response->m_TotalReceived - response->m_ContentOffset);
            http_content(client, client->m_Userdata, response->m_Status, client->m_Buffer + response->m_ContentOffset, n);
            total_transferred += n;
            assert(total_transferred <= to_transfer);
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
            dmSocket::Result sock_res = dmSocket::Receive(client->m_Socket, client->m_Buffer, BUFFER_SIZE, &recv_bytes);
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

            }
            else
            {
                return RESULT_SOCKET_ERROR;
            }
        }
        assert(total_transferred <= to_transfer);

        if (total_transferred != to_transfer)
        {
            return RESULT_PARTIAL_CONTENT;
        }
        else
        {
            return RESULT_OK;
        }
    }

    static void HttpContentConsume(HClient client, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
    {
        (void) client;
        (void) user_data;
        (void) status_code;
        (void) content_data;
        (void) content_data_size;
    }

    Result DoGet(HClient client, const char* path)
    {
        Response response(client);

        client->m_SocketResult = dmSocket::RESULT_OK;
        Result r = Connect(client);
        if (r != RESULT_OK)
            return r;

        dmSocket::Result sock_res;
        sock_res = SendRequest(client, path);

        if (sock_res != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            return RESULT_SOCKET_ERROR;
        }

        r = RecvAndParseHeaders(client, &response);
        if (r != RESULT_OK)
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            return r;
        }

        if (response.m_Chunked)
        {
            // Ok
        }
        else if (response.m_ContentLength == -1)
        {
            // If not chunked we require Content-Length attribute to be set.
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            return RESULT_MISSING_CONTENT_LENGTH;
        }

        if (response.m_Chunked)
        {
            // Chunked encoding
            // Move actual data to the beginning of the buffer
            memmove(client->m_Buffer, client->m_Buffer + response.m_ContentOffset, response.m_TotalReceived - response.m_ContentOffset);

            response.m_TotalReceived = response.m_TotalReceived - response.m_ContentOffset;
            response.m_ContentOffset = 0;

            int chunk_size;
            int chunk_number = 0;
            while(true)
            {
                chunk_size = 0;
                // NOTE: We have an extra byte for null-termination so no buffer overrun here.
                client->m_Buffer[response.m_TotalReceived] = '\0';

                char* chunk_size_end = strstr(client->m_Buffer, "\r\n");
                if (chunk_size_end)
                {
                    // We found a chunk
                    sscanf(client->m_Buffer, "%x", &chunk_size);
                    chunk_size_end += 2; // "\r\n"

                    // Move content-offset after chunk termination, ie after "\r\n"
                    response.m_ContentOffset = chunk_size_end - client->m_Buffer;
                    r = DoTransfer(client, &response, chunk_size, client->m_HttpContent);
                    if (r != RESULT_OK)
                        break;

                    // Consume \r\n"
                    r = DoTransfer(client, &response, 2, &HttpContentConsume);
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
                    int max_to_recv = BUFFER_SIZE - response.m_TotalReceived;

                    if (max_to_recv <= 0)
                    {
                        dmSocket::Delete(client->m_Socket);
                        client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
                        return RESULT_HTTP_HEADERS_ERROR;
                    }

                    int recv_bytes;
                    dmSocket::Result sock_r = dmSocket::Receive(client->m_Socket, client->m_Buffer + response.m_TotalReceived, max_to_recv, &recv_bytes);
                    if (sock_r == dmSocket::RESULT_TRY_AGAIN)
                        continue;

                    if (sock_r != dmSocket::RESULT_OK)
                    {
                        dmSocket::Delete(client->m_Socket);
                        client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
                        return RESULT_SOCKET_ERROR;
                    }
                    response.m_TotalReceived += recv_bytes;
                }
            }
        }
        else
        {
            // "Regular" transfer, single chunk
            assert(response.m_ContentOffset != -1);
            r = DoTransfer(client, &response, response.m_ContentLength, client->m_HttpContent);
        }

        assert(response.m_TotalReceived == 0);

        if (r == RESULT_OK)
        {
            if (response.m_CloseConnection)
            {
                dmSocket::Delete(client->m_Socket);
                client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
            }

            if (response.m_Status == 200)
                return RESULT_OK;
            else
                return RESULT_NOT_200_OK;
        }
        else
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET_HANDLE;
        }
        return r;
    }

    Result Get(HClient client, const char* path)
    {
        Result r;
        for (int i = 0; i < client->m_MaxGetRetries; ++i)
        {
            r = DoGet(client, path);
            if (r == RESULT_UNEXPECTED_EOF ||
               (r == RESULT_SOCKET_ERROR && (client->m_SocketResult == dmSocket::RESULT_CONNRESET || client->m_SocketResult == dmSocket::RESULT_PIPE)))
            {
                // Try again
                dmLogInfo("HTTPCLIENT: Connection lost, reconnecting.");
            }
            else
            {
                return r;
            }
        }
        return r;
    }

#undef HTTP_CLIENT_SENDALL_AND_BAIL

} // namespace dmHttpClient


