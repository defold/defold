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

#include "engine_service_systemtrace.h"

#include <string.h>
#include <unistd.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/json.hpp>
#include <dlib/time.h>

namespace dmEngineService
{
    // Keep a message field last for Android 10's Perfetto text parser.
    static const char CONFIG_TEMPLATE[] =
        "allow_user_build_tracing: true\n"
        "write_into_file: true\n"
        "file_write_period_ms: 1000\n"
        "flush_period_ms: 1000\n"
        "buffers { size_kb: 16384 fill_policy: RING_BUFFER }\n"
        "buffers { size_kb: 1024 fill_policy: RING_BUFFER }\n"
        "data_sources { config {\n"
        "  name: \"linux.ftrace\"\n"
        "  ftrace_config {\n"
        "    ftrace_events: \"sched/sched_switch\"\n"
        "    ftrace_events: \"sched/sched_waking\"\n"
        "    ftrace_events: \"power/cpu_frequency\"\n"
        "    ftrace_events: \"power/cpu_idle\"\n"
        "    ftrace_events: \"power/gpu_frequency\"\n"
        "    ftrace_events: \"gpu_mem/gpu_mem_total\"\n"
        "    atrace_categories: \"gfx\"\n"
        "    atrace_categories: \"view\"\n"
        "    atrace_categories: \"wm\"\n"
        "    atrace_categories: \"am\"\n"
        "    atrace_apps: \"%s\"\n"
        "  }\n"
        "} }\n"
        "data_sources { config {\n"
        "  name: \"linux.process_stats\"\n"
        "  target_buffer: 1\n"
        "  process_stats_config { scan_all_processes_on_start: true }\n"
        "} }\n"
        "data_sources { config { name: \"gpu.renderstages\" } }\n"
        "data_sources { config { name: \"android.surfaceflinger.frametimeline\" } }\n"
        "trigger_config {\n"
        "  trigger_mode: START_TRACING\n"
        "  triggers { name: \"%s\" stop_delay_ms: %u }\n"
        "  trigger_timeout_ms: 120000\n"
        "}\n";

    static void SendText(dmWebServer::Request* request, const char* text)
    {
        dmWebServer::Send(request, text, strlen(text));
    }

    static void WriteJson(void* context, const char* text, uint32_t size)
    {
        dmWebServer::Send((dmWebServer::Request*)context, text, size);
    }

    static void SendString(dmWebServer::Request* request, const char* text)
    {
        dmJson::WriteString(text, strlen(text), WriteJson, request);
    }

    static void SendHeaders(dmWebServer::Request* request, int status, const char* content_type)
    {
        dmWebServer::SetStatusCode(request, status);
        dmWebServer::SendAttribute(request, "Content-Type", content_type);
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
    }

    static void SendError(dmWebServer::Request* request, int status, const char* code, const char* message)
    {
        SendHeaders(request, status, "application/json");
        if (status == 405)
        {
            dmWebServer::SendAttribute(request, "Allow", !strcmp(request->m_Resource, "/profiler/perfetto/config") ? "GET" : "GET, POST");
        }

        char text[128];
        dmSnPrintf(text, sizeof(text), "{\"ok\":false,\"error\":{\"status\":%d,\"code\":", status);
        SendText(request, text);
        SendString(request, code);
        SendText(request, ",\"message\":");
        SendString(request, message);
        SendText(request, "}}");
    }

    static const char* StateName(SystemTraceState state)
    {
        switch (state)
        {
            case SYSTEM_TRACE_PREPARED:
            {
                return "prepared";
            }
            case SYSTEM_TRACE_TRIGGER_PENDING:
            {
                return "trigger_pending";
            }
            case SYSTEM_TRACE_TRIGGER_SENT:
            {
                return "trigger_sent";
            }
            case SYSTEM_TRACE_FAILED:
            {
                return "failed";
            }
            default:
            {
                return "idle";
            }
        }
    }

