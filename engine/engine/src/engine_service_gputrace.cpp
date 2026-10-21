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

#include "engine_service_darwin.h"

#include <string.h>
#include <dlib/dstrings.h>

namespace dmEngineService
{
    void GPUTraceSendError(dmWebServer::Request* request, int status, const char* code, const char* message)
    {
        // Callers supply fixed error codes and messages. Native errors are serialized with capture status.
        char response[512];
        int  length = dmSnPrintf(response, sizeof(response), "{\"ok\":false,\"error\":{\"status\":%d,\"code\":\"%s\",\"message\":\"%s\"}}", status, code, message);
        dmWebServer::SetStatusCode(request, status);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        if (status == 405)
            dmWebServer::SendAttribute(request, "Allow", "GET, POST, DELETE");
        dmWebServer::Send(request, response, length);
    }

    void GPUTraceRequest(void* context, dmWebServer::Request* request)
    {
        // Bound allocations and consume the entire body, including on rejected requests.
        const uint32_t max_body_size = 64 * 1024;
        dmArray<char>  body;
        if (request->m_ContentLength <= max_body_size)
            body.SetCapacity(request->m_ContentLength);
        char     discard[256];
        uint32_t offset = 0;
        while (offset < request->m_ContentLength)
        {
            uint32_t remaining = request->m_ContentLength - offset;
            uint32_t received = 0;
            uint32_t count = remaining;
            char*    destination = discard;
            if (request->m_ContentLength <= max_body_size)
                destination = body.Begin() + offset;
            else
            {
                count = remaining < sizeof(discard) ? remaining : sizeof(discard);
            }
            if (dmWebServer::Receive(request, destination, count, &received) != dmWebServer::RESULT_OK || received == 0)
            {
                GPUTraceSendError(request, 400, "bad_request", "could not read request body");
                return;
            }
            offset += received;
        }

        const char* suffix = request->m_Resource + strlen("/profiler/gputrace");
        if (*suffix != 0 && *suffix != '?')
        {
            GPUTraceSendError(request, 404, "not_found", "unknown GPU trace endpoint");
            return;
        }
        if (!dmStrEq(request->m_Method, "GET") && !dmStrEq(request->m_Method, "POST") && !dmStrEq(request->m_Method, "DELETE"))
        {
            GPUTraceSendError(request, 405, "method_not_allowed", "use GET, POST or DELETE");
            return;
        }
        if (request->m_ContentLength > max_body_size)
        {
            GPUTraceSendError(request, 413, "request_too_large", "request body exceeds 65536 bytes");
            return;
        }
        GPUTracePlatformRequest((GPUTrace*)context, request, body.Begin(), request->m_ContentLength);
    }
} // namespace dmEngineService
