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

#ifndef DM_PLATFORM_SERVICE_OPENAPI_H
#define DM_PLATFORM_SERVICE_OPENAPI_H

namespace dmPlatformService
{
    static const char WINDOW_OPENAPI[] = R"json(
{
  "/window": {
    "get": {
      "summary": "Get window client dimensions and drawable pixels",
      "responses": {
        "200": {
          "description": "OK",
          "content": {"application/json": {"schema": {
            "type": "object",
            "required": ["revision", "window", "drawable", "display_scale", "resize"],
            "properties": {
              "revision": {"type": "string", "description": "Observed geometry revision."},
              "window": {"type": "object", "nullable": true, "required": ["width", "height"], "properties": {"width": {"type": "integer"}, "height": {"type": "integer"}}},
              "drawable": {"$ref": "#/paths/~1window/get/responses/200/content/application~1json/schema/properties/window"},
              "display_scale": {"type": "number"},
              "resize": {"type": "object", "required": ["supported", "unavailable_reason", "max_operations", "retention_ms"], "properties": {"supported": {"type": "boolean"}, "unavailable_reason": {"type": "string", "nullable": true}, "max_operations": {"type": "integer"}, "retention_ms": {"type": "integer"}}}
            }
          }}}
        },
        "400": {"description": "Unexpected body"},
        "405": {"description": "Method not allowed"}
      }
    }
  },
  "/window/resize": {
    "post": {
      "summary": "Resize the window in client units",
      "description": "Preserves project reference resolution. Resizing waits for simulation updates.",
      "parameters": [{"name": "X-Engine-Instance", "in": "header", "required": false, "schema": {"type": "string"}, "description": "Optional engine_instance_id from /state. Supply and retain it when polling the receipt."}],
      "requestBody": {
        "required": true,
        "content": {"application/json": {"schema": {
          "type": "object", "additionalProperties": false, "required": ["width", "height"],
          "properties": {
            "width": {"type": "integer", "minimum": 1, "maximum": 2147483647},
            "height": {"type": "integer", "minimum": 1, "maximum": 2147483647},
            "expected_window_revision": {"type": "string", "minLength": 1, "maxLength": 31, "description": "Revision from /window."},
            "supersede": {"type": "boolean", "default": false, "description": "Replace a pending resize after request validation. The replaced receipt fails with resize_superseded. Expired pending resizes fail with resize_not_observed before admitting a new request."},
            "timeout_ms": {"type": "integer", "minimum": 1, "maximum": 60000, "default": 30000}
          }
        }}}
      },
      "responses": {
        "202": {
          "description": "Accepted. Clients that require completion can poll Location with the original engine instance header.",
          "headers": {"Location": {"schema": {"type": "string"}}},
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1window~1resize~1{id}/get/responses/200/content/application~1json/schema"}}}
        },
        "400": {"description": "Invalid request"},
        "405": {"description": "Method not allowed"},
        "409": {"description": "Resize pending without supersede, or window revision changed"},
        "410": {"description": "Engine instance changed"},
        "413": {"description": "Body exceeds 1024 bytes"},
        "501": {"description": "Resizing unavailable; inspect /window"}
      }
    }
  },
  "/window/resize/{id}": {
    "get": {
      "summary": "Get a resize receipt",
      "parameters": [
        {"name": "id", "in": "path", "required": true, "schema": {"type": "string"}},
        {"name": "X-Engine-Instance", "in": "header", "required": true, "schema": {"type": "string"}, "description": "engine_instance_id retained from before resize submission."}
      ],
      "responses": {
        "200": {
          "description": "Receipt; complete means matching client dimensions, not a rendered frame.",
          "content": {"application/json": {"schema": {
            "type": "object", "required": ["operation_id", "state", "expires_in_ms", "requested", "observed", "window_revision", "already_matches", "error"],
            "properties": {
              "operation_id": {"type": "string"},
              "state": {"type": "string", "enum": ["queued", "awaiting_window", "complete", "failed"]},
              "expires_in_ms": {"type": "integer", "nullable": true, "minimum": 0, "maximum": 60000},
              "requested": {"type": "object", "required": ["width", "height"], "properties": {"width": {"type": "integer"}, "height": {"type": "integer"}}},
              "observed": {"type": "object", "required": ["window", "drawable"], "properties": {
                "window": {"$ref": "#/paths/~1window/get/responses/200/content/application~1json/schema/properties/window"},
                "drawable": {"$ref": "#/paths/~1window/get/responses/200/content/application~1json/schema/properties/window"}
              }},
              "window_revision": {"type": "string", "nullable": true},
              "already_matches": {"type": "boolean"},
              "error": {"type": "object", "nullable": true, "required": ["code", "message"], "properties": {"code": {"type": "string"}, "message": {"type": "string"}}}
            }
          }}}
        },
        "400": {"description": "Unexpected body or missing instance"},
        "404": {"description": "Unknown receipt"},
        "405": {"description": "Method not allowed"},
        "410": {"description": "Receipt expired or engine instance changed"}
      }
    }
  }
}
)json";
}

#endif
