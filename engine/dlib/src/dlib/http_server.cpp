#include "http_server.h"

#include "array.h"
#include "dstrings.h"
#include "http_server_private.h"
#include "log.h"
#include "math.h"
#include "network_constants.h"
#include "time.h"

#include <stdlib.h>

namespace dmHttpServer
{
    const uint32_t BUFFER_SIZE = 64 * 1024;

    struct Connection
    {
        dmSocket::Socket m_Socket;
        uint16_t         m_RequestCount;
        uint64_t         m_ConnectionTimeStart;
    };

    struct Server
    {
        Server()
        {
            m_ServerSocket = dmSocket::INVALID_SOCKET_HANDLE;
            m_Reconnect = 0;
        }
        dmSocket::Address   m_Address;
        uint16_t            m_Port;
        HttpHeader          m_HttpHeader;
        HttpResponse        m_HttpResponse;
        void*               m_Userdata;

        // Connection timeout in useconds. NOTE: In params it is specified in seconds.
        uint64_t            m_ConnectionTimeout;
        dmArray<Connection> m_Connections;
        dmSocket::Socket    m_ServerSocket;
        // Receive and send buffer
        char                m_Buffer[BUFFER_SIZE];

        uint32_t            m_Reconnect : 1;
    };

    struct InternalRequest
    {
        Request  m_Request;
        Result   m_Result;
        dmSocket::Socket m_Socket;
        Server*  m_Server;

        char     m_Method[16];
        char     m_Resource[128];

        int      m_StatusCode;

        // Offset to where content start in buffer. This the extra content read while parsing headers
        // The value is adjusted in Receive when data is consumed
        uint32_t m_ContentOffset;
        // Total amount of data received in Server.m_Buffer
        uint32_t m_TotalReceived;

        // Total content received, ie the payload
        uint32_t m_TotalContentReceived;

        // Number of bytes in send buffer
        uint32_t m_SendBufferPos;

        uint16_t m_CloseConnection : 1;
        uint16_t m_HeaderSent : 1;
        uint16_t m_AttributesSent : 1;

        InternalRequest()
        {
            memset(this, 0, sizeof(*this));
            m_StatusCode = 200;
        }
    };

    static void FlushSendBuffer(const Request* request);

    void SetDefaultParams(struct NewParams* params)
    {
        memset(params, 0, sizeof(*params));
        params->m_MaxConnections = 16;
        params->m_ConnectionTimeout = 60;
    }

    static void Disconnect(Server* server)
    {
        if (server->m_ServerSocket != dmSocket::INVALID_SOCKET_HANDLE)
        {
            dmSocket::Delete(server->m_ServerSocket);
            server->m_ServerSocket = dmSocket::INVALID_SOCKET_HANDLE;
        }
    }

    static Result Connect(Server* server, uint16_t port)
    {
        dmSocket::Socket socket = -1;
        dmSocket::Address bind_address;
        dmSocket::Result r = dmSocket::RESULT_OK;

        Disconnect(server);

        r = dmSocket::GetHostByName(DM_UNIVERSAL_BIND_ADDRESS_IPV4, &bind_address);
        if (r != dmSocket::RESULT_OK)
        {
            return RESULT_SOCKET_ERROR;
        }

        r = dmSocket::New(bind_address.m_family, dmSocket::TYPE_STREAM, dmSocket::PROTOCOL_TCP, &socket);
        if (r != dmSocket::RESULT_OK)
        {
            return RESULT_UNKNOWN;
        }

        dmSocket::SetReuseAddress(socket, true);

        r = dmSocket::Bind(socket, bind_address, port);
        if (r != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return RESULT_SOCKET_ERROR;
        }

        r = dmSocket::Listen(socket, 32);
        if (r != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return RESULT_SOCKET_ERROR;
        }

        dmSocket::Address address;
        uint16_t actual_port;
        r = dmSocket::GetName(socket, &address, &actual_port);
        if (r != dmSocket::RESULT_OK)
        {
            dmSocket::Delete(socket);
            return RESULT_SOCKET_ERROR;
        }

        server->m_Address = address;
        server->m_Port = actual_port;
        server->m_ServerSocket = socket;

        return RESULT_OK;
    }

