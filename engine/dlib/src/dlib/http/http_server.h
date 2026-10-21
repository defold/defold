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

#ifndef DM_HTTP_SERVER_H
#define DM_HTTP_SERVER_H

#include <dlib/socket.h>

namespace dmHttpServer
{
    /**
     * @file
     * HTTP server with update-thread handlers and asynchronous response sending.
     * Http methods sending data, eg put and post, are not supported.
     */

    /**
     * Http-server handle
     */
    typedef struct Server* HServer;

    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK = 0,                //!< RESULT_OK
        RESULT_SOCKET_ERROR = -1,     //!< RESULT_SOCKET_ERROR
        RESULT_INVALID_REQUEST = -2,  //!< RESULT_INVALID_REQUEST
        RESULT_ERROR_INVAL = -3,      //!< RESULT_ERROR_INVAL
        RESULT_TRY_AGAIN = -4,        //!< Request body is incomplete; return from the response callback and retry on a later update.
        RESULT_INTERNAL_ERROR = -100, //!< RESULT_INTERNAL_ERROR
        RESULT_UNKNOWN = -1000,       //!< RESULT_UNKNOWN
    };

    /**
     * Set NewParams default values
     * @param params Pointer to NewParams
     */
    void SetDefaultParams(struct NewParams* params);

    /**
     * Http request. Contains relevant information about the request. Passed into #HttpResponse callback
     */
    struct Request
    {
        /// Request method
        const char* m_Method;
        /// Request resource
        const char* m_Resource;
        /// HTTP major version
        int         m_Major;
        /// HTTP minor version
        int         m_Minor;
        /// Content-Length header
        uint32_t    m_ContentLength;
        /// Internal data
        void*       m_Internal;
    };

    /**
     * Http header callback function. Called with http header information
     * @param user_data User data
     * @param key Header attribute key, eg "Connection"
     * @param value Header attribute value
     */
    typedef void (*HttpHeader)(void* user_data, const char* key, const char* value);

    /**
     * Http response callback. Called when response should be sent back to the client
     * @param user_data User data
     * @param request Request information
     */
    typedef void (*HttpResponse)(void* user_data, const Request* request);

    /**
     * Parameters passed into #New when creating a new server instance
     */
    struct NewParams
    {
        /// User-data. Passed in to callbacks
        void*       m_Userdata;

        /// HTTP-header handle
        HttpHeader  m_HttpHeader;

        /// HTTP-response callback
        HttpResponse m_HttpResponse;

        /// Max persistent client connections
        uint16_t    m_MaxConnections;

        /// Connection timeout in seconds
        uint16_t    m_ConnectionTimeout;

        NewParams()
        {
            SetDefaultParams(this);
        }
    };

    /**
     * Create a new http server instance
     * @param params Parameters
     * @param port Port to run the server on
     * @param server Http server instance
     * @return RESULT_OK on success
     */
    Result New(const NewParams* params, uint16_t port, HServer* server);

    /**
     * Set response status code, eg 200 and 404
     * @note Only valid to invoke before #Send is invoked
     * @param request Request
     * @param status_code Status code to set
     * @return RESULT_OK on success
     */
    Result SetStatusCode(const Request* request, int status_code);

    /**
     * Close the connection after sending the response, without draining unread request data.
     * Must be called before sending any response headers or data.
     */
    Result CloseConnection(const Request* request);

    /**
     * Send response data
     * Copies data into an owned response sent asynchronously after the handler returns.
     * Socket errors are handled by the server; RESULT_OK means the data was queued.
     * @param request Request
     * @param data Data to send
     * @param data_length Data-length to send
     * @return RESULT_OK on success
     */
    Result Send(const Request* request, const void* data, uint32_t data_length);

    typedef uint32_t (*StreamRead)(void* context, void* buffer, uint32_t size);
    typedef void (*StreamClose)(void* context);

    // Stream exactly length bytes with Content-Length after the handler returns, using a bounded buffer.
    // Call after setting headers, instead of Send. Read runs on the response worker
    // (or Update on platforms without threads); returning zero early aborts the response.
    // On success the server owns context and calls close exactly once, including
    // disconnect/shutdown. On failure ownership stays with the caller.
    Result SendStream(const Request* request, uint64_t length, StreamRead read, StreamClose close, void* context);

    /**
     * Send attribute
     * @note Only valid to invoke before #Send is invoked
     * @param request Request
     * @param key Attribute key
     * @param value Attribute value
     * @return RESULT_ON on success
     */
    Result SendAttribute(const Request* request, const char* key, const char* value);

    /**
     * Receive request data
     * @note The function will block until all data is read or if any socket error occur. Handling
     * of partial content is not required.
     * @param request Request
     * @param data Data buffer to receive to
     * @param buffer_size Buffer size
     * @param received_bytes Number of bytes received [out]
     * @return RESULT_OK on success
     */
    Result Receive(const Request* request, void* buffer, uint32_t buffer_size, uint32_t* received_bytes);

    /**
     * Receive a complete, bounded request body without waiting for socket data.
     * Call before consuming request data or sending response headers. The buffer
     * must hold Content-Length bytes, and headers plus body must fit in 64 KiB.
     * On RESULT_TRY_AGAIN, return immediately from the response callback without
     * side effects. It will be called again with the same headers and body when
     * more data arrives. Incomplete requests expire at the connection timeout.
     */
    Result ReceiveBuffered(const Request* request, void* buffer, uint32_t buffer_size);

    /**
     * Delete http server instance
     * @param server Http server instance handle
     */
    void Delete(HServer server);

    /**
     * Update http server, eg serve and accept connections
     * @param server Http server instance
     * @return RESULT_OK on success
     */
    Result Update(HServer server);

    /**
     * Get name for socket, ie address and port
     * @param server Http server instance
     * @param address Address (result)
     * @param port Port (result)
     */
    void GetName(HServer server, dmSocket::Address* address, uint16_t* port);
}

#endif
