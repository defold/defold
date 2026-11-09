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

#include "platform_service.h"
#include "platform_service_openapi.h"
#include <dlib/dstrings.h>
#include <dlib/json.h>
#include <dlib/time.h>
#include "window.hpp"
#include <dlib/webserver.h>
#include <string.h>

namespace dmPlatformService
{
    static const uint32_t MAX_RESIZES = 64;
    static const uint64_t RETENTION_US = 60000000;

    enum ResizeState
    {
        RESIZE_QUEUED, RESIZE_AWAITING_WINDOW, RESIZE_COMPLETE, RESIZE_FAILED
    };

    struct Resize
    {
        uint64_t      m_Id;
        uint64_t      m_Deadline;
        uint64_t      m_Expires;
        uint32_t      m_Width;
        uint32_t      m_Height;
        uint32_t      m_Timeout;
        char          m_ExpectedRevision[32];
        char          m_Revision[32];
        WindowMetrics m_Observed;
        ResizeState   m_State;
        const char*   m_Error;
        const char*   m_Message;
        bool          m_AlreadyMatches;
        bool          m_Supersede;
    };

    struct Service
    {
        dmWebServer::HServer   m_Server;
        bool                   m_AttachmentChanged;
        FGetTime               m_GetTime;
        WindowMetrics          m_Metrics;
        HWindow                m_Window;
        Resize                 m_Resizes[MAX_RESIZES];
        uint64_t               m_Sequence;
        uint64_t               m_Revision;
        int32_t                m_Active;
    };

    static void Revision(Service* service, char* out, uint32_t size)
    {
        dmSnPrintf(out, size, "w%llu", (unsigned long long)service->m_Revision);
    }

    static const char* UnavailableReason(Service* service)
    {
        if (!service->m_Metrics.m_WindowAvailable)
        {
            return "client_metrics_unavailable";
        }
        if (!service->m_Metrics.m_DrawableAvailable)
        {
            return "drawable_unavailable";
        }
        if (!service->m_Metrics.m_ResizeAvailable)
        {
            return "window_mode_unsupported";
        }
        return 0;
    }

    static void Dimensions(char* out, uint32_t size, bool available, uint32_t width, uint32_t height)
    {
        if (available)
        {
            dmSnPrintf(out, size, "{\"width\":%u,\"height\":%u}", width, height);
        }
        else
        {
            dmStrlCpy(out, "null", size);
        }
    }

    static void SendJson(dmWebServer::Request* request, int status, const char* body, int length, uint32_t capacity, const char* header = 0, const char* value = 0)
    {
        bool valid = length >= 0 && (uint32_t)length < capacity;
        dmWebServer::SetStatusCode(request, valid ? status : 500);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        if (!valid)
        {
            const char* error = "{\"error\":{\"code\":\"serialization_failed\",\"message\":\"Unable to serialize window metrics\"}}";
            dmWebServer::Send(request, error, strlen(error));
            return;
        }
        if (header)
        {
            dmWebServer::SendAttribute(request, header, value);
        }
        dmWebServer::Send(request, body, length);
    }

    static void Error(dmWebServer::Request* request, int status, const char* code, const char* message, const char* allow = 0)
    {
        // Errors contain only service-owned ASCII constants, never client data.
        char body[384];
        int length = dmSnPrintf(body, sizeof(body), "{\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}", code, message);
        SendJson(request, status, body, length, sizeof(body), allow ? "Allow" : 0, allow);
    }

    static void Reject(dmWebServer::Request* request, int status, const char* code, const char* message, const char* allow = 0)
    {
        dmWebServer::CloseConnection(request);
        Error(request, status, code, message, allow);
    }

    static void Finish(Service* service, Resize* resize, uint64_t now, const char* code, const char* message)
    {
        resize->m_State = code ? RESIZE_FAILED : RESIZE_COMPLETE;
        resize->m_Error = code;
        resize->m_Message = message;
        resize->m_Observed = service->m_Metrics;
        Revision(service, resize->m_Revision, sizeof(resize->m_Revision));
        resize->m_Expires = now + RETENTION_US;
        service->m_Active = -1;
    }

