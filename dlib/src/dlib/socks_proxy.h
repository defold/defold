#ifndef SOCKS_PROXY_H
#define SOCKS_PROXY_H

#include "socket.h"

namespace dmSocksProxy
{
    /**
     * SocksProxy result
     */
    enum Result
    {
        RESULT_OK = 0,                      //!< RESULT_OK
        RESULT_SOCKET_ERROR = -1,           //!< RESULT_SOCKET_ERROR
        RESULT_REQUEST_FAILED = -2,         //!< RESULT_REQUEST_FAILED. Corresponds to SOCKS result 0x5b
        RESULT_NOT_REACHABLE = -3,          //!< RESULT_NOT_REACHABLE. Corresponds to SOCKS result 0x5c
        RESULT_INVALID_USER_ID = -4,        //!< RESULT_INVALID_USER_ID. Corresponds to SOCKS result 0x5d
        RESULT_INVALID_SERVER_RESPONSE = -5,//!< RESULT_INVALID_SERVER_RESPONSE
        RESULT_NO_DMSOCKS_PROXY_SET = -6,   //!< RESULT_NO_DMSOCKS_PROXY_SET
    };

    /**
     * Connect using a SOCKS4 proxy. The environment variable DMSOCKS_PROXY must be set to proxy host name.
     * Port 1080 is the default proxy port. Can be set using the environment variable DMSOCKS_PROXY_PORT
     * @param address Address to connect to
     * @param port Port to connect to
     * @param socket Resulting socks through proxy
     * @param socket_result Socket result. Set if RESULT_SOCKET_ERROR is returned. Can be null.
     * @return RESULT_OK on success.
     */
    Result Connect(dmSocket::Address address, int port, dmSocket::Socket* socket, dmSocket::Result* socket_result);
}

#endif
