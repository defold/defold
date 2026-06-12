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

#ifndef DMSDK_DLIB_HTTP_H
#define DMSDK_DLIB_HTTP_H

#include <stdint.h>

/*# HTTP API documentation
 * HTTP service and request functions.
 *
 * @document
 * @name HTTP
 * @language C
 * @examples
 *
 * Create and push a GET request. The caller owns the request until
 * HttpPushRequest() succeeds. The callback is called from an HTTP worker
 * thread, and data pointers are valid only for the duration of the callback.
 *
 * ```c
 * typedef struct HttpExampleContext
 * {
 *     uint32_t   m_TotalBytes;
 *     HttpResult m_Result;
 *     int        m_Done;
 * } HttpExampleContext;
 *
 * static HttpCallbackResult HttpExampleCallback(HttpRequest* request, void* user_data, const HttpResponseInfo* response)
 * {
 *     HttpExampleContext* context = (HttpExampleContext*) user_data;
 *     (void) request;
 *
 *     switch (HttpResponseGetEvent(response))
 *     {
 *         case HTTP_RESPONSE_EVENT_HEADER:
 *             // HttpResponseGetHeader(response) points to "Name:Value".
 *             // Copy it here if it is needed after the callback returns.
 *             break;
 *
 *         case HTTP_RESPONSE_EVENT_DATA:
 *             // Process or copy HttpResponseGetData(response) before returning.
 *             context->m_TotalBytes += HttpResponseGetDataSize(response);
 *             break;
 *
 *         case HTTP_RESPONSE_EVENT_COMPLETE:
 *             context->m_Result = HttpResponseGetResult(response);
 *             context->m_Done = 1;
 *             break;
 *     }
 *
 *     return HTTP_CALLBACK_RESULT_CONTINUE;
 * }
 *
 * static HttpResult PushExampleGet(HttpService* service, HttpExampleContext* context, HttpRequestHandle* handle)
 * {
 *     HttpRequest* request = 0;
 *     HttpResult result = HttpNewRequest(&request);
 *     if (result != HTTP_RESULT_OK)
 *     {
 *         return result;
 *     }
 *
 *     result = HttpSetMethod(request, "GET");
 *     if (result == HTTP_RESULT_OK)
 *     {
 *         result = HttpSetURL(request, "https://example.com/data.json");
 *     }
 *     if (result == HTTP_RESULT_OK)
 *     {
 *         result = HttpAddHeader(request, "Accept: application/json");
 *     }
 *     if (result == HTTP_RESULT_OK)
 *     {
 *         result = HttpSetResponseCallback(request, HttpExampleCallback, context);
 *     }
 *     if (result == HTTP_RESULT_OK)
 *     {
 *         result = HttpPushRequest(service, request, handle);
 *     }
 *
 *     if (result != HTTP_RESULT_OK)
 *     {
 *         HttpDeleteRequest(request);
 *     }
 *
 *     return result;
 * }
 * ```
 */

/*# HTTP service extension context name
 * Name used when registering the HTTP service with dmExtension context params.
 * @constant
 * @name HTTP_SERVICE_CONTEXT_NAME
 */
#define HTTP_SERVICE_CONTEXT_NAME "http_service"

/*# HTTP service object
 * HTTP service context provided by the engine.
 * @typedef
 * @name HttpService
 */
typedef struct HttpService HttpService;

/*# HTTP request object
 * The caller owns the request until HttpPushRequest() succeeds.
 * @typedef
 * @name HttpRequest
 */
typedef struct HttpRequest HttpRequest;

/*# HTTP request handle
 * Opaque service-local value identifying a request after ownership has been
 * transferred to the HTTP service.
 * @typedef
 * @name HttpRequestHandle
 */
typedef uint32_t HttpRequestHandle;

/*# invalid HTTP request handle
 * @constant
 * @name HTTP_REQUEST_HANDLE_INVALID
 */
#define HTTP_REQUEST_HANDLE_INVALID ((HttpRequestHandle)0)