    static bool Matches(Service* service, Resize* resize)
    {
        return service->m_Metrics.m_WindowAvailable &&
               service->m_Metrics.m_WindowWidth == resize->m_Width && service->m_Metrics.m_WindowHeight == resize->m_Height;
    }

    static WindowMetrics ReadWindowMetrics(Service* service)
    {
        WindowMetrics state = {};
        HWindow window = service->m_Window;
        if (window && dmPlatform::GetWindowStateParam(window, WINDOW_STATE_OPENED))
        {
            state.m_WindowAvailable = dmPlatform::GetWindowClientSize(window, &state.m_WindowWidth, &state.m_WindowHeight);
            state.m_DrawableWidth = dmPlatform::GetWindowWidth(window);
            state.m_DrawableHeight = dmPlatform::GetWindowHeight(window);
            state.m_DrawableAvailable = state.m_DrawableWidth && state.m_DrawableHeight;
            state.m_DisplayScale = dmPlatform::GetDisplayScaleFactor(window);
            state.m_ResizeAvailable = dmPlatform::IsWindowResizeSupported(window);
        }
        return state;
    }

    static void RefreshWindowMetrics(Service* service)
    {
        WindowMetrics state = ReadWindowMetrics(service);
        const WindowMetrics* old = &service->m_Metrics;
        if (!service->m_Revision || service->m_AttachmentChanged ||
            old->m_WindowAvailable != state.m_WindowAvailable || old->m_DrawableAvailable != state.m_DrawableAvailable ||
            old->m_WindowWidth != state.m_WindowWidth || old->m_WindowHeight != state.m_WindowHeight ||
            old->m_DrawableWidth != state.m_DrawableWidth || old->m_DrawableHeight != state.m_DrawableHeight)
        {
            ++service->m_Revision;
        }
        service->m_Metrics = state;
        service->m_AttachmentChanged = false;
    }

    void Attach(HService service, HWindow window)
    {
        service->m_Window = window;
        service->m_AttachmentChanged = true;
    }

    void Observe(HService service, WindowObservation* observation)
    {
        RefreshWindowMetrics(service);
        Revision(service, observation->m_Revision, sizeof(observation->m_Revision));
        observation->m_Metrics = service->m_Metrics;
    }

    void Update(HService service)
    {
        uint64_t now = service->m_GetTime();
        RefreshWindowMetrics(service);
        if (service->m_Active < 0)
        {
            return;
        }
        Resize* resize = &service->m_Resizes[service->m_Active];
        if (now >= resize->m_Deadline)
        {
            Finish(service, resize, now, "resize_not_observed", "Requested client size was not observed before the deadline");
            return;
        }
        if (resize->m_State == RESIZE_QUEUED)
        {
            char revision[32];
            Revision(service, revision, sizeof(revision));
            if (resize->m_ExpectedRevision[0] && strcmp(resize->m_ExpectedRevision, revision))
            {
                Finish(service, resize, now, "window_revision_changed", "Window geometry changed before resize dispatch");
                return;
            }
            if (UnavailableReason(service))
            {
                Finish(service, resize, now, "resize_unavailable", "Window resizing became unavailable");
                return;
            }
            if (Matches(service, resize))
            {
                resize->m_AlreadyMatches = true;
                Finish(service, resize, now, 0, 0);
                return;
            }
            resize->m_State = RESIZE_AWAITING_WINDOW;
            dmPlatform::SetWindowSize(service->m_Window, resize->m_Width, resize->m_Height);
            RefreshWindowMetrics(service);
        }
        resize->m_Observed = service->m_Metrics;
        Revision(service, resize->m_Revision, sizeof(resize->m_Revision));
        if (Matches(service, resize))
        {
            Finish(service, resize, now, 0, 0);
        }
        else if (UnavailableReason(service))
        {
            Finish(service, resize, now, "resize_unavailable", "Window resizing became unavailable");
        }
    }

