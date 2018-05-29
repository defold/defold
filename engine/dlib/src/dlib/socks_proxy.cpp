#include "socks_proxy.h"

#include "socket.h"

#include <stdlib.h>

namespace dmSocksProxy
{
    struct Request
    {
        uint8_t            m_Version;
        uint8_t            m_CommandCode;
        uint16_t           m_Port;
        uint32_t           m_Address;
        char               m_UserID; // No support for userid. Null terminated string.
    };

    struct Response
    {
        uint8_t  m_NullByte;
        uint8_t  m_Status;
        uint16_t m_Pad1;
        uint32_t m_Pad2;
    };

    Result Connect(dmSocket::Address address, int port, dmSocket::Socket* socket, dmSocket::Result* socket_result)
    {
        if (address.m_family != dmSocket::DOMAIN_IPV4)
        {
            // Socks v4 doesn't support IPv6.
            return RESULT_SOCKET_ERROR;
        }

        char* socks_proxy = getenv("DMSOCKS_PROXY");
        if (!socks_proxy)
            return RESULT_NO_DMSOCKS_PROXY_SET;

        int socks_proxy_port = 1080;
        char* socks_proxy_port_str = getenv("DMSOCKS_PROXY_PORT");
        if (socks_proxy_port_str)
            socks_proxy_port = strtol(socks_proxy_port_str, 0, 10);

        dmSocket::Address proxy_address;
        dmSocket::Result sock_res = dmSocket::GetHostByName(socks_proxy, &proxy_address);
        if (sock_res != dmSocket::RESULT_OK)
        {
            if (socket_result)
                *socket_result = sock_res;
            return RESULT_SOCKET_ERROR;
        }

        sock_res = dmSocket::New(proxy_address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, socket);
        if (sock_res != dmSocket::RESULT_OK)
        {
            if (socket_result)
                *socket_result = sock_res;
            return RESULT_SOCKET_ERROR;
        }

        sock_res = dmSocket::Connect(*socket, proxy_address, socks_proxy_port);
        if (sock_res != dmSocket::RESULT_OK)
        {
            if (socket_result)
            {
                dmSocket::Delete(*socket);
                *socket_result = sock_res;
            }
            return RESULT_SOCKET_ERROR;
        }

        Request request = { 0x04, 0x01, htons(port), *dmSocket::IPv4(&address), 0 };
        // NOTE: Due to alignment calculate size. Do *not* use sizeof(.)
        int request_size = 1 + 1 + 2 + 4 + 1;

        const char* buf = (const char*) &request;
        int total_sent = 0;
        while (total_sent < request_size)
        {
            int sent_bytes;
            sock_res = dmSocket::Send(*socket, buf + total_sent, request_size - total_sent, &sent_bytes);
            if (sock_res == dmSocket::RESULT_TRY_AGAIN || sock_res == dmSocket::RESULT_OK)
            {
                total_sent += sent_bytes;
            }
            else
            {
                if (socket_result)
                {
                    dmSocket::Delete(*socket);
                    *socket_result = sock_res;
                }
                return RESULT_SOCKET_ERROR;
            }
        }

        Response response;
        int total_recv = 0;
        char* recv_buf = (char*) &response;
        while (total_recv < (int) sizeof(response))
        {
            int recv_bytes;
            sock_res = dmSocket::Receive(*socket, recv_buf + total_recv, sizeof(response) - total_recv, &recv_bytes);
            if (sock_res == dmSocket::RESULT_TRY_AGAIN || sock_res == dmSocket::RESULT_OK)
            {
                if (recv_bytes == 0)
                {
                    // Connection closed
                    return RESULT_INVALID_SERVER_RESPONSE;
                }
                total_recv += recv_bytes;
            }
            else
            {
                if (socket_result)
                {
                    dmSocket::Delete(*socket);
                    *socket_result = sock_res;
                }
                return RESULT_SOCKET_ERROR;
            }
        }

        switch (response.m_Status)
        {
            case 0x5a:
                return RESULT_OK;
            case 0x5b:
                return RESULT_REQUEST_FAILED;
            case 0x5c:
                return RESULT_NOT_REACHABLE;
            case 0x5d:
                return RESULT_INVALID_USER_ID;
            default:
                return RESULT_INVALID_SERVER_RESPONSE;
        }
    }
}