/*# HTTP result values
 * @enum
 * @name HttpResult
 * @member HTTP_RESULT_NOT_200_OK = 1
 * @member HTTP_RESULT_OK = 0
 * @member HTTP_RESULT_SOCKET_ERROR = -1
 * @member HTTP_RESULT_HTTP_HEADERS_ERROR = -2
 * @member HTTP_RESULT_INVALID_RESPONSE = -3
 * @member HTTP_RESULT_PARTIAL_CONTENT = -4
 * @member HTTP_RESULT_UNSUPPORTED_TRANSFER_ENCODING = -5
 * @member HTTP_RESULT_INVAL_ERROR = -6
 * @member HTTP_RESULT_UNEXPECTED_EOF = -7
 * @member HTTP_RESULT_IO_ERROR = -8
 * @member HTTP_RESULT_HANDSHAKE_FAILED = -9
 * @member HTTP_RESULT_INVAL = -10
 * @member HTTP_RESULT_UNKNOWN = -1000
 */
typedef enum HttpResult
{
    HTTP_RESULT_NOT_200_OK = 1,
    HTTP_RESULT_OK = 0,
    HTTP_RESULT_SOCKET_ERROR = -1,
    HTTP_RESULT_HTTP_HEADERS_ERROR = -2,
    HTTP_RESULT_INVALID_RESPONSE = -3,
    HTTP_RESULT_PARTIAL_CONTENT = -4,
    HTTP_RESULT_UNSUPPORTED_TRANSFER_ENCODING = -5,
    HTTP_RESULT_INVAL_ERROR = -6,
    HTTP_RESULT_UNEXPECTED_EOF = -7,
    HTTP_RESULT_IO_ERROR = -8,
    HTTP_RESULT_HANDSHAKE_FAILED = -9,
    HTTP_RESULT_INVAL = -10,
    HTTP_RESULT_UNKNOWN = -1000,
} HttpResult;

/*# HTTP response event
 * @enum
 * @name HttpResponseEvent
 * @member HTTP_RESPONSE_EVENT_HEADER Response header data is available.
 * @member HTTP_RESPONSE_EVENT_DATA Response body data is available.
 * @member HTTP_RESPONSE_EVENT_COMPLETE Request completed.
 * @member HTTP_RESPONSE_EVENT_PROGRESS Request progress data is available.
 */
typedef enum HttpResponseEvent
{
    HTTP_RESPONSE_EVENT_HEADER = 0,
    HTTP_RESPONSE_EVENT_DATA = 1,
    HTTP_RESPONSE_EVENT_COMPLETE = 2,
    HTTP_RESPONSE_EVENT_PROGRESS = 3,
} HttpResponseEvent;

/*# HTTP callback result
 * Return value from HttpResponseCallback.
 * @enum
 * @name HttpCallbackResult
 * @member HTTP_CALLBACK_RESULT_CONTINUE Continue processing the request.
 * @member HTTP_CALLBACK_RESULT_CANCEL Cancel the request. Ignored for HTTP_RESPONSE_EVENT_COMPLETE.
 */
typedef enum HttpCallbackResult
{
    HTTP_CALLBACK_RESULT_CONTINUE = 0,
    HTTP_CALLBACK_RESULT_CANCEL = 1,
} HttpCallbackResult;

/*# HTTP response information
 * Opaque data passed to HttpResponseCallback.
 * @typedef
 * @name HttpResponseInfo
 */
typedef struct HttpResponseInfo HttpResponseInfo;

/*# HTTP response callback
 * Called from the HTTP client worker thread. HTTP_RESPONSE_EVENT_DATA events
 * are sent as response chunks arrive; the receiver is responsible for
 * processing or copying them before returning. The receiver is also responsible
 * for synchronizing with other threads. Pointers in HttpResponseInfo are valid
 * only for the duration of the callback. Returning HTTP_CALLBACK_RESULT_CANCEL
 * from header, data, or progress events cancels the request.
 * @typedef
 * @name HttpResponseCallback
 * @param request [type:HttpRequest*] Request object. Valid only for the duration of the callback.
 * @param user_data [type:void*] User data.
 * @param response [type:const HttpResponseInfo*] Response data.
 * @return result [type:HttpCallbackResult] callback result.
 */
typedef HttpCallbackResult (*HttpResponseCallback)(HttpRequest* request, void* user_data, const HttpResponseInfo* response);