    Result New(const NewParams* params, uint16_t port, HServer* server)
    {
        *server = 0;

        if (!params->m_HttpResponse)
            return RESULT_ERROR_INVAL;

        Server* ret = new Server();
        if (Connect(ret, port) != RESULT_OK)
        {
            delete ret;
            return RESULT_SOCKET_ERROR;
        }

        ret->m_HttpHeader = params->m_HttpHeader;
        ret->m_HttpResponse = params->m_HttpResponse;
        ret->m_Userdata = params->m_Userdata;
        ret->m_ConnectionTimeout = params->m_ConnectionTimeout * 1000000U;
        ret->m_Connections.SetCapacity(params->m_MaxConnections);

        *server = ret;
        return RESULT_OK;
    }

    void Delete(HServer server)
    {
        // TODO: Shutdown connections
        dmSocket::Delete(server->m_ServerSocket);
        delete server;
    }

    static void HandleRequest(void* user_data, const char* request_method, const char* resource, int major, int minor)
    {
        InternalRequest* req = (InternalRequest*) user_data;

        req->m_Request.m_Major = major;
        req->m_Request.m_Minor = minor;

        dmStrlCpy(req->m_Method, request_method, sizeof(req->m_Method));
        dmStrlCpy(req->m_Resource, resource, sizeof(req->m_Resource));

        if ((major << 16 | minor) < (1 << 16 | 1))
        {
            // Close connection for HTTP protocol version < 1.1
            req->m_CloseConnection = 1;
        }
    }

    static void HandleHeader(void* user_data, const char* key, const char* value)
    {
        InternalRequest* req = (InternalRequest*) user_data;

        if (dmStrCaseCmp(key, "Content-Length") == 0)
        {
            req->m_Request.m_ContentLength = strtol(value, 0, 10);
        }
        else if (dmStrCaseCmp(key, "Connection") == 0 && dmStrCaseCmp(value, "close") == 0)
        {
            req->m_CloseConnection = 1;
        }

        Server* server = req->m_Server;

        if (server->m_HttpHeader)
            server->m_HttpHeader(server->m_Userdata, key, value);
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

    static const char* StatusCodeString(int status_code)
    {
        switch (status_code)
        {
            case 200:
                return "OK";
            case 404:
                return "Not Found";
            case 500:
                return "Internal Server Error";
            case 302:
                return "Found";
            default:
                dmLogWarning("Unsupported status code: %d", status_code);
                return "";
        }
    }

#define HTTP_SERVER_SENDALL_AND_BAIL(s) \
    r = SendAll(internal_req->m_Socket, s, strlen(s));\
    if (r != dmSocket::RESULT_OK)\
        goto bail;\

    static void SendHeader(InternalRequest* internal_req)
    {
        dmSocket::Result r;
        internal_req->m_HeaderSent = 1;

        char header[128];
        DM_SNPRINTF(header, sizeof(header), "HTTP/1.1 %d %s\r\n",
                    internal_req->m_StatusCode,
                    StatusCodeString(internal_req->m_StatusCode));

        HTTP_SERVER_SENDALL_AND_BAIL(header)
        return;
bail:
        internal_req->m_Result = RESULT_SOCKET_ERROR;
        return;
    }

    static void SendAttributes(InternalRequest* internal_req)
    {
        internal_req->m_AttributesSent = 1;

        dmSocket::Result r;
        const char* chunked_header = "Transfer-Encoding: chunked\r\n";

        HTTP_SERVER_SENDALL_AND_BAIL("Server: Dynamo 1.0\r\n")
        if (internal_req->m_CloseConnection)
            HTTP_SERVER_SENDALL_AND_BAIL("Connection: close\r\n")
        HTTP_SERVER_SENDALL_AND_BAIL(chunked_header)
        HTTP_SERVER_SENDALL_AND_BAIL("\r\n")

        return;
bail:
        internal_req->m_Result = RESULT_SOCKET_ERROR;
        return;
    }

    static void HandleReponse(void* user_data, int offset)
    {
        InternalRequest* internal_req = (InternalRequest*) user_data;
        Request* request = &internal_req->m_Request;

        Server* server = internal_req->m_Server;
        internal_req->m_ContentOffset = offset;

        dmSocket::Result r;

        request->m_Method = internal_req->m_Method;
        request->m_Resource = internal_req->m_Resource;
        request->m_Internal = internal_req;
        server->m_HttpResponse(server->m_Userdata, request);

        if (internal_req->m_Result == RESULT_OK &&
            internal_req->m_TotalContentReceived != internal_req->m_Request.m_ContentLength)
        {
            dmLogWarning("Actual content differs from expected content-length (%d != %d)",
                    internal_req->m_TotalContentReceived,
                    internal_req->m_Request.m_ContentLength);
            goto bail;
        }

        // Send headers and attributes even if no data is sent
        if (!internal_req->m_HeaderSent)
            SendHeader(internal_req);

        if (!internal_req->m_AttributesSent)
            SendAttributes(internal_req);

        FlushSendBuffer(request);

        HTTP_SERVER_SENDALL_AND_BAIL("0\r\n\r\n")

        return;
bail:
        internal_req->m_Result = RESULT_SOCKET_ERROR;
        return;
    }

    Result SetStatusCode(const Request* request, int status_code)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_HeaderSent)
        {
            dmLogError("Set status code is only valid before any data is sent");
            return RESULT_ERROR_INVAL;
        }