    static void SendStatus(SystemTrace* trace, dmWebServer::Request* request, bool available)
    {
        SendHeaders(request, 200, "application/json");
        char text[1024];
        dmSnPrintf(text, sizeof(text), "{\"ok\":true,\"data\":{\"state\":\"%s\",\"trigger_available\":%s,\"recording_owner\":\"host\",\"duration_ms\":%u,\"package\":",
                   StateName(trace->m_State), available ? "true" : "false", trace->m_DurationMs);
        SendText(request, text);
        SendString(request, trace->m_Package);
        SendText(request, ",\"capture_id\":");
        SendString(request, trace->m_CaptureId);
        if (trace->m_CaptureId[0])
        {
            SendText(request, ",\"trigger_name\":");
            SendString(request, trace->m_Trigger);
            SendText(request, ",\"path\":");
            dmSnPrintf(text, sizeof(text), "defold-%s.perfetto-trace", trace->m_CaptureId);
            SendString(request, text);
            SendText(request, ",\"config_url\":\"/profiler/perfetto/config\",\"config_filename\":");
            dmSnPrintf(text, sizeof(text), "defold-%s.pbtxt", trace->m_CaptureId);
            SendString(request, text);
            SendText(request, ",\"record_command\":");
            // A shell-owned pipe supports older Android SELinux rules. Keep
            // the recorder's exit status even when cat succeeds.
            dmSnPrintf(text, sizeof(text), "adb shell -T \"set -o pipefail; perfetto --txt -c - -o - | cat\" < defold-%s.pbtxt > defold-%s.perfetto-trace", trace->m_CaptureId, trace->m_CaptureId);
            SendString(request, text);
        }

        if (trace->m_Error[0])
        {
            SendText(request, ",\"error\":");
            SendString(request, trace->m_Error);
        }

        SendText(request, ",\"viewer_url\":\"https://ui.perfetto.dev\"}}");
    }

    struct Options
    {
        bool        m_Trigger;
        uint32_t    m_DurationMs;
        const char* m_CaptureId;
    };

