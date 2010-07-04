#ifndef DM_HTTP_CLIENT_H
#define DM_HTTP_CLIENT_H

#include <stdint.h>
#include <dlib/socket.h>

namespace dmHttpClient
{
    /**
     * HTTP-client handle
     */
    typedef struct Client* HClient;

    /**
     * HTTP-header call-back
     * @param client Client handle
     * @param user_data User data
     * @param status_code Status code, eg 200
     * @param key Header key, eg "Content-Length"
     * @param value Header value
     */
    typedef void (*HttpHeader)(HClient client, void* user_data, int status_code, const char* key, const char* value);

    /**
     * HTTP-content call-back.
     * @param client Client handle
     * @param user_data User data
     * @param status_code Status code, eg 200
     * @param content_data Content data
     * @param content_data_size Content data size
     */
    typedef void (*HttpContent)(HClient client, void* user_data, int status_code, const void* content_data, uint32_t content_data_size);

    /**
     * Result values
     */
    enum Result
    {
        RESULT_NOT_200_OK = 1,                    //!< RESULT_NOT_200_OK
        RESULT_OK = 0,                            //!< RESULT_OK
        RESULT_SOCKET_ERROR = -1,                 //!< RESULT_SOCKET_ERROR
        RESULT_HTTP_HEADERS_ERROR = -2,           //!< RESULT_HTTP_HEADERS_ERROR
        RESULT_MISSING_CONTENT_LENGTH = -3,       //!< RESULT_MISSING_CONTENT_LENGTH
        RESULT_PARTIAL_CONTENT = -4,              //!< RESULT_PARTIAL_CONTENT
        RESULT_UNSUPPORTED_TRANSFER_ENCODING = -5,//!< RESULT_UNSUPPORTED_TRANSFER_ENCODING
        RESULT_INVAL_ERROR = -6,                  //!< RESULT_INVAL_ERROR
    };

    /**
     * HTTP-client options
     */
    enum Option
    {
        /// Maximum number of retries for GET-request. Default is 4.
        OPTION_MAX_GET_RETRIES,
    };

    /**
     * Set NewParsms default values
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

        /// HTTP-header handle
        HttpHeader  m_HttpHeader;

        NewParams()
        {
            SetDefaultParams(this);
        }
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
     * Set HTTP client option
     * @param client Client handle
     * @param option Options enum
     * @param value Option value
     * @return RESULT_OK on success
     */
    Result SetOptionInt(HClient client, Option option, int value);

    /**
     * Get last socket error. Called to get further information when RESULT_SOCKET_ERROR is returned.
     * @param client Client handle
     * @return dmSocket::Result
     */
    dmSocket::Result GetLastSocketResult(HClient client);

    /**
     * HTTP GET-request
     * @param client Client handle
     * @param path Path part of URI
     * @return RESULT_OK on success
     */
    Result Get(HClient client, const char* path);

    /**
     * Delete HTTP client
     * @param client Client handle
     */
    void Delete(HClient client);
}

#endif // DM_HTTP_CLIENT_H
