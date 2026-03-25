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

#include <dlib/log.h>
#include <dlib/message.h>

#include "http_ddf.h"
#include "http_service.h"

namespace dmHttpService
{
    #define HTTP_SOCKET_NAME "@http"

    struct HttpService
    {
        dmMessage::HSocket m_Socket;
    };

    HHttpService New(const Params* params)
    {
        (void) params;

        HttpService* service = new HttpService;
        service->m_Socket = 0;

        dmMessage::Result result = dmMessage::NewSocket(HTTP_SOCKET_NAME, &service->m_Socket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogError("Unable to create HTTP service socket: %d", result);
            delete service;
            return 0;
        }

        return service;
    }

    dmMessage::HSocket GetSocket(HHttpService http_service)
    {
        return http_service ? http_service->m_Socket : 0;
    }

    void Delete(HHttpService http_service)
    {
        if (!http_service)
            return;

        if (http_service->m_Socket)
            dmMessage::DeleteSocket(http_service->m_Socket);
        delete http_service;
    }
}