#ifdef __cplusplus
extern "C"
{
#endif

    /*# get response event
     * @name HttpResponseGetEvent
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return event [type:HttpResponseEvent] Event type.
     */
    HttpResponseEvent HttpResponseGetEvent(const HttpResponseInfo* response);

    /*# get response result
     * Valid for HTTP_RESPONSE_EVENT_COMPLETE.
     * @name HttpResponseGetResult
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return result [type:HttpResult] Transfer result.
     */
    HttpResult HttpResponseGetResult(const HttpResponseInfo* response);

    /*# get response status code
     * @name HttpResponseGetStatusCode
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return status_code [type:int] HTTP status code, eg 200.
     */
    int HttpResponseGetStatusCode(const HttpResponseInfo* response);

    /*# get response header
     * Valid for HTTP_RESPONSE_EVENT_HEADER. Returns the raw response header line
     * as "Name:Value", without a trailing newline. The returned pointer is valid
     * only for the duration of the callback. Use HttpResponseGetHeaderSize() for
     * the number of bytes in the header line.
     * @name HttpResponseGetHeader
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return header [type:const char*] Raw header line in "Name:Value" form.
     */
    const char* HttpResponseGetHeader(const HttpResponseInfo* response);

    /*# get response header size
     * Valid for HTTP_RESPONSE_EVENT_HEADER.
     * @name HttpResponseGetHeaderSize
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return size [type:uint32_t] Number of bytes returned by
     * HttpResponseGetHeader(), excluding any null terminator.
     */
    uint32_t HttpResponseGetHeaderSize(const HttpResponseInfo* response);

    /*# get response data
     * Valid for HTTP_RESPONSE_EVENT_DATA.
     * @name HttpResponseGetData
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return data [type:const void*] Response body chunk data.
     */
    const void* HttpResponseGetData(const HttpResponseInfo* response);

    /*# get response data size
     * Valid for HTTP_RESPONSE_EVENT_DATA.
     * @name HttpResponseGetDataSize
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return size [type:uint32_t] Response body chunk size.
     */
    uint32_t HttpResponseGetDataSize(const HttpResponseInfo* response);

    /*# get response URL
     * @name HttpResponseGetURL
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return url [type:const char*] Request URL.
     */
    const char* HttpResponseGetURL(const HttpResponseInfo* response);

    /*# get response path
     * @name HttpResponseGetPath
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return path [type:const char*] User supplied response path.
     */
    const char* HttpResponseGetPath(const HttpResponseInfo* response);

    /*# get response range start
     * @name HttpResponseGetRangeStart
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return range_start [type:uint32_t] Start offset into the requested file, when known.
     */
    uint32_t HttpResponseGetRangeStart(const HttpResponseInfo* response);

    /*# get response range end
     * @name HttpResponseGetRangeEnd
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return range_end [type:uint32_t] End offset into the requested file, when known.
     */
    uint32_t HttpResponseGetRangeEnd(const HttpResponseInfo* response);

    /*# get response document size
     * @name HttpResponseGetDocumentSize
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return document_size [type:uint32_t] Full size of the requested file, when known.
     */
    uint32_t HttpResponseGetDocumentSize(const HttpResponseInfo* response);

    /*# get progress bytes sent
     * Valid for HTTP_RESPONSE_EVENT_PROGRESS.
     * @name HttpResponseGetBytesSent
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return bytes_sent [type:uint32_t] Sent byte count.
     */
    uint32_t HttpResponseGetBytesSent(const HttpResponseInfo* response);

    /*# get progress bytes received
     * Valid for HTTP_RESPONSE_EVENT_PROGRESS.
     * @name HttpResponseGetBytesReceived
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return bytes_received [type:uint32_t] Received byte count.
     */
    uint32_t HttpResponseGetBytesReceived(const HttpResponseInfo* response);

    /*# get progress byte total
     * Valid for HTTP_RESPONSE_EVENT_PROGRESS.
     * @name HttpResponseGetBytesTotal
     * @param response [type:const HttpResponseInfo*] Response data.
     * @return bytes_total [type:int32_t] Total byte count, or -1 if unknown.
     */
    int32_t HttpResponseGetBytesTotal(const HttpResponseInfo* response);

    /*# create a new HTTP request
     * @name HttpNewRequest
     * @param request [type:HttpRequest**] Request object on success.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpNewRequest(HttpRequest** request);

    /*# delete an HTTP request
     * Only valid before HttpPushRequest() succeeds.
     * @name HttpDeleteRequest
     * @param request [type:HttpRequest*] Request object.
     */
    void HttpDeleteRequest(HttpRequest* request);

    /*# set request URL
     * @name HttpSetURL
     * @param request [type:HttpRequest*] Request object.
     * @param url [type:const char*] Full request URL.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetURL(HttpRequest* request, const char* url);

    /*# set request method
     * @name HttpSetMethod
     * @param request [type:HttpRequest*] Request object.
     * @param method [type:const char*] HTTP method, eg "GET".
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetMethod(HttpRequest* request, const char* method);

    /*# add a request header
     * @name HttpAddHeader
     * @param request [type:HttpRequest*] Request object.
     * @param header [type:const char*] Raw request header, eg "Accept: application/json".
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpAddHeader(HttpRequest* request, const char* header);

    /*# set request body
     * Copies request body data into the request. Passing a zero size clears the body.
     * The current HTTP client sends request bodies for POST, PUT, and PATCH.
     * @name HttpSetRequestBody
     * @param request [type:HttpRequest*] Request object.
     * @param body [type:const void*] Request body data.
     * @param body_size [type:uint32_t] Request body size.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetRequestBody(HttpRequest* request, const void* body, uint32_t body_size);

    /*# set response path
     * Stores a user supplied path with the request. The HTTP API does not write the
     * response to this path; it is returned in response callbacks for callers that
     * want to write response data themselves.
     * @name HttpSetResponsePath
     * @param request [type:HttpRequest*] Request object.
     * @param path [type:const char*] Response path, or 0 to clear.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetResponsePath(HttpRequest* request, const char* path);

    /*# set proxy URL
     * @name HttpSetProxy
     * @param request [type:HttpRequest*] Request object.
     * @param proxy [type:const char*] Full proxy URL, or 0 to clear.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetProxy(HttpRequest* request, const char* proxy);

    /*# set ignore cache
     * @name HttpSetIgnoreCache
     * @param request [type:HttpRequest*] Request object.
     * @param ignore_cache [type:int] Non-zero to ignore the HTTP cache.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetIgnoreCache(HttpRequest* request, int ignore_cache);

    /*# set chunked transfer
     * @name HttpSetChunkedTransfer
     * @param request [type:HttpRequest*] Request object.
     * @param chunked_transfer [type:int] Non-zero to allow chunked transfer encoding.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetChunkedTransfer(HttpRequest* request, int chunked_transfer);

    /*# set progress reporting
     * @name HttpSetReportProgress
     * @param request [type:HttpRequest*] Request object.
     * @param report_progress [type:int] Non-zero to receive HTTP_RESPONSE_EVENT_PROGRESS callbacks.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetReportProgress(HttpRequest* request, int report_progress);

    /*# set basic authentication credentials
     * @name HttpSetBasicAuth
     * @param request [type:HttpRequest*] Request object.
     * @param username [type:const char*] User name.
     * @param password [type:const char*] Password.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetBasicAuth(HttpRequest* request, const char* username, const char* password);

    /*# set bearer authentication token
     * @name HttpSetBearerAuth
     * @param request [type:HttpRequest*] Request object.
     * @param token [type:const char*] Bearer token.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetBearerAuth(HttpRequest* request, const char* token);

    /*# set request timeout
     * @name HttpSetTimeout
     * @param request [type:HttpRequest*] Request object.
     * @param timeout_us [type:uint32_t] Timeout in microseconds.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetTimeout(HttpRequest* request, uint32_t timeout_us);

    /*# set response callback
     * @name HttpSetResponseCallback
     * @param request [type:HttpRequest*] Request object.
     * @param callback [type:HttpResponseCallback] Response callback.
     * @param user_data [type:void*] User data.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpSetResponseCallback(HttpRequest* request, HttpResponseCallback callback, void* user_data);

    /*# push a request to an HTTP service
     * On success, ownership of request is transferred to the service and the caller
     * must not modify or delete it.
     * @name HttpPushRequest
     * @param service [type:HttpService*] HTTP service object.
     * @param request [type:HttpRequest*] Request object.
     * @param request_handle [type:HttpRequestHandle*] Request handle on success.
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpPushRequest(HttpService* service, HttpRequest* request, HttpRequestHandle* request_handle);

    /*# request cancellation
     * Marks a queued or running request as canceled. Cancellation is best effort.
     * @name HttpCancelRequest
     * @param service [type:HttpService*] HTTP service object.
     * @param request_handle [type:HttpRequestHandle] Request handle returned by HttpPushRequest().
     * @return result [type:HttpResult] HTTP_RESULT_OK on success.
     */
    HttpResult HttpCancelRequest(HttpService* service, HttpRequestHandle request_handle);

#ifdef __cplusplus
}
#endif

#endif // DMSDK_DLIB_HTTP_H
