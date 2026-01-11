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

#ifndef DM_WEBSERVER_H
#define DM_WEBSERVER_H

#include <dmsdk/dlib/socket.h>
#include <dmsdk/dlib/webserver.h>

namespace dmWebServer
{
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
}

#endif // DM_WEBSERVER_H