    void Detach(HService service)
    {
        service->m_Window = 0;
        service->m_AttachmentChanged = true;
        memset(&service->m_Metrics, 0, sizeof(service->m_Metrics));
        memset(service->m_Resizes, 0, sizeof(service->m_Resizes));
        service->m_Active = -1;
    }

    static void SendWindowMetrics(Service* service, dmWebServer::Request* request)
    {
        const WindowMetrics* state = &service->m_Metrics;
        char window[64], drawable[64], revision[32], reason[64];
        Dimensions(window, sizeof(window), state->m_WindowAvailable, state->m_WindowWidth, state->m_WindowHeight);
        Dimensions(drawable, sizeof(drawable), state->m_DrawableAvailable, state->m_DrawableWidth, state->m_DrawableHeight);
        Revision(service, revision, sizeof(revision));
        const char* unavailable = UnavailableReason(service);
        dmSnPrintf(reason, sizeof(reason), "\"%s\"", unavailable ? unavailable : "");
        char native_window[96] = "null";
        dmPlatform::NativeWindowId identity = dmPlatform::GetNativeWindowId(service->m_Window);
        if (identity.m_Type != dmPlatform::NATIVE_WINDOW_TYPE_NONE)
        {
            static const char* types[] = { "none", "win32", "cocoa", "x11" };
            // A decimal string preserves all handle bits in clients with double-precision JSON numbers.
            dmSnPrintf(native_window, sizeof(native_window), "{\"type\":\"%s\",\"id\":\"%llu\"}",
                       types[identity.m_Type], (unsigned long long)identity.m_Id);
        }
        // Format fixed-point components so the game's numeric locale cannot change the JSON decimal point.
        uint64_t display_scale = (uint64_t)(state->m_DisplayScale * 1000000.0 + 0.5);
        char body[1024];
        int length = dmSnPrintf(body, sizeof(body),
                                "{\"revision\":\"%s\",\"window\":%s,\"drawable\":%s,\"display_scale\":%llu.%06llu,"
                                "\"native_window\":%s,\"resize\":{\"supported\":%s,\"unavailable_reason\":%s,\"max_operations\":64,\"retention_ms\":60000}}",
                                revision, window, drawable,
                                (unsigned long long)(display_scale / 1000000), (unsigned long long)(display_scale % 1000000),
                                native_window, unavailable ? "false" : "true", unavailable ? reason : "null");
        SendJson(request, 200, body, length, sizeof(body));
    }

    static void SendResize(Resize* resize, dmWebServer::Request* request, int status, uint64_t now)
    {
        char window[64], drawable[64], error[384] = "null", expires[24] = "null", revision[36];
        const WindowMetrics* observed = &resize->m_Observed;
        Dimensions(window, sizeof(window), observed->m_WindowAvailable, observed->m_WindowWidth, observed->m_WindowHeight);
        Dimensions(drawable, sizeof(drawable), observed->m_DrawableAvailable, observed->m_DrawableWidth, observed->m_DrawableHeight);
        if (resize->m_Error)
        {
            dmSnPrintf(error, sizeof(error), "{\"code\":\"%s\",\"message\":\"%s\"}", resize->m_Error, resize->m_Message);
        }
        if (resize->m_Expires)
        {
            dmSnPrintf(expires, sizeof(expires), "%llu", (unsigned long long)((resize->m_Expires - now) / 1000));
        }
        dmSnPrintf(revision, sizeof(revision), "\"%s\"", resize->m_Revision);
        static const char* states[] = { "queued", "awaiting_window", "complete", "failed" };
        char body[1280];
        int length = dmSnPrintf(body, sizeof(body),
                                "{\"operation_id\":\"r%llu\",\"state\":\"%s\",\"expires_in_ms\":%s,"
                                "\"requested\":{\"width\":%u,\"height\":%u},\"observed\":{\"window\":%s,\"drawable\":%s},"
                                "\"window_revision\":%s,\"already_matches\":%s,\"error\":%s}",
                                (unsigned long long)resize->m_Id, states[resize->m_State], expires,
                                resize->m_Width, resize->m_Height, window, drawable, resize->m_Revision[0] ? revision : "null",
                                resize->m_AlreadyMatches ? "true" : "false", error);
        char location[48];
        dmSnPrintf(location, sizeof(location), "/window/resize/r%llu", (unsigned long long)resize->m_Id);
        SendJson(request, status, body, length, sizeof(body), status == 202 ? "Location" : 0, location);
    }

