#ifndef DM_HTTP_CLIENT_H
#define DM_HTTP_CLIENT_H

#include "http_cache.h"
#include "socket.h"

namespace dmHttpClient
{
    /**
     * Result values
     */
    enum Result
    {
        RESULT_NOT_200_OK = 1,                    //!< RESULT_NOT_200_OK
        RESULT_OK = 0,                            //!< RESULT_OK
        RESULT_SOCKET_ERROR = -1,                 //!< RESULT_SOCKET_ERROR
        RESULT_HTTP_HEADERS_ERROR = -2,           //!< RESULT_HTTP_HEADERS_ERROR
        RESULT_INVALID_RESPONSE = -3,             //!< RESULT_INVALID_RESPONSE
        RESULT_PARTIAL_CONTENT = -4,              //!< RESULT_PARTIAL_CONTENT
        RESULT_UNSUPPORTED_TRANSFER_ENCODING = -5,//!< RESULT_UNSUPPORTED_TRANSFER_ENCODING
        RESULT_INVAL_ERROR = -6,                  //!< RESULT_INVAL_ERROR
        RESULT_UNEXPECTED_EOF = -7,               //!< RESULT_UNEXPECTED_EOF
        RESULT_IO_ERROR = -8,                     //!< RESULT_IO_ERROR
        RESULT_HANDSHAKE_FAILED = -9,             //!< RESULT_HANDSHAKE_FAILED
        RESULT_INVAL = -10,                       //!< RESULT_INVAL
        RESULT_UNKNOWN = -1000,                   //!< RESULT_UNKNOWN
    };

    /**
     * HTTP-client handle
     */
    typedef struct Client* HClient;

    typedef struct Response* HResponse;

    /**
     * HTTP-header call-back
     * @param response Response handle
     * @param user_data User data
     * @param status_code Status code, eg 200
     * @param key Header key, eg "Content-Length"
     * @param value Header value
     */
    typedef void (*HttpHeader)(HResponse response, void* user_data, int status_code, const char* key, const char* value);

    /**
     * HTTP-content call-back. Called with both content_data and content_data_size set to 0 indicates the start of a response.
     * During a request retry this might occur more than once when the request starts over.
     * @param response Response handle
     * @param user_data User data
     * @param status_code Status code, eg 200
     * @param content_data Content data
     * @param content_data_size Content data size
     */
    typedef void (*HttpContent)(HResponse response, void* user_data, int status_code, const void* content_data, uint32_t content_data_size);

    /**
     * HTTP content-length callback. Invoked for POST-request prior to HttpWrite-callback to determine content-length
     * @param response Response handle
     * @param user_data User data
     * @return Content length
     */
    typedef uint32_t (*HttpSendContentLength)(HResponse response, void* user_data);

    /**
     * HTTP-post callback. Invoked for POST method. The function invokes the Write function to POST data.
     * @param response Response handle
     * @param user_data User data
     * @return The callback should return the value returned from the Write function
     */
    typedef Result (*HttpWrite)(HResponse response, void* user_data);

    /**
     * HTTP write request header callback. The function invokes the WriteHeader function to send request header
     * @param response Response handle
     * @param user_data User data
     * @return The callback should return the value returned from the WriteHeader function
     */
    typedef Result (*HttpWriteHeaders)(HResponse response, void* user_data);

    /**
     * HTTP-client options
     */
    enum Option
    {
        /// Maximum number of retries for GET-request. Default is 4.
        OPTION_MAX_GET_RETRIES,
        /// Request timeout in us
        OPTION_REQUEST_TIMEOUT,
    };

    /**
     * Set NewParams default values
     * @param params Pointer to NewParams
     */
    void SetDefaultParams(struct NewParams* params);

    /**
     * New HTTP-client parameters.
     * The structure is automatically initialized to default values
     */
    struct NewParams
    {
        /// User-data. Passed in to callbacks
        void*       m_Userdata;

        //// HTTP-content callback
        HttpContent m_HttpContent;

        /// HTTP-header callback
        HttpHeader  m_HttpHeader;

        /// HTTP send content-length callback
        HttpSendContentLength m_HttpSendContentLength;

