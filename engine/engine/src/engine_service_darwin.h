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

#ifndef DM_ENGINE_SERVICE_DARWIN_H
#define DM_ENGINE_SERVICE_DARWIN_H

#include <stdint.h>
#include <dlib/webserver.h>

namespace dmEngineService
{
    enum GPUTraceState
    {
        GPU_TRACE_IDLE,
        GPU_TRACE_PENDING,
        GPU_TRACE_CAPTURING,
        GPU_TRACE_COMPLETE,
        GPU_TRACE_CANCELED,
        GPU_TRACE_FAILED,
    };

    // Owned by the engine service and accessed only on the engine thread.
    struct GPUTrace
    {
        GPUTraceState m_State;
        uint32_t m_Frames;
        uint32_t m_FramesCaptured;
        bool m_StopRequested;
        char m_Path[1024];
        char m_Error[512];
    };

    void GPUTraceRequest(void* context, dmWebServer::Request* request);
    void GPUTraceBeginFrame(GPUTrace* trace);
    void GPUTraceEndFrame(GPUTrace* trace);
    void GPUTraceStop(GPUTrace* trace);

    static const char GPU_TRACE_OPENAPI[] = R"json(
{
  "/profiler/gputrace": {
    "get": {
      "summary": "Get the current or most recent GPU trace capture",
      "description": "Requires a debug engine on macOS using Metal, launched with METAL_CAPTURE_ENABLED=1. Captures can be opened in Xcode or inspected with gpudebug.",
      "responses": {
        "200": {
          "description": "Capture status. Poll until complete, canceled or failed. The path is on the engine host.",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["ok", "data"],
                "properties": {
                  "ok": {"type": "boolean", "enum": [true]},
                  "data": {
                    "type": "object",
                    "required": ["state", "path", "frames", "frames_captured", "stop_requested"],
                    "properties": {
                      "state": {"type": "string", "enum": ["idle", "pending", "capturing", "complete", "canceled", "failed"]},
                      "path": {"type": "string"},
                      "frames": {"type": "integer"},
                      "frames_captured": {"type": "integer"},
                      "stop_requested": {"type": "boolean"},
                      "error": {"type": "string", "description": "Native capture startup error, when state is failed."}
                    }
                  }
                }
              }
            }
          }
        },
        "501": {"description": "GPU trace capture is unavailable for this adapter or launch environment."}
      }
    },
    "post": {
      "summary": "Schedule a Metal GPU trace",
      "description": "Starts before rendering the next frame and stops after Flip submits the requested number of complete frames. Use a writable absolute output path with no existing trace. Query parameters override JSON body fields. The UTF-8 path must fit in 1023 bytes. Request headers and body together must be smaller than 65536 bytes. Use JSON for long paths; request URLs are limited to 127 bytes.",
      "parameters": [
        {"name": "path", "in": "query", "schema": {"type": "string"}, "description": "Absolute output path ending in .gputrace. Required in either the query or body."},
        {"name": "frames", "in": "query", "schema": {"type": "integer", "minimum": 1, "maximum": 10000, "default": 1}}
      ],
      "requestBody": {
        "content": {
          "application/json": {
            "schema": {
              "type": "object",
              "properties": {
                "path": {"type": "string", "example": "/tmp/frame.gputrace"},
                "frames": {"type": "integer", "minimum": 1, "maximum": 10000, "default": 1}
              }
            }
          }
        }
      },
      "responses": {
        "200": {"$ref": "#/paths/~1profiler~1gputrace/get/responses/200"},
        "400": {"description": "Invalid JSON, path or frame count."},
        "409": {"description": "A capture is already pending or active."},
        "413": {"description": "Request headers and body together exceed 65535 bytes."},
        "414": {"description": "Output path exceeds 1023 UTF-8 bytes."},
        "501": {"description": "GPU trace capture is unavailable."}
      }
    },
    "delete": {
      "summary": "Stop a GPU trace",
      "description": "Cancels a pending capture or requests completion after the current rendered frame is submitted. Poll GET until terminal. Does not delete the trace file.",
      "responses": {
        "200": {"$ref": "#/paths/~1profiler~1gputrace/get/responses/200"},
        "409": {"description": "No capture is pending or active."},
        "501": {"description": "GPU trace capture is unavailable."}
      }
    }
  }
}
)json";
} // namespace dmEngineService

#endif // DM_ENGINE_SERVICE_DARWIN_H
