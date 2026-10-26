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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http_server.h"
#include "http_server_private.h"

#include <dlib/array.h>
#include <dlib/atomic.h>
#include <dlib/condition_variable.h>
#include <dlib/dstrings.h>
#include <dlib/jobsystem.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/network_constants.h>
#include <dlib/socket.h>
#include <dlib/time.h>


namespace dmHttpServer
{
    const uint32_t BUFFER_SIZE = 64 * 1024;

    struct AsyncResponse
    {
        dmSocket::Socket m_Socket;
        char*            m_Buffer;
        uint32_t         m_Capacity;
        uint32_t         m_Size;
        uint32_t         m_Offset;
        uint64_t         m_Deadline;
        uint64_t         m_Timeout;
        int32_atomic_t   m_Done;
        bool             m_Succeeded;
        bool             m_CloseConnection;
        bool             m_Draining;
        StreamRead       m_Read;
        StreamClose      m_Close;
        void*            m_Context;
        uint64_t         m_Remaining;
    };

    struct ResponseSender
    {
        HJobContext                             m_Jobs;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Wakeup;
        dmArray<AsyncResponse*>                 m_Pending;
        dmArray<AsyncResponse*>                 m_Active;
        bool                                    m_Stop;
    };

    struct Connection
    {
        dmSocket::Socket m_Socket;
        uint16_t         m_RequestCount;
        uint64_t         m_Deadline;
        char*            m_RequestBuffer;
        uint32_t         m_RequestBufferCapacity;
        uint32_t         m_RequestBufferSize;
        AsyncResponse*   m_Response;
    };

    struct Server
    {
        Server()
        {
            m_ServerSocket = dmSocket::INVALID_SOCKET_HANDLE;
            m_Reconnect = 0;
            m_ResponseSender = 0;
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

        // The dedicated job multiplexes responses without occupying shared workers.
        ResponseSender*     m_ResponseSender;

        uint32_t            m_Reconnect : 1;
    };

    struct InternalRequest
    {
        Request          m_Request;
        Result           m_Result;
        dmSocket::Socket m_Socket;
        Server*          m_Server;
        Connection*      m_Connection;

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
        uint16_t m_DeferResponse : 1;

        InternalRequest()
        {
            memset(this, 0, sizeof(*this));
            m_StatusCode = 200;
        }
    };

    static void FlushSendBuffer(const Request* request);
    static Result SendResponse(AsyncResponse* response, bool* progress);
    static bool UpdateResponses(ResponseSender* sender, int32_t timeout);
    static int32_t ResponseJob(HJobContext context, HJob job, void* user_context, void* user_data);

    static void CloseStream(AsyncResponse* response)
    {
        if (response->m_Close)
        {
            response->m_Close(response->m_Context);
            response->m_Close = 0;
        }
    }

    static void DeleteResponse(AsyncResponse* response)
    {
        if (response)
        {
            CloseStream(response);
            free(response->m_Buffer);
            delete response;
        }
    }

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
        dmSocket::Socket socket = dmSocket::INVALID_SOCKET_HANDLE;
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

