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

#ifndef DM_ENGINE_SERVICE_PERFETTO_H
#define DM_ENGINE_SERVICE_PERFETTO_H

#include <stdint.h>
#include <dlib/webserver.h>

namespace dmEngineService
{
    enum SystemTraceState
    {
        SYSTEM_TRACE_IDLE,
        SYSTEM_TRACE_PREPARED,
        SYSTEM_TRACE_TRIGGER_PENDING,
        SYSTEM_TRACE_TRIGGER_SENT,
        SYSTEM_TRACE_FAILED,
    };

    // Engine-thread state. The app owns only its short-lived trigger helper;
    // the host owns the privileged Perfetto recording and its output file.
    struct SystemTrace
    {
        SystemTraceState m_State;
        int              m_TriggerProcess;
        uint32_t         m_DurationMs;
        char             m_Package[256];
        char             m_CaptureId[64];
        char             m_Trigger[96];
        char             m_Error[256];
    };

    void SystemTraceRequest(void* context, dmWebServer::Request* request);
    void UpdateSystemTrace(SystemTrace* trace);
    void FinalizeSystemTrace(SystemTrace* trace);

    // Platform implementation, private to the engine service. Child process
    // ownership transfers to the caller and ends when polled or stopped.
    bool             SystemTraceAppInfo(char* package, uint32_t package_size);
    int              SystemTraceGetAndroidApiLevel();
    int              SystemTraceStartTrigger(const char* trigger, char* error, uint32_t error_size);
    SystemTraceState SystemTracePollTrigger(int* process, char* error, uint32_t error_size);
    void             SystemTraceStopTrigger(int* process);

    static const char SYSTEM_TRACE_OPENAPI[] = R"json(
{
  "/profiler/perfetto": {
    "get": {
      "summary": "Get Android CPU/GPU system-trace coordination status",
      "description": "Debug engine only. Uses Android's built-in Perfetto tooling; no GFXReconstruct layer or graphics API changes are required. A host running APA/ADB owns the full system recording. The app can signal a named Perfetto trigger but cannot start a privileged system recording or inspect its completion. trigger_sent means the helper delivered the signal, not that a matching recorder was armed or that a trace is complete. GPU data depends on the device and driver; configure additional hardware counters in APA when needed.",
      "responses": {
        "200": {
          "description": "Current capture plan and trigger-helper status. Once triggered, wait for the host recording command to exit successfully and check that its output file contains data.",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["ok", "data"],
                "properties": {
                  "ok": {"type": "boolean", "enum": [true]},
                  "data": {
                    "type": "object",
                    "required": ["state", "trigger_available", "recording_owner", "package", "capture_id", "duration_ms"],
                    "properties": {
                      "state": {"type": "string", "enum": ["idle", "prepared", "trigger_pending", "trigger_sent", "failed"]},
                      "trigger_available": {"type": "boolean", "description": "The app can execute Android's trigger_perfetto helper."},
                      "recording_owner": {"type": "string", "enum": ["host"]},
                      "package": {"type": "string"},
                      "capture_id": {"type": "string"},
                      "duration_ms": {"type": "integer"},
                      "trigger_name": {"type": "string"},
                      "path": {"type": "string", "description": "Relative output filename on the host, in the directory where record_command is run. The app cannot verify file readiness."},
                      "config_url": {"type": "string", "description": "Relative URL for this capture's Perfetto text configuration. A superseded capture URL returns HTTP 409."},
                      "config_filename": {"type": "string"},
                      "record_command": {"type": "string", "description": "Run on the host after saving the config. On Windows, use Command Prompt (cmd.exe). Streams binary trace data directly to path on the host and blocks until tracing finishes. Use another terminal or an asynchronous host process to send the HTTP trigger. Wait for a successful exit and nonempty output before opening the file."},
                      "viewer_url": {"type": "string", "format": "uri"},
                      "error": {"type": "string"}
                    }
                  }
                }
              }
            }
          }
        }
      }
    },
    "post": {
      "summary": "Prepare or trigger an Android Perfetto system trace",
      "description": "1. POST with action=prepare (the default) and an optional duration_ms. 2. GET config_url and save its body under config_filename. 3. Start record_command on the host to arm Perfetto. 4. POST action=trigger and the capture_id. If the host recorder is still running but its output file stays empty, retry the same capture_id once the helper leaves trigger_pending. Stop retrying when trace data starts arriving. Retries also work after a helper failure and do not restart an active recording. The prepared recorder waits up to 120 seconds for the trigger, then records for duration_ms. Buffers are drained to host stdout every second during recording. The config collects CPU scheduling/frequency, process information, Android graphics events, and GPU frequency/memory/render-stage/frame-timeline data where supported. APA can also use the returned trigger name in a custom Perfetto config. Preparing a new plan does not stop any existing host recording and changes the trigger name, so it cannot recover the previous recorder. Request headers and body together must be smaller than 65536 bytes.",
      "requestBody": {
        "content": {
          "application/json": {
            "schema": {
              "type": "object",
              "additionalProperties": false,
              "properties": {
                "action": {"type": "string", "enum": ["prepare", "trigger"], "default": "prepare"},
                "duration_ms": {"type": "integer", "minimum": 1, "maximum": 300000, "default": 1000, "description": "Only valid for prepare."},
                "capture_id": {"type": "string", "description": "Required for trigger; must match the current capture plan. Reuse the same ID when retrying signal delivery."}
              }
            }
          }
        }
      },
      "responses": {
        "200": {"$ref": "#/paths/~1profiler~1perfetto/get/responses/200"},
        "400": {"description": "Invalid action, duration or JSON options."},
        "409": {"description": "No matching capture plan, or a trigger helper is still running."},
        "413": {"description": "Request headers and body together exceed 65535 bytes."},
        "500": {"description": "Unable to start the trigger helper."},
        "501": {"description": "The Android trigger helper is unavailable. APA/ADB can still record system traces independently."}
      }
    }
  },
  "/profiler/perfetto/config/{capture_id}": {
    "get": {
      "summary": "Download the prepared Perfetto text configuration",
      "parameters": [
        {"name": "capture_id", "in": "path", "required": true, "schema": {"type": "string"}}
      ],
      "responses": {
        "200": {"description": "Save as config_filename on the host, then run record_command.", "content": {"text/plain": {"schema": {"type": "string"}}}},
        "409": {"description": "The capture ID does not match the current plan."}
      }
    }
  }
}
)json";
}

#endif // DM_ENGINE_SERVICE_PERFETTO_H
