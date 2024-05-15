// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_WEBSERVER_H
#define DMSDK_WEBSERVER_H

#include <dmsdk/dlib/socket.h>


/*# Web server functions
 *
 * Simple high-level single-threaded Web server based on dmHttpServer
 * The web-server has a handler concept similar to servlets in Java
 *
 * @document
 * @name WebServer
 * @namespace dmWebServer
 * @path engine/dlib/src/dmsdk/dlib/webserver.h
 */

namespace dmWebServer
{
    /*# web server handle
     * @typedef
     * @name HServer
     */
    typedef struct Server* HServer;

    /*# result codes
     * @enum
     * @name Result
     * @member RESULT_OK
     * @member RESULT_SOCKET_ERROR
     * @member RESULT_INVALID_REQUEST
     * @member RESULT_ERROR_INVAL
     * @member RESULT_HANDLER_ALREADY_REGISTRED
     * @member RESULT_HANDLER_NOT_REGISTRED
     * @member RESULT_INTERNAL_ERROR
     * @member RESULT_UNKNOWN
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_SOCKET_ERROR = -1,
        RESULT_INVALID_REQUEST = -2,
        RESULT_ERROR_INVAL = -3,
        RESULT_HANDLER_ALREADY_REGISTRED = -4,
        RESULT_HANDLER_NOT_REGISTRED = -5,
        RESULT_INTERNAL_ERROR = -100,
        RESULT_UNKNOWN = -1000,
    };

    /*# web server request
     * @struct
     * @name Request
     * @member m_Method [type:const char*] Request method
     * @member m_Method [type:const char*] Request resource
     * @member m_Method [type:const char*] Content-Length header
     * @member m_Method [type:const char*] Internal data
     */
    struct Request
    {
        const char* m_Method;
        const char* m_Resource;
        uint32_t    m_ContentLength;
        void*       m_Internal;
    };

    /*#
     * Web request handler callback
     * @typedef
     * @name Handler
     * @param user_data [type:void*] User  data
     * @param request [type:Request*] Request
     * @return [type:void]
     */
    typedef void (*Handler)(void* user_data, Request* request);

    /*# handler parameters
     * @struct
     * @name HandlerParams
     * @member m_UserData [type:void*] The user data
     * @member m_Handler [type:Handler] The callback
     */
    struct HandlerParams
    {
        void*       m_Userdata;
        Handler     m_Handler;
    };

    /*# Add a new handler
     * @name AddHandler
     * @param server [type:HServer] Server handle
     * @param prefix [type:const char*] Location prefix for which locations this handler should handle
     * @param handler_params [type:HandlerParams] Handler parameters
     * @return [type:Result] RESULT_OK on success
     */
    Result AddHandler(HServer server,
                      const char* prefix,
                      const HandlerParams* handler_params);

    /*# Remove handle
     * @name RemoveHandler
     * @param server [type:HServer] Server handle
     * @param prefix [type:const char*] Prefix for handle to remove
     * @return [type:Result] RESULT_OK on success
     */
    Result RemoveHandler(HServer server, const char* prefix);

    /*# Set response status code.
     * @note Only valid to invoke before #Send is invoked
     * @name SetStatusCode
     * @param request [type:Request*] Request
     * @param status_code [type:int] Status code to set
     * @return [type:Result] RESULT_OK on success
     */
    Result SetStatusCode(Request* request, int status_code);

    /*# Get http header value for key
     * @name GetHeader
     * @param request [type:Request*] Request
     * @param name [type:const char*] Header key
     * @return [type:const char*] Header value. NULL if the key doesn't exists
     */
    const char* GetHeader(Request* request, const char* name);

    /*# Send response data
     * @name Send
     * @param request [type:Request] Request handle
     * @param data [type:void*] Data to send
     * @param data_length [type:uint32_t] Data-lenght to send
     * @return [type:Result] RESULT_OK on success
     */
    Result Send(Request* request, const void* data, uint32_t data_length);

    /*# Receive data
     * @name Receive
     * @param request [type:Request*] Request
     * @param buffer [type:void*] Data buffer to receive to
     * @param buffer_size [type:uint32_t] Buffer size
     * @param received_bytes [type:uint32_t*] Number of bytes received
     * @return [type:Result] RESULT_OK on success
     */
    Result Receive(Request* request, void* buffer, uint32_t buffer_size, uint32_t* received_bytes);

    /*# Sends a header attribute
     * @name SendAttribute
     * @param request [type:Request*] Request
     * @param key [type:const char*] the header name
     * @param value [type:const char*] the header value
     * @return [type:Result] RESULT_OK on success
     */
    Result SendAttribute(Request* request, const char* key, const char* value);

}

#endif // DMSDK_WEBSERVER_H