    static bool ParseUInt(const char** p, uint64_t* value)
    {
        if (**p < '0' || **p > '9')
        {
            return false;
        }
        const char* start = *p;
        *value = 0;
        do
        {
            uint32_t digit = *(*p)++ - '0';
            if (*value > (UINT64_MAX - digit) / 10)
            {
                return false;
            }
            *value = *value * 10 + digit;
        } while (**p >= '0' && **p <= '9');
        return *start != '0' || *p == start + 1;
    }

    static bool ParseResize(const char* body, uint32_t size, Resize* resize)
    {
        dmJson::Document json(32);
        if (!json.Parse(body, size, 0, dmJson::PARSE_DECODE_VALUES) || json.Get(0).m_Type != dmJson::TYPE_OBJECT)
        {
            return false;
        }
        for (int field = json.Get(0).m_Container.m_First; field >= 0; field = json.Get(field).m_Next)
        {
            const char* name = json.Get(field).Name(json.m_Text);
            if (strcmp(name, "width") && strcmp(name, "height") && strcmp(name, "timeout_ms") && strcmp(name, "expected_window_revision") && strcmp(name, "supersede"))
            {
                return false;
            }
        }
        int width = json.Integer(json.Field(0, "width"), -1);
        int height = json.Integer(json.Field(0, "height"), -1);
        int timeout_field = json.Field(0, "timeout_ms");
        int timeout = timeout_field < 0 ? 30000 : json.Integer(timeout_field, -1);
        if (width <= 0 || height <= 0 || timeout <= 0 || timeout > 60000)
        {
            return false;
        }
        int revision_field = json.Field(0, "expected_window_revision");
        if (revision_field >= 0)
        {
            const char* revision = json.String(revision_field, 0);
            if (!revision || !revision[0] || strlen(revision) >= sizeof(resize->m_ExpectedRevision))
            {
                return false;
            }
            dmStrlCpy(resize->m_ExpectedRevision, revision, sizeof(resize->m_ExpectedRevision));
        }
        int supersede_field = json.Field(0, "supersede");
        if (supersede_field >= 0 && json.Get(supersede_field).m_Type != dmJson::TYPE_BOOL)
        {
            return false;
        }
        resize->m_Supersede = json.Boolean(supersede_field, false);
        resize->m_Width = (uint32_t)width;
        resize->m_Height = (uint32_t)height;
        resize->m_Timeout = (uint32_t)timeout;
        return true;
    }