    static void CloseConnection(Connection* connection)
    {
        dmSocket::Shutdown(connection->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        dmSocket::Delete(connection->m_Socket);
        free(connection->m_RequestBuffer);
        DeleteResponse(connection->m_Response);
    }

    void Delete(HServer server)
    {
        ResponseSender* sender = server->m_ResponseSender;
        if (sender)
        {
            {
                DM_MUTEX_SCOPED_LOCK(sender->m_Mutex);
                sender->m_Stop = true;
                dmConditionVariable::Signal(sender->m_Wakeup);
            }
            // No socket operation can wait for the client. Finish the current
            // bounded poll/read before releasing any response or stream context.
            JobSystemDestroy(sender->m_Jobs);
            dmConditionVariable::Delete(sender->m_Wakeup);
            dmMutex::Delete(sender->m_Mutex);
            delete sender;
        }
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            // Allow a small shutdown acknowledgement, without reading a stream
            // or waiting for the client on the update thread.
            if (connection->m_Response && !connection->m_Response->m_Close && !connection->m_Response->m_Draining &&
                !dmAtomicGet32(&connection->m_Response->m_Done))
            {
                bool progress = false;
                SendResponse(connection->m_Response, &progress);
            }
            CloseConnection(connection);
        }
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

    static void SendResponseData(InternalRequest* internal_req, const void* buffer, uint32_t length)
    {
        if (internal_req->m_Result != RESULT_OK)
        {
            return;
        }

        AsyncResponse* response = internal_req->m_Connection->m_Response;
        if (!response)
        {
            response = new AsyncResponse();
            response->m_Socket = internal_req->m_Socket;
            internal_req->m_Connection->m_Response = response;
        }
        if (length > UINT32_MAX - response->m_Size)
        {
            internal_req->m_Result = RESULT_INTERNAL_ERROR;
            return;
        }
        uint32_t size = response->m_Size + length;
        if (size > response->m_Capacity)
        {
            uint64_t capacity = dmMath::Max((uint64_t) size, (uint64_t) response->m_Capacity * 2);
            capacity = dmMath::Min(dmMath::Max(capacity, (uint64_t) 1024), (uint64_t) UINT32_MAX);
            char* data = (char*) realloc(response->m_Buffer, (size_t) capacity);
            if (!data)
            {
                internal_req->m_Result = RESULT_INTERNAL_ERROR;
                return;
            }
            response->m_Buffer = data;
            response->m_Capacity = (uint32_t) capacity;
        }
        memcpy(response->m_Buffer + response->m_Size, buffer, length);
        response->m_Size = size;
    }

    static void SendResponseData(InternalRequest* internal_req, const char* text)
    {
        SendResponseData(internal_req, text, (uint32_t) strlen(text));
    }

    static const char* StatusCodeString(int status_code)
    {
        switch (status_code)
        {
            case 202:
                return "Accepted";
            case 204:
                return "No Content";
            case 206:
                return "Partial Content";
            case 400:
                return "Bad Request";
            case 409:
                return "Conflict";
            case 410:
                return "Gone";
            case 413:
                return "Content Too Large";
            case 415:
                return "Unsupported Media Type";
            case 416:
                return "Range Not Satisfiable";
            case 501:
                return "Not Implemented";
            case 507:
                return "Insufficient Storage";
            case 200:
                return "OK";
            case 404:
                return "Not Found";
            case 405:
                return "Method Not Allowed";
            case 414:
                return "URI Too Long";
            case 500:
                return "Internal Server Error";
            case 302:
                return "Found";
            default:
                dmLogWarning("Unsupported status code: %d", status_code);
                return "";
        }
    }

    static void SendHeader(InternalRequest* internal_req)
    {
        internal_req->m_HeaderSent = 1;

        char header[128];
        dmSnPrintf(header, sizeof(header), "HTTP/1.1 %d %s\r\n",
                    internal_req->m_StatusCode,
                    StatusCodeString(internal_req->m_StatusCode));

        SendResponseData(internal_req, header);
    }

    static void SendAttributes(InternalRequest* internal_req, bool chunked = true)
    {
        internal_req->m_AttributesSent = 1;

        SendResponseData(internal_req, "Server: Dynamo 1.0\r\n");
        if (internal_req->m_CloseConnection)
            SendResponseData(internal_req, "Connection: close\r\n");
        if (internal_req->m_StatusCode != 204 && chunked)
        {
            SendResponseData(internal_req, "Transfer-Encoding: chunked\r\n");
        }
        SendResponseData(internal_req, "\r\n");
    }

    static void HandleReponse(void* user_data, int offset)
    {
        InternalRequest* internal_req = (InternalRequest*) user_data;
        Request* request = &internal_req->m_Request;

        Server* server = internal_req->m_Server;
        internal_req->m_ContentOffset = offset;

        request->m_Method = internal_req->m_Method;
        request->m_Resource = internal_req->m_Resource;
        request->m_Internal = internal_req;
        server->m_HttpResponse(server->m_Userdata, request);

        if (internal_req->m_DeferResponse)
        {
            return;
        }

        if (internal_req->m_Result == RESULT_OK && !internal_req->m_CloseConnection &&
            internal_req->m_TotalContentReceived != internal_req->m_Request.m_ContentLength)
        {
            dmLogWarning("Actual content differs from expected content-length (%d != %d)",
                    internal_req->m_TotalContentReceived,
                    internal_req->m_Request.m_ContentLength);
            internal_req->m_Result = RESULT_SOCKET_ERROR;
            return;
        }

        // Send headers and attributes even if no data is sent
        if (!internal_req->m_HeaderSent)
            SendHeader(internal_req);

        if (!internal_req->m_AttributesSent)
            SendAttributes(internal_req);

        // A 204 response ends at the headers, without a body or terminating chunk.
        if (internal_req->m_StatusCode == 204)
        {
            return;
        }

        FlushSendBuffer(request);

        if (internal_req->m_Result == RESULT_OK && !internal_req->m_Connection->m_Response->m_Read)
        {
            SendResponseData(internal_req, "0\r\n\r\n");
        }
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

    Result CloseConnection(const Request* request)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_HeaderSent)
        {
            return RESULT_ERROR_INVAL;
        }
        internal_req->m_CloseConnection = 1;
        return RESULT_OK;
    }