        internal_req->m_StatusCode = status_code;
        return RESULT_OK;
    }

    static void FlushSendBuffer(const Request* request)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        dmSocket::Result r;
        if (internal_req->m_SendBufferPos > 0)
        {
            uint32_t data_length = internal_req->m_SendBufferPos;
            internal_req->m_SendBufferPos = 0;

            char buf[16];
            DM_SNPRINTF(buf, sizeof(buf), "%x", data_length);
            HTTP_SERVER_SENDALL_AND_BAIL(buf);
            HTTP_SERVER_SENDALL_AND_BAIL("\r\n")

            r = SendAll(internal_req->m_Socket, (const char*) internal_req->m_Server->m_Buffer, data_length);
            if (r != dmSocket::RESULT_OK)
                goto bail;

            HTTP_SERVER_SENDALL_AND_BAIL("\r\n")
        }

        return;
bail:
        internal_req->m_Result = RESULT_SOCKET_ERROR;
    }

    Result Send(const Request* request, const void* data, uint32_t data_length)
    {
        // Do not send empty chunks as the empty chunk is
        // interpreted as the last and terminating chunk
        if (data_length == 0) {
            return RESULT_OK;
        }

        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_Result != RESULT_OK)
            return internal_req->m_Result;

        if (!internal_req->m_HeaderSent)
            SendHeader(internal_req);

        if (!internal_req->m_AttributesSent)
            SendAttributes(internal_req);

        if (internal_req->m_Result != RESULT_OK)
            return internal_req->m_Result;

        uint32_t total_sent = 0;
        internal_req->m_Result = RESULT_OK;

        while (total_sent < data_length && internal_req->m_Result == RESULT_OK)
        {
            uint32_t to_send = dmMath::Min(BUFFER_SIZE - internal_req->m_SendBufferPos, data_length - total_sent);
            memcpy(internal_req->m_Server->m_Buffer + internal_req->m_SendBufferPos, (char*) data + total_sent, to_send);
            internal_req->m_SendBufferPos += to_send;
            if (internal_req->m_SendBufferPos == BUFFER_SIZE)
            {
                FlushSendBuffer(request);
            }
            total_sent += to_send;
        }
        return internal_req->m_Result;
    }

    Result SendAttribute(const Request* request, const char* key, const char* value)
    {
        dmSocket::Result r;
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;

        if (internal_req->m_AttributesSent)
        {
            dmLogError("SendAttribute is only valid before any data is sent");
            return RESULT_ERROR_INVAL;
        }

        if (!internal_req->m_HeaderSent)
            SendHeader(internal_req);

        HTTP_SERVER_SENDALL_AND_BAIL(key)
        HTTP_SERVER_SENDALL_AND_BAIL(":")
        HTTP_SERVER_SENDALL_AND_BAIL(value)
        HTTP_SERVER_SENDALL_AND_BAIL("\r\n")

        internal_req->m_Result = RESULT_OK;
        return internal_req->m_Result;
bail:
        internal_req->m_Result = RESULT_SOCKET_ERROR;
        return internal_req->m_Result;
    }