    static bool ParseOptions(dmJson::Document* json, const char* body, uint32_t size, Options* options)
    {
        options->m_Trigger = false;
        options->m_DurationMs = 1000;
        options->m_CaptureId = 0;
        if (!size)
        {
            return true;
        }

        if (!json->Parse(body, size, 0, dmJson::PARSE_DECODE_VALUES) || json->Get(0).m_Type != dmJson::TYPE_OBJECT)
        {
            return false;
        }

        bool duration_supplied = false;
        for (int field = json->Get(0).m_Container.m_First; field >= 0; field = json->Get(field).m_Next)
        {
            const char* name = json->Name(field);
            if (!strcmp(name, "action"))
            {
                const char* action = json->String(field, "");
                if (!strcmp(action, "trigger"))
                {
                    options->m_Trigger = true;
                }
                else if (strcmp(action, "prepare"))
                {
                    return false;
                }
            }
            else if (!strcmp(name, "duration_ms"))
            {
                duration_supplied = true;
                if (!json->UnsignedInteger(field, &options->m_DurationMs) || !options->m_DurationMs || options->m_DurationMs > 300000)
                {
                    return false;
                }
            }
            else if (!strcmp(name, "capture_id"))
            {
                options->m_CaptureId = json->String(field, 0);
                if (!options->m_CaptureId || !options->m_CaptureId[0])
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }

        return options->m_Trigger ? options->m_CaptureId && !duration_supplied : !options->m_CaptureId;
    }

    void SystemTraceRequest(void* context, dmWebServer::Request* request)
    {
        const uint32_t max_request_size = 64 * 1024;
        dmArray<char> body;
        dmWebServer::Result received = dmWebServer::RESULT_ERROR_INVAL;
        if (request->m_ContentLength < max_request_size)
        {
            body.SetCapacity(request->m_ContentLength);
            received = dmWebServer::ReceiveBuffered(request, body.Begin(), body.Capacity());
        }

        if (received == dmWebServer::RESULT_TRY_AGAIN)
        {
            return;
        }

        if (received != dmWebServer::RESULT_OK)
        {
            dmWebServer::CloseConnection(request);
            bool too_large = received == dmWebServer::RESULT_ERROR_INVAL;
            SendError(request, too_large ? 413 : 400, too_large ? "request_too_large" : "bad_request", "unable to buffer request; headers and body must be smaller than 65536 bytes");
            return;
        }

        bool config_request = !strcmp(request->m_Resource, "/profiler/perfetto/config");
        if (!config_request && strcmp(request->m_Resource, "/profiler/perfetto"))
        {
            SendError(request, 404, "not_found", "unknown system trace endpoint");
            return;
        }

        if (!dmStrEq(request->m_Method, "GET") && (!dmStrEq(request->m_Method, "POST") || config_request))
        {
            SendError(request, 405, "method_not_allowed", config_request ? "use GET" : "use GET or POST");
            return;
        }

        SystemTrace* trace = (SystemTrace*)context;
        if (config_request)
        {
            if (!trace->m_CaptureId[0])
            {
                SendError(request, 409, "systemtrace_not_prepared", "prepare a capture before downloading its configuration");
                return;
            }

            char config[4096];
            int length = dmSnPrintf(config, sizeof(config), CONFIG_TEMPLATE, trace->m_Package, trace->m_Trigger, trace->m_DurationMs);
            SendHeaders(request, 200, "text/plain");
            dmWebServer::Send(request, config, length);
            return;
        }

        bool available = SystemTraceAppInfo(trace->m_Package, sizeof(trace->m_Package));
        if (dmStrEq(request->m_Method, "POST"))
        {
            dmJson::Document json(16);
            Options options;
            if (!ParseOptions(&json, body.Begin(), request->m_ContentLength, &options))
            {
                SendError(request, 400, "bad_request", "prepare accepts duration_ms from 1 to 300000; trigger requires capture_id");
                return;
            }

            if (trace->m_State == SYSTEM_TRACE_TRIGGER_PENDING)
            {
                SendError(request, 409, "systemtrace_busy", "the trigger helper is still running");
                return;
            }

            if (!available)
            {
                SendError(request, 501, "systemtrace_unavailable", "Android's trigger_perfetto helper is unavailable; APA/ADB can still record system traces independently");
                return;
            }

            if (options.m_Trigger)
            {
                if (!trace->m_CaptureId[0] || strcmp(trace->m_CaptureId, options.m_CaptureId))
                {
                    SendError(request, 409, "systemtrace_not_prepared", "capture_id must match the current capture plan");
                    return;
                }

                trace->m_Error[0] = 0;
                trace->m_TriggerProcess = SystemTraceStartTrigger(trace->m_Trigger, trace->m_Error, sizeof(trace->m_Error));
                if (trace->m_TriggerProcess <= 0)
                {
                    trace->m_TriggerProcess = 0;
                    trace->m_State = SYSTEM_TRACE_FAILED;
                    SendError(request, 500, "systemtrace_trigger_failed", trace->m_Error);
                    return;
                }

                trace->m_State = SYSTEM_TRACE_TRIGGER_PENDING;
            }
            else
            {
                static uint32_t serial = 0;
                dmSnPrintf(trace->m_CaptureId, sizeof(trace->m_CaptureId), "%x-%llx-%x", (unsigned int)getpid(), (unsigned long long)dmTime::GetMonotonicTime(), ++serial);
                dmSnPrintf(trace->m_Trigger, sizeof(trace->m_Trigger), "defold.systemtrace.%s", trace->m_CaptureId);
                trace->m_DurationMs = options.m_DurationMs;
                trace->m_Error[0] = 0;
                trace->m_State = SYSTEM_TRACE_PREPARED;
            }
        }

        SendStatus(trace, request, available);
    }

    void UpdateSystemTrace(SystemTrace* trace)
    {
        if (trace->m_State == SYSTEM_TRACE_TRIGGER_PENDING)
        {
            trace->m_State = SystemTracePollTrigger(&trace->m_TriggerProcess, trace->m_Error, sizeof(trace->m_Error));
        }
    }

    void FinalizeSystemTrace(SystemTrace* trace)
    {
        if (trace->m_TriggerProcess)
        {
            SystemTraceStopTrigger(&trace->m_TriggerProcess);
        }

        memset(trace, 0, sizeof(*trace));
    }

}