    static void FlushSendBuffer(const Request* request)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_SendBufferPos > 0)
        {
            uint32_t data_length = internal_req->m_SendBufferPos;
            internal_req->m_SendBufferPos = 0;

            char buf[16];
            dmSnPrintf(buf, sizeof(buf), "%x\r\n", data_length);
            SendResponseData(internal_req, buf);
            SendResponseData(internal_req, internal_req->m_Server->m_Buffer, data_length);
            SendResponseData(internal_req, "\r\n");
        }
    }

    Result Send(const Request* request, const void* data, uint32_t data_length)
    {
        // Do not send empty chunks as the empty chunk is
        // interpreted as the last and terminating chunk
        if (data_length == 0) {
            return RESULT_OK;
        }

        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_Connection->m_Response && internal_req->m_Connection->m_Response->m_Read)
        {
            return RESULT_ERROR_INVAL;
        }
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

    Result SendStream(const Request* request, uint64_t length, StreamRead read, StreamClose close, void* context)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (!read || !close || internal_req->m_AttributesSent || internal_req->m_StatusCode == 204)
        {
            return RESULT_ERROR_INVAL;
        }
        if (internal_req->m_Result != RESULT_OK)
        {
            return internal_req->m_Result;
        }
        char content_length[32];
        dmSnPrintf(content_length, sizeof(content_length), "%llu", (unsigned long long) length);
        SendAttribute(request, "Content-Length", content_length);
        SendAttributes(internal_req, false);
        if (internal_req->m_Result != RESULT_OK)
        {
            return internal_req->m_Result;
        }
        AsyncResponse* response = internal_req->m_Connection->m_Response;
        response->m_Read = read;
        response->m_Close = close;
        response->m_Context = context;
        response->m_Remaining = length;
        return RESULT_OK;
    }

    Result SendAttribute(const Request* request, const char* key, const char* value)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;

        if (internal_req->m_AttributesSent)
        {
            dmLogError("SendAttribute is only valid before any data is sent: (%s : %s)", key, value);
            return RESULT_ERROR_INVAL;
        }

        if (!internal_req->m_HeaderSent)
            SendHeader(internal_req);

        SendResponseData(internal_req, key);
        SendResponseData(internal_req, ":");
        SendResponseData(internal_req, value);
        SendResponseData(internal_req, "\r\n");

        return internal_req->m_Result;
    }

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
            else if (r == dmSocket::RESULT_OK && recv_bytes > 0)
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

    Result ReceiveBuffered(const Request* request, void* buffer, uint32_t buffer_size)
    {
        InternalRequest* internal_req = (InternalRequest*) request->m_Internal;
        if (internal_req->m_HeaderSent || internal_req->m_TotalContentReceived ||
            request->m_ContentLength > buffer_size ||
            request->m_ContentLength > BUFFER_SIZE - 1 - internal_req->m_ContentOffset)
        {
            return RESULT_ERROR_INVAL;
        }

        if (internal_req->m_TotalReceived - internal_req->m_ContentOffset < request->m_ContentLength)
        {
            internal_req->m_DeferResponse = 1;
            return RESULT_TRY_AGAIN;
        }

        uint32_t received_bytes;
        return Receive(request, buffer, request->m_ContentLength, &received_bytes);
    }

    /*
     * Handle an http-connection
     * Returns false if the connection should be closed
     */
    static bool HandleConnection(Server* server, Connection* connection)
    {
        if (connection->m_RequestBufferCapacity == 0 ||
            connection->m_RequestBufferSize == connection->m_RequestBufferCapacity - 1)
        {
            uint32_t capacity = connection->m_RequestBufferCapacity ?
                dmMath::Min(2 * connection->m_RequestBufferCapacity, BUFFER_SIZE) : 1024;
            if (capacity == connection->m_RequestBufferCapacity)
            {
                return false;
            }
            char* buffer = (char*) realloc(connection->m_RequestBuffer, capacity);
            if (!buffer)
            {
                return false;
            }
            connection->m_RequestBuffer = buffer;
            connection->m_RequestBufferCapacity = capacity;
        }

        // Update selects readable sockets. Read once, then yield if the headers
        // or a buffered body are incomplete, so other connections and frames run.
        int received_bytes;
        dmSocket::Result r = dmSocket::Receive(connection->m_Socket,
                                               connection->m_RequestBuffer + connection->m_RequestBufferSize,
                                               connection->m_RequestBufferCapacity - 1 - connection->m_RequestBufferSize,
                                               &received_bytes);
        if (r == dmSocket::RESULT_TRY_AGAIN)
        {
            return true;
        }
        if (r != dmSocket::RESULT_OK || received_bytes == 0)
        {
            return false;
        }
        connection->m_RequestBufferSize += received_bytes;

        // ParseHeader modifies its input and Send reuses the server buffer.
        // Retain the original request per connection until its handler completes.
        memcpy(server->m_Buffer, connection->m_RequestBuffer, connection->m_RequestBufferSize);
        server->m_Buffer[connection->m_RequestBufferSize] = '\0';

        InternalRequest internal_req;
        internal_req.m_Result = RESULT_OK;
        internal_req.m_Socket = connection->m_Socket;
        internal_req.m_Server = server;
        internal_req.m_Connection = connection;
        internal_req.m_TotalReceived = connection->m_RequestBufferSize;
        dmHttpServerPrivate::ParseResult parse_result = dmHttpServerPrivate::ParseHeader(server->m_Buffer,
                                                                                         &internal_req,
                                                                                         &HandleRequest,
                                                                                         &HandleHeader,
                                                                                         &HandleReponse);

        switch (parse_result)
        {
            case dmHttpServerPrivate::PARSE_RESULT_NEED_MORE_DATA:
                if (connection->m_RequestBufferSize == BUFFER_SIZE - 1)
                {
                    dmLogError("Buffer size in http-server too small");
                    return false;
                }
                return true;

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

        if (internal_req.m_DeferResponse)
        {
            return true;
        }

        connection->m_RequestBufferSize = 0;
        if (internal_req.m_Result != RESULT_OK ||
            dmSocket::SetBlocking(connection->m_Socket, false) != dmSocket::RESULT_OK)
        {
            return false;
        }

        connection->m_Response->m_CloseConnection = internal_req.m_CloseConnection;
        connection->m_Response->m_Timeout = server->m_ConnectionTimeout;
        connection->m_Response->m_Deadline = dmTime::GetMonotonicTime() + server->m_ConnectionTimeout;
        if (!server->m_ResponseSender)
        {
            ResponseSender* sender = new ResponseSender();
            sender->m_Mutex = dmMutex::New();
            sender->m_Wakeup = dmConditionVariable::New();
            sender->m_Pending.SetCapacity(server->m_Connections.Capacity());
            sender->m_Active.SetCapacity(server->m_Connections.Capacity());
            JobSystemCreateParams params = {};
            params.m_ThreadNamePrefix = "http_send";
            params.m_ThreadCount = 1;
            sender->m_Jobs = JobSystemCreate(&params);
            server->m_ResponseSender = sender;
            if (JobSystemGetWorkerCount(sender->m_Jobs))
            {
                Job job = {};
                job.m_Process = ResponseJob;
                job.m_Data = sender;
                JobSystemPushJob(sender->m_Jobs, JobSystemCreateJob(sender->m_Jobs, &job));
            }
        }
        ResponseSender* sender = server->m_ResponseSender;
        DM_MUTEX_SCOPED_LOCK(sender->m_Mutex);
        sender->m_Pending.Push(connection->m_Response);
        dmConditionVariable::Signal(sender->m_Wakeup);
        return true;
    }

    static void CompleteResponse(AsyncResponse* response, bool succeeded)
    {
        CloseStream(response);
        if (!succeeded || response->m_CloseConnection)
        {
            dmSocket::Shutdown(response->m_Socket, dmSocket::SHUTDOWNTYPE_READWRITE);
        }
        free(response->m_Buffer);
        response->m_Buffer = 0;
        response->m_Succeeded = succeeded;
        // Publish last, with a full barrier: Update may immediately destroy this
        // response and socket once it observes completion.
        dmAtomicCompareStore32(&response->m_Done, 1, 0);
    }

    static Result SendResponse(AsyncResponse* response, bool* progress)
    {
        if (response->m_Offset == response->m_Size && response->m_Remaining)
        {
            // Each stream retains at most one buffer regardless of its length.
            if (response->m_Capacity < BUFFER_SIZE)
            {
                char* buffer = (char*) realloc(response->m_Buffer, BUFFER_SIZE);
                if (!buffer)
                {
                    return RESULT_INTERNAL_ERROR;
                }
                response->m_Buffer = buffer;
                response->m_Capacity = BUFFER_SIZE;
            }
            uint32_t count = (uint32_t) dmMath::Min(response->m_Remaining, (uint64_t) BUFFER_SIZE);
            uint32_t read = response->m_Read(response->m_Context, response->m_Buffer, count);
            if (!read || read > count)
            {
                return RESULT_INTERNAL_ERROR;
            }
            response->m_Offset = 0;
            response->m_Size = read;
            response->m_Remaining -= read;
        }
        int sent_bytes;
        dmSocket::Result r = dmSocket::Send(response->m_Socket,
                                            response->m_Buffer + response->m_Offset,
                                            dmMath::Min(response->m_Size - response->m_Offset, BUFFER_SIZE),
                                            &sent_bytes);
        if (r == dmSocket::RESULT_TRY_AGAIN || r == dmSocket::RESULT_WOULDBLOCK)
        {
            return RESULT_TRY_AGAIN;
        }
        if (r != dmSocket::RESULT_OK || sent_bytes == 0)
        {
            return RESULT_SOCKET_ERROR;
        }
        *progress = true;
        response->m_Offset += sent_bytes;
        response->m_Deadline = dmTime::GetMonotonicTime() + response->m_Timeout;
        if (response->m_Offset != response->m_Size || response->m_Remaining)
        {
            return RESULT_TRY_AGAIN;
        }
        if (!response->m_CloseConnection)
        {
            return RESULT_OK;
        }

        // A read-side shutdown with unread upload data resets TCP on Windows,
        // potentially discarding this response. Send FIN first, then discard
        // incoming bytes asynchronously until EOF or the fixed close deadline.
        if (dmSocket::Shutdown(response->m_Socket, dmSocket::SHUTDOWNTYPE_WRITE) != dmSocket::RESULT_OK)
        {
            return RESULT_SOCKET_ERROR;
        }
        CloseStream(response);
        free(response->m_Buffer);
        response->m_Buffer = 0;
        response->m_Draining = true;
        return RESULT_TRY_AGAIN;
    }

    static Result DrainRequest(AsyncResponse* response, bool* progress)
    {
        char buffer[4096];
        int received_bytes;
        dmSocket::Result result = dmSocket::Receive(response->m_Socket, buffer, sizeof(buffer), &received_bytes);
        if (result == dmSocket::RESULT_TRY_AGAIN || result == dmSocket::RESULT_WOULDBLOCK)
        {
            return RESULT_TRY_AGAIN;
        }
        if (result != dmSocket::RESULT_OK)
        {
            return RESULT_SOCKET_ERROR;
        }
        *progress = true;
        // Do not extend the deadline: a rejected upload must not keep its slot
        // indefinitely by continuing to send data after the response.
        return received_bytes == 0 ? RESULT_OK : RESULT_TRY_AGAIN;
    }

    static bool UpdateResponses(ResponseSender* sender, int32_t timeout)
    {
        {
            DM_MUTEX_SCOPED_LOCK(sender->m_Mutex);
            while (timeout && sender->m_Active.Empty() && sender->m_Pending.Empty() && !sender->m_Stop)
            {
                dmConditionVariable::Wait(sender->m_Wakeup, sender->m_Mutex);
            }
            if (sender->m_Stop)
            {
                return false;
            }
            for (uint32_t i = 0; i < sender->m_Pending.Size(); ++i)
            {
                sender->m_Active.Push(sender->m_Pending[i]);
            }
            sender->m_Pending.SetSize(0);
        }
        dmSocket::Selector selector;
        for (uint32_t i = 0; i < sender->m_Active.Size(); ++i)
        {
            AsyncResponse* response = sender->m_Active[i];
            dmSocket::SelectorSet(&selector, response->m_Draining ? dmSocket::SELECTOR_KIND_READ : dmSocket::SELECTOR_KIND_WRITE,
                                  response->m_Socket);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_EXCEPT, response->m_Socket);
        }
        dmSocket::Result selected = dmSocket::Select(&selector, timeout);
        if (selected == dmSocket::RESULT_TRY_AGAIN)
        {
            return true;
        }
        uint64_t now = dmTime::GetMonotonicTime();
        bool progress = false;
        for (uint32_t i = 0; i < sender->m_Active.Size();)
        {
            AsyncResponse* response = sender->m_Active[i];
            Result result = RESULT_TRY_AGAIN;
            if (now >= response->m_Deadline ||
                (selected != dmSocket::RESULT_OK && selected != dmSocket::RESULT_WOULDBLOCK))
            {
                result = RESULT_SOCKET_ERROR;
            }
            else if (dmSocket::SelectorIsSet(&selector, response->m_Draining ? dmSocket::SELECTOR_KIND_READ : dmSocket::SELECTOR_KIND_WRITE,
                                           response->m_Socket) ||
                     dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_EXCEPT, response->m_Socket))
            {
                // A hangup can mean the client only closed its sending side.
                // Let nonblocking I/O distinguish that from a broken socket.
                result = response->m_Draining ? DrainRequest(response, &progress) : SendResponse(response, &progress);
            }
            if (result == RESULT_TRY_AGAIN)
            {
                ++i;
            }
            else
            {
                // Remove the worker's reference before publishing completion.
                sender->m_Active.EraseSwap(i);
                CompleteResponse(response, result == RESULT_OK);
            }
        }
        // A half-close can keep poll reporting a hangup while the send buffer is
        // full. Back off only if no client progressed, and only on the worker.
        if (timeout && selected == dmSocket::RESULT_OK && !progress && !sender->m_Active.Empty())
        {
            dmTime::Sleep((uint32_t) timeout);
        }
        return true;
    }

    static int32_t ResponseJob(HJobContext context, HJob job, void* user_context, void* user_data)
    {
        // One dedicated job services all clients fairly, including when engine
        // updates pause. Never hold the publication mutex during I/O.
        while (UpdateResponses((ResponseSender*) user_data, 10000))
        {
        }
        return 0;
    }

    Result Update(HServer server)
    {
        if (server->m_ResponseSender && !JobSystemGetWorkerCount(server->m_ResponseSender->m_Jobs))
        {
            UpdateResponses(server->m_ResponseSender, 0);
        }

        // Reclaim completed responses before accepting new connections. The
        // worker owns pending responses; it enforces their deadline itself.
        uint64_t current_time = dmTime::GetMonotonicTime();
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            bool keep_connection = true;
            if (connection->m_Response)
            {
                if (!dmAtomicGet32(&connection->m_Response->m_Done))
                {
                    continue;
                }
                keep_connection = connection->m_Response->m_Succeeded && !connection->m_Response->m_CloseConnection &&
                    dmSocket::SetBlocking(connection->m_Socket, true) == dmSocket::RESULT_OK;
                connection->m_Deadline = connection->m_Response->m_Deadline;
                DeleteResponse(connection->m_Response);
                connection->m_Response = 0;
            }
            if (!keep_connection || current_time >= connection->m_Deadline)
            {
                CloseConnection(connection);
                server->m_Connections.EraseSwap(i);
                --i;
            }
        }
        if (server->m_Reconnect)
        {
            dmLogWarning("Reconnecting http server (%d)", server->m_Port);
            Connect(server, server->m_Port);
            server->m_Reconnect = 0;
        }
        dmSocket::Selector selector;
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
                    connection.m_Deadline = dmTime::GetMonotonicTime() + server->m_ConnectionTimeout;
                    server->m_Connections.Push(connection);
                }
            }
            else if (r == dmSocket::RESULT_CONNABORTED || r == dmSocket::RESULT_NOTCONN)
            {
                server->m_Reconnect = 1;
            }
        }

        dmSocket::SelectorZero(&selector);

        // Iterate over persistent connections, select phase
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            if (connection->m_Response)
            {
                continue;
            }
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_READ, connection->m_Socket);
            dmSocket::SelectorSet(&selector, dmSocket::SELECTOR_KIND_EXCEPT, connection->m_Socket);
        }

        r = dmSocket::Select(&selector, 0);
        if (r != dmSocket::RESULT_OK)
            return RESULT_SOCKET_ERROR;

        // Iterate over persistent connections, handle phase
        for (uint32_t i = 0; i < server->m_Connections.Size(); ++i)
        {
            Connection* connection = &server->m_Connections[i];
            if (connection->m_Response)
            {
                continue;
            }
            bool keep_connection = true;
            if (dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_READ, connection->m_Socket))
            {
                // Read buffered request data even when a write-side FIN also
                // reports a hangup. Receive handles EOF and actual socket errors.
                keep_connection = HandleConnection(server, connection);
            }
            else if (dmSocket::SelectorIsSet(&selector, dmSocket::SELECTOR_KIND_EXCEPT, connection->m_Socket))
            {
                // There is no buffered request data left to process.
                keep_connection = false;
            }
            if (!keep_connection)
            {
                CloseConnection(connection);
                server->m_Connections.EraseSwap(i);
                --i;
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