        /// HTTP-write callback
        HttpWrite    m_HttpWrite;

        /// HTTP-write headers callback
        HttpWriteHeaders m_HttpWriteHeaders;

        /// HTTP-cache. Default value 0. Set to a http-cache to enable http-caching
        dmHttpCache::HCache m_HttpCache;

        NewParams()
        {
            SetDefaultParams(this);
        }
    };

    /**
     * HTTP-client statistics
     */
    struct Statistics
    {
        /// Number of responses. This includes all responses, even for retries for idempotent requests.
        // TODO: Rename to m_Requests? Perhaps more appropriate?
        uint32_t m_Responses;
        /// Number of cached responses. This includes all cached responses, even for retries for idempotent requests.
        uint32_t m_CachedResponses;
        /// Number of direct cached verified, ie data taken directly from cache without validation request
        uint32_t m_DirectFromCache;
        /// Number of reconnections as a result of connection lost
        uint32_t m_Reconnections;
    };

    /**
     * Create a new HTTP client
     * @param params Parameters
     * @param hostname Hostname
     * @param port Port number
     * @return HTTP-client handle on success. 0 on failure.
     */
    HClient New(const NewParams* params, const char* hostname, uint16_t port);

    /**
     * Create a new HTTP client
     * @param params Parameters
     * @param hostname Hostname
     * @param port Port number
     * @param secure TLS/SSL or not
     * @return HTTP-client handle on success. 0 on failure.
     */
    HClient New(const NewParams* params, const char* hostname, uint16_t port, bool secure);

    /**
     * Set HTTP client option
     * @param client Client handle
     * @param option Options enum
     * @param value Option value
     * @return RESULT_OK on success
     */
    Result SetOptionInt(HClient client, Option option, int64_t value);

    /**
     * Get last socket error. Called to get further information when RESULT_SOCKET_ERROR is returned.
     * @param client Client handle
     * @return dmSocket::Result
     */
    dmSocket::Result GetLastSocketResult(HClient client);

    /**
     * HTTP GET-request with automatic retry
     * @param client Client handle
     * @param path Path part of URI
     * @return RESULT_OK on success
     */
    Result Get(HClient client, const char* path);

    /**
     * HTTP POST-request
     * @param client Client handle
     * @param path Path part of URI
     * @return RESULT_OK on success
     */
    Result Post(HClient client, const char* path);

    /**
     * Generic HTTP request with automatic retry for GET-requests only
     * @param client Client handle
     * @param method HTTP method
     * @param path Path part of URI
     * @return RESULT_OK on success
     */
    Result Request(HClient client, const char* method, const char* path);

    /**
     * Write data. Called from HttpWrite-callback to write POST-data
     * @param response Response handle
     * @param buffer Buffer
     * @param buffer_size Buffer size
     * @return RESULT_OK on success
     */
    Result Write(HResponse response, const void* buffer, uint32_t buffer_size);

    /**
     * Write request header. Called from HttpWriteHeadwers-callback to write request headers
     * @param response Response handle
     * @param name Header name
     * @param value Header value
     * @return RESULT_OK on success
     */
    Result WriteHeader(HResponse response, const char* name, const char* value);

    /**
     * Get HTTP-client statistics
     * @param client Client handle
     * @param statistics Pointer to statistics struct
     */
    void GetStatistics(HClient client, Statistics* statistics);

    /**
     * Get HTTP-cache associated with this client
     * @param client client
     * @return dmHttpCache::HCache handle
     */
    dmHttpCache::HCache GetHttpCache(HClient client);

    /**
     * Delete HTTP client
     * @param client Client handle
     */
    void Delete(HClient client);

    /**
     * Close all sockets open by the internal pool and return how many connections
     * are still in use. If the function returns zero, there are no remaining in-use
     * connections and no new connections can be created.
     *
     * @return number of in-use connections
     */
    uint32_t ShutdownConnectionPool();

    /**
     * Reopen the internal connection pool. Implemented so testing the shutdown sequence is possible without
     * permanently breaking for subsequent requests.
    */
    void ReopenConnectionPool();
}

#endif // DM_HTTP_CLIENT_H