    static void Handler(void* context, dmWebServer::Request* request)
    {
        Service* service = (Service*)context;
        const char* path = request->m_Resource;
        bool metrics = !strcmp(path, "/window");
        bool resize = !strcmp(path, "/window/resize");
        bool receipt = !strncmp(path, "/window/resize/", 15) && path[15] && !strpbrk(path + 15, "/?");
        if (!metrics && !resize && !receipt)
        {
            Reject(request, 404, "not_found", "Window route does not exist");
            return;
        }
        const char* allowed = resize ? "POST" : "GET";
        if (strcmp(request->m_Method, allowed))
        {
            Reject(request, 405, "method_not_allowed", "Method is not supported by this window route", allowed);
            return;
        }
        if (!resize && request->m_ContentLength)
        {
            Reject(request, 400, "unexpected_body", "This route does not accept a body");
            return;
        }
        if (metrics)
        {
            RefreshWindowMetrics(service);
            SendWindowMetrics(service, request);
            return;
        }
        // The engine service validates supplied instance headers before dispatch.
        // Receipt reads require one because IDs are local to this service.
        const char* instance = dmWebServer::GetHeader(request, "X-Engine-Instance");
        if (receipt && (!instance || !instance[0]))
        {
            Reject(request, 400, "missing_instance", "X-Engine-Instance from /state is required");
            return;
        }
        if (receipt)
        {
            uint64_t now = service->m_GetTime();
            const char* number = path + 15;
            uint64_t sequence;
            if (*number++ != 'r' || !ParseUInt(&number, &sequence) || !sequence || *number)
            {
                Error(request, 404, "not_found", "Resize does not exist");
                return;
            }
            Resize* operation = &service->m_Resizes[(sequence - 1) % MAX_RESIZES];
            if (operation->m_Id == sequence && (!operation->m_Expires || now < operation->m_Expires))
            {
                SendResize(operation, request, 200, now);
                return;
            }
            Error(request, sequence <= service->m_Sequence ? 410 : 404,
                  sequence <= service->m_Sequence ? "expired_operation" : "not_found", "Resize receipt is unavailable");
            return;
        }
        if (request->m_ContentLength > 1024)
        {
            Reject(request, 413, "body_too_large", "Resize body must not exceed 1024 bytes");
            return;
        }
        const char* type = dmWebServer::GetHeader(request, "Content-Type");
        if (!type || strncmp(type, "application/json", 16) || (type[16] && type[16] != ';'))
        {
            Reject(request, 400, "invalid_content_type", "Use application/json");
            return;
        }
        char body[1025];
        dmWebServer::Result result = dmWebServer::ReceiveBuffered(request, body, sizeof(body));
        if (result == dmWebServer::RESULT_TRY_AGAIN)
        {
            return;
        }
        if (result != dmWebServer::RESULT_OK || memchr(body, 0, request->m_ContentLength))
        {
            Reject(request, 400, "invalid_body", "Unable to read resize body");
            return;
        }
        body[request->m_ContentLength] = 0;
        Resize operation = {};
        if (!ParseResize(body, request->m_ContentLength, &operation))
        {
            Error(request, 400, "invalid_options", "Request does not match the resize schema in /openapi.json");
            return;
        }
        RefreshWindowMetrics(service);
        if (UnavailableReason(service))
        {
            Error(request, 501, "resize_unavailable", "Window resizing is unavailable; inspect /window");
            return;
        }
        char revision[32];
        Revision(service, revision, sizeof(revision));
        if (operation.m_ExpectedRevision[0] && strcmp(operation.m_ExpectedRevision, revision))
        {
            Error(request, 409, "window_revision_changed", "Window geometry changed; reacquire /window");
            return;
        }
        uint64_t now = service->m_GetTime();
        if (service->m_Active >= 0)
        {
            Resize* active = &service->m_Resizes[service->m_Active];
            if (now >= active->m_Deadline)
            {
                Finish(service, active, now, "resize_not_observed", "Requested client size was not observed before the deadline");
            }
            else if (operation.m_Supersede)
            {
                Finish(service, active, now, "resize_superseded", "Resize was superseded by a newer request");
            }
            else
            {
                Error(request, 409, "resize_busy", "Another resize is pending");
                return;
            }
        }
        operation.m_Id = ++service->m_Sequence;
        operation.m_Deadline = now + (uint64_t)operation.m_Timeout * 1000;
        service->m_Active = (int32_t)((operation.m_Id - 1) % MAX_RESIZES);
        service->m_Resizes[service->m_Active] = operation;
        SendResize(&service->m_Resizes[service->m_Active], request, 202, now);
    }

    HService New(dmWebServer::HServer server, FGetTime get_time)
    {
        Service* service = new Service();
        service->m_Server = server;
        service->m_GetTime = get_time ? get_time : dmTime::GetMonotonicTime;
        service->m_Active = -1;
        dmWebServer::HandlerParams params;
        params.m_Handler = Handler;
        params.m_Userdata = service;
        if (dmWebServer::AddHandler(server, "/window", &params, WINDOW_OPENAPI) != dmWebServer::RESULT_OK)
        {
            delete service;
            return 0;
        }
        return service;
    }

    void Delete(HService service)
    {
        dmWebServer::RemoveHandler(service->m_Server, "/window");
        delete service;
    }
}