#undef HTTP_SERVER_SENDALL_AND_BAIL

    Result Receive(const Request* request, void* buffer, uint32_t buffer_size, uint32_t* received_bytes)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_Result != RESULT_OK)
            return internal_req->m_Result;

        uint32_t total = 0;
        assert(internal_req->m_TotalReceived >= internal_req->m_ContentOffset);
        uint32_t bytes_in_buffer = internal_req->m_TotalReceived - internal_req->m_ContentOffset;
        if (bytes_in_buffer > 0)
        {
            uint32_t to_copy = dmMath::Min(buffer_size, bytes_in_buffer);
            memcpy(buffer, internal_req->m_Server->m_Buffer + internal_req->m_ContentOffset, to_copy);
            internal_req->m_ContentOffset += to_copy;
            total += to_copy;
        }

        while (total < buffer_size)
        {
            void* p = (void*) (((uintptr_t) buffer) + total);
            int to_recv = buffer_size - total;
            int recv_bytes = 0;
            dmSocket::Result r = dmSocket::Receive(internal_req->m_Socket, p, to_recv, &recv_bytes);
            if (r == dmSocket::RESULT_TRY_AGAIN)
            {
                // Ok
            }
            else if (r == dmSocket::RESULT_OK)
            {
                total += recv_bytes;
            }
            else
            {
                internal_req->m_Result = RESULT_SOCKET_ERROR;
                break;
            }
        }
        internal_req->m_TotalContentReceived += total;
        *received_bytes = total;

        return internal_req->m_Result;
    }

    /*
     * Handle an http-connection
     * Returns false if the connection should be closed
     */
    static bool HandleConnection(Server* server, Connection* connection)
    {
        int total_recv = 0;

        InternalRequest internal_req;
        internal_req.m_Result = RESULT_OK;
        internal_req.m_Socket = connection->m_Socket;
        internal_req.m_Server = server;

        dmHttpServerPrivate::ParseResult parse_result;
        bool first_recv = true;
        do
        {
            // -1 in order to be able to null-terminate the string
            int max_to_recv = (BUFFER_SIZE-1) - total_recv;
            if (max_to_recv == 0)
                break;

            int recv_bytes;
            dmSocket::Result r = dmSocket::Receive(connection->m_Socket, server->m_Buffer + total_recv, max_to_recv, &recv_bytes);
            if (r == dmSocket::RESULT_OK)
            {
                if (recv_bytes == 0)
                {
                    // Interpret zero bytes, in first read, as connection closed by client.
                    if(!first_recv)
                        dmLogWarning("Client socket in http server was unexpectedly closed");

                    return false;
                }

                total_recv += recv_bytes;
                internal_req.m_TotalReceived = (uint32_t) total_recv;
                // null-terminate the string.
                server->m_Buffer[dmMath::Min(total_recv, (int) BUFFER_SIZE-1)] = '\0';
                parse_result = dmHttpServerPrivate::ParseHeader(server->m_Buffer, &internal_req, &HandleRequest, &HandleHeader, &HandleReponse);
            }
            else
            {
                return false;
            }

            first_recv = false;
        }
        while (parse_result == dmHttpServerPrivate::PARSE_RESULT_NEED_MORE_DATA);

        switch (parse_result)
        {
            case dmHttpServerPrivate::PARSE_RESULT_NEED_MORE_DATA:
                dmLogError("Buffer size in http-server too small");
                return false;

            case dmHttpServerPrivate::PARSE_RESULT_OK:
                // Ok
                break;

            case dmHttpServerPrivate::PARSE_RESULT_SYNTAX_ERROR:
                dmLogWarning("Invalid http request");
                return false;

            default:
                assert(0);
                break;
        }

        if (internal_req.m_Result == RESULT_OK)
        {
            return (bool) !internal_req.m_CloseConnection;
        }
        else
        {
            return false;
        }
    }

    Result Update(HServer server)
    {
        if (server->m_Reconnect)
        {
            dmLogWarning("Reconnecting http server (%d)", server->m_Port);
            Connect(server, server->m_Port);
            server->m_Reconnect = 0;
        }
        dmSocket::Selector selector;
        dmSocket::SelectorZero(&selector);
        dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, server->m_ServerSocket);

        dmSocket::Result r = dmSocket::Select(&selector, 0);

        if (r != dmSocket::RESULT_OK)
        {
            return RESULT_SOCKET_ERROR;
        }

        // Check for new connections
        if (dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, server->m_ServerSocket))
        {
            dmSocket::Address address;
            dmSocket::Socket client_socket;
            r = dmSocket::Accept(server->m_ServerSocket, &address, &client_socket);
            if (r == dmSocket::RESULT_OK)
            {
                if (server->m_Connections.Full())
                {
                    dmLogWarning("Out of client connections in http server (max: %d)", server->m_Connections.Capacity());
                    dmSocket::Shutdown(client_socket, dmSocket::SHUTDOWNTYPE_READWRITE);
                    dmSocket::Delete(client_socket);
                }
                else
                {
                    dmSocket::SetNoDelay(client_socket, true);
                    Connection connection;
                    memset(&connection, 0, sizeof(connection));
                    connection.m_Socket = client_socket;
                    connection.m_ConnectionTimeStart = dmTime::GetTime();
                    server->m_Connections.Push(connection);
                }
            }
            else if (r == dmSocket::RESULT_CONNABORTED || r == dmSocket::RESULT_NOTCONN)
            {
                server->m_Reconnect = 1;
            }
        }

        dmSocket::SelectorZero(&selector);

        uint64_t current_time = dmTime::GetTime();

        // Iterate over persistent connections, timeout phase
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            uint64_t time_diff = current_time - connection->m_ConnectionTimeStart;
            if (time_diff > server->m_ConnectionTimeout)
            {
                dmSocket::Shutdown(connection->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
                dmSocket::Delete(connection->m_Socket);
                server->m_Connections.EraseSwap(i);
                --i;
            }
        }

        // Iterate over persistent connections, select phase
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, connection->m_Socket);
        }

        r = dmSocket::Select(&selector, 0);
        if (r != dmSocket::RESULT_OK)
            return RESULT_SOCKET_ERROR;

        // Iterate over persistent connections, handle phase
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            if (dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, connection->m_Socket))
            {
                bool keep_connection =  HandleConnection(server, connection);
                if (!keep_connection)
                {
                    dmSocket::Shutdown(connection->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
                    dmSocket::Delete(connection->m_Socket);
                    server->m_Connections.EraseSwap(i);
                    --i;
                }
            }
        }
        return RESULT_OK;
    }

    void GetName(HServer server, dmSocket::Address* address, uint16_t* port)
    {
        *address = server->m_Address;
        *port = server->m_Port;
    }

}

