#ifndef DM_WEB_SERVER_H
#define DM_WEB_SERVER_H

#include <dlib/socket.h>

namespace dmWebServer
{
    /**
     * @file
     * Simple high-level single-threaded Web server based on dmHttpServer
     * The web-server has a handler concept similar to servlets in Java
     */

    /**
     * Web-server handle
     */
    typedef struct Server* HServer;

    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK = 0,               //!< RESULT_OK
        RESULT_SOCKET_ERROR = -1,    //!< RESULT_SOCKET_ERROR
        RESULT_INVALID_REQUEST = -2, //!< RESULT_INVALID_REQUEST
        RESULT_ERROR_INVAL = -3,     //!< RESULT_ERROR_INVAL
        RESULT_HANDLER_ALREADY_REGISTRED = -4, //!< RESULT_HANDLER_ALREADY_REGISTRED
        RESULT_HANDLER_NOT_REGISTRED = -5, //!< RESULT_HANDLER_NOT_REGISTRED
        RESULT_INTERNAL_ERROR = -100,//!< RESULT_INTERNAL_ERROR
        RESULT_UNKNOWN = -1000,      //!< RESULT_UNKNOWN
    };

    /**
     * Web-server request
     */
    struct Request
    {
        /// Request method
        const char* m_Method;
        /// Request resource
        const char* m_Resource;
        /// Content-Length header
        uint32_t    m_ContentLength;
        /// Internal data
        void*       m_Internal;
    };

    /**
     * Set NewParams default values
     * @param params Pointer to NewParams
     */
    void SetDefaultParams(struct NewParams* params);

    /**
     * Parameters passed into #New when creating a new web-server instance
     */
    struct NewParams
    {
        /// Web-server port
        uint16_t    m_Port;

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
     * Web request handler callback
     * @param user_data User  data
     * @param request Request
     */
    typedef void (*Handler)(void* user_data, Request* request);

    /**
     * Crete a new web server
     * @param params Parameters
     * @param server Web server instance
     * @return RESULT_OK on success
     */
    Result New(const NewParams* params, HServer* server);

    /**
     * Delete web server
     * @param server Web server instance
     */
    void Delete(HServer server);

    /**
     * Handler parameters
     */
    struct HandlerParams
    {
        /// User data
        void*       m_Userdata;
        /// Handler callback
        Handler     m_Handler;
    };

    /**
     * Add a new handler
     * @param server Server handle
     * @param prefix Location prefix for which locations this handler should handle
     * @param handler_params Handler parameters
     * @return RESULT_OK on success
     */
    Result AddHandler(HServer server,
                      const char* prefix,
                      const HandlerParams* handler_params);

    /**
     * Remove handle
     * @param server Server handle
     * @param prefix Prefix for handle to remove
     * @return RESULT_OK on success
     */
    Result RemoveHandler(HServer server, const char* prefix);

    /**
     * Set response status code.
     * @note Only valid to invoke before #Send is invoked
     * @param request Request
     * @param status_code Status code to set
     * @return RESULT_OK on success
     */
    Result SetStatusCode(Request* request, int status_code);

    /**
     * Get http header value for key
     * @param request Request
     * @param name Header key
     * @return Header value. NULL if the key doesn't exists
     */
    const char* GetHeader(Request* request, const char* name);

    /**
     * Send response data
     * @param request Request handle
     * @param data Data to send
     * @param data_length Data-lenght to send
     * @return RESULT_OK on success
     */
    Result Send(Request* request, const void* data, uint32_t data_length);

    /**
     * Receive data
     * @param request Request
     * @param buffer Data buffer to receive to
     * @param buffer_size Buffer size
     * @param received_bytes Number of bytes received
     * @return RESULT_OK on success
     */
    Result Receive(Request* request, void* buffer, uint32_t buffer_size, uint32_t* received_bytes);

    /**
     * Update server
     * @param server Server handle
     * @return RESULT_OK on success
     */
    Result Update(HServer server);

    /**
     * Get name for socket, ie address and port
     * @param server Web server
     * @param address Address (result)
     * @param port Port (result)
     */
    void GetName(HServer server, dmSocket::Address* address, uint16_t* port);

    /**
     * Sends a header attribute
     * @param request Request
     * @param key the header name
     * @param value the header value
     */
    Result SendAttribute(Request* request, const char* key, const char* value);

}

#endif // DM_WEB_SERVER_H
