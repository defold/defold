#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "socket.h"
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
        int m_ContentOffset;

        // Headers
        int      m_ContentLength;
        uint32_t m_Chunked : 1;

        Response(HClient client)
        {
            m_Client = client;
            m_ContentLength = -1;
            m_ContentOffset = -1;
            m_Chunked = 0;
        }
    };

    struct Client
    {
        dmSocket::Socket    m_Socket;
        dmSocket::Address   m_Address;
        uint16_t            m_Port;
        dmSocket::Result    m_SocketResult;

        void*               m_Userdata;
        HttpContent         m_HttpContent;
        HttpHeader          m_HttpHeader;
        int                 m_MaxGetRetries;

        // Used both for reading header and content. NOTE: Extra byte for null-termination
        char              m_Buffer[BUFFER_SIZE + 1];
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

        Client* client = new Client();

        client->m_Socket = dmSocket::INVALID_SOCKET;
        client->m_Address = address;
        client->m_Port = port;
        client->m_SocketResult = dmSocket::RESULT_OK;

        client->m_Userdata = params->m_Userdata;
        client->m_HttpContent = params->m_HttpContent;
        client->m_HttpHeader = params->m_HttpHeader;
        client->m_MaxGetRetries = 4;

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
        if (client->m_Socket != dmSocket::INVALID_SOCKET)
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
        if (client->m_Socket == dmSocket::INVALID_SOCKET)
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
    }

    static void HandleHeader(void* user_data, const char* key, const char* value)
    {
        Response* resp = (Response*) user_data;

        if (strcmp(key, "Content-Length") == 0)
        {
            resp->m_ContentLength = strtol(value, 0, 10);
        }
        else if (strcmp(key, "Transfer-Encoding") == 0 && strcmp(key, "chunked") == 0)
        {
            resp->m_Chunked = 1;
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

    static Result RecvAndParseHeaders(HClient client, Response* response, int* total_recv_out)
    {
        *total_recv_out = 0;
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

            total_recv += recv_bytes;

            if (r != dmSocket::RESULT_OK)
            {
                client->m_SocketResult = r;
                return RESULT_SOCKET_ERROR;
            }


            // NOTE: We have an extra byte for null-termination so no buffer overrun here.
            client->m_Buffer[total_recv] = '\0';

            dmHttpClientPrivate::ParseResult parse_res;
            parse_res = dmHttpClientPrivate::ParseHeader(client->m_Buffer, response, &HandleVersion, &HandleHeader, &HandleContent);
            if (parse_res == dmHttpClientPrivate::PARSE_RESULT_NEED_MORE_DATA)
            {
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

        *total_recv_out = total_recv;
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
        client->m_Socket = dmSocket::INVALID_SOCKET;
        return sock_res;
    }

    Result DoGet(HClient client, const char* path)
    {
        Response response(client);
        int total_content;

        client->m_SocketResult = dmSocket::RESULT_OK;
        Result r = Connect(client);
        if (r != RESULT_OK)
            return r;

        dmSocket::Result sock_res;
        sock_res = SendRequest(client, path);

        if (sock_res != dmSocket::RESULT_OK)
        {
            goto bail;
        }

        int total_recv;
        r = RecvAndParseHeaders(client, &response, &total_recv);
        if (r != RESULT_OK)
        {
            goto bail;
        }

        if (response.m_Chunked)
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET;
            return RESULT_UNSUPPORTED_TRANSFER_ENCODING;
        }

        if (response.m_ContentLength == -1)
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET;
            return RESULT_MISSING_CONTENT_LENGTH;
        }

        assert(response.m_ContentOffset != -1);
        client->m_HttpContent(client, client->m_Userdata, response.m_Status, client->m_Buffer + response.m_ContentOffset, total_recv - response.m_ContentOffset);

        total_content = total_recv - response.m_ContentOffset;
        while (total_content < response.m_ContentLength)
        {
            int recv_bytes;
            sock_res = dmSocket::Receive(client->m_Socket, client->m_Buffer, BUFFER_SIZE, &recv_bytes);
            if (sock_res == dmSocket::RESULT_OK)
            {
                if (recv_bytes == 0)
                {
                    break;
                }
                else
                {
                    client->m_HttpContent(client, client->m_Userdata, response.m_Status, client->m_Buffer, recv_bytes);
                    total_content += recv_bytes;
                }
            }
            else if (sock_res == dmSocket::RESULT_TRY_AGAIN)
            {

            }
            else
            {
                goto bail;
            }
        }

        if (total_content != response.m_ContentLength)
        {
            dmSocket::Delete(client->m_Socket);
            client->m_Socket = dmSocket::INVALID_SOCKET;
            return RESULT_PARTIAL_CONTENT;
        }

        if (response.m_Status == 200)
            return RESULT_OK;
        else
            return RESULT_NOT_200_OK;
bail:
        dmSocket::Delete(client->m_Socket);
        client->m_Socket = dmSocket::INVALID_SOCKET;
        return RESULT_SOCKET_ERROR;
    }

    Result Get(HClient client, const char* path)
    {
        Result r;
        for (int i = 0; i < client->m_MaxGetRetries; ++i)
        {
            r = DoGet(client, path);
            if (r == RESULT_SOCKET_ERROR && (client->m_SocketResult == dmSocket::RESULT_CONNRESET || client->m_SocketResult == dmSocket::RESULT_PIPE))
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


