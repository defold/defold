// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_CONNECTION_POOL
#define DM_CONNECTION_POOL

#include <stdint.h>

// Cannot forward declare enums
#include <dmsdk/dlib/connection_pool.h>

/**
 * Connection pooling
 */
namespace dmConnectionPool
{
    // These are more for testing purposes, and are thus not added to the dmSDK

    /**
     * Stats
     */
    struct Stats
    {
        uint32_t m_Free;
        uint32_t m_Connected;
        uint32_t m_InUse;
        uint32_t m_InUseAndValid; // In use and has a valid socket
    };

    /**
     * Get statistics
     * @param pool
     * @param stats
     */
    void GetStats(HPool pool, Stats* stats);

    /**
     * Set max keep-alive for sockets in seconds. Sockets older than max_keep_alive
     * are not reused and hence closed
     * @name dmConnectionPool::SetMaxKeepAlive
     * @param pool
     * @param max_keep_alive
     */
    void SetMaxKeepAlive(HPool pool, uint32_t max_keep_alive);

    /**
     * Reopen the pool from a Shutdown call so it allows Dialing again. This function is here so the pool can be reset
     * during testing, or subsequent tests will break when the pool has been put in shutdown mode.
     */
    void Reopen(HPool pool);

    /**
     * Create an SSL Socket for the specified connection. The socket perform an
     * SSL handshake with the specified host.
     * @param pool [type:dmConnectionPool::HPool] pool
     * @param connection [type:dmConnectionPool::HConnection] connection
     * @param host [type:const char*] host
     * @param timeout [type:int] The timeout (micro seconds) for the connection and ssl handshake
     * @param sock_res [type:int] Pointer to write a dmSocket::Result to
     * @return dmConnectionPool::RESULT_OK on success
     */
    Result CreateSSLSocket(HPool pool, HConnection connection, const char* host, int timeout, dmSocket::Result* sock_res);
}

#endif // #ifndef DM_CONNECTION_POOL
