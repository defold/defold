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

#ifndef DM_RECORD_SERVICE_OPENAPI_H
#define DM_RECORD_SERVICE_OPENAPI_H

namespace dmRecord
{
    static const char RECORD_OPENAPI[] = R"json(
{
  "/recording/capabilities": {
    "get": {
      "summary": "Get native recording capabilities",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "version": {"type": "integer"},
                  "instance_id": {"type": "string", "description": "Recording service identity; changes on engine restart or reboot. Send as X-Recording-Instance for mutations."},
                  "supported": {"type": "boolean"},
                  "audio": {"type": "boolean"},
                  "backend": {"type": "string"},
                  "format": {"type": "string"},
                  "codec": {"type": "string"},
                  "minimum_platform_version": {"type": "string"},
                  "unsupported_reason": {"type": "string"},
                  "max_fps": {"type": "integer"}
                },
                "required": [
                  "version",
                  "instance_id",
                  "supported",
                  "audio",
                  "backend",
                  "format",
                  "codec",
                  "minimum_platform_version",
                  "unsupported_reason",
                  "max_fps"
                ]
              }
            }
          }
        },
        "400": {
          "description": "Malformed request or parameters.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    }
  },
  "/recording/status": {
    "get": {
      "summary": "Get the current recording and download receipt",
      "parameters": [{"name": "X-Recording-Instance", "in": "header", "schema": {"type": "string"}, "description": "Reject a receipt from a replacement recording service."}],
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "instance_id": {"type": "string"},
                  "operation_id": {"type": "integer"},
                  "state": {"type": "string", "enum": ["idle", "starting", "recording", "stopping", "complete", "failed"]},
                  "pending": {"type": "boolean"},
                  "supported": {"type": "boolean"},
                  "active": {"type": "boolean"},
                  "finalized": {"type": "boolean"},
                  "audio": {"type": "boolean"},
                  "backend": {"type": "string"},
                  "format": {"type": "string"},
                  "codec": {"type": "string"},
                  "failure": {"type": "string"},
                  "width": {"type": "integer"},
                  "height": {"type": "integer"},
                  "fps": {"type": "integer"},
                  "started_wall_time_us": {"type": "integer"},
                  "started_monotonic_time_us": {"type": "integer"},
                  "stopped_wall_time_us": {"type": "integer"},
                  "stopped_monotonic_time_us": {"type": "integer"},
                  "file": {
                    "type": "object",
                    "properties": {
                      "id": {"type": "string"},
                      "url": {"type": "string"},
                      "state": {"type": "string", "enum": ["pending", "complete", "failed"]},
                      "size": {"type": "integer"},
                      "sha256": {"type": "string", "description": "SHA-256 hex digest, available when state is complete."}
                    },
                    "required": ["id", "url", "state", "size", "sha256"],
                    "nullable": true
                  }
                },
                "required": [
                  "instance_id",
                  "operation_id",
                  "state",
                  "pending",
                  "supported",
                  "active",
                  "finalized",
                  "audio",
                  "backend",
                  "format",
                  "codec",
                  "failure",
                  "width",
                  "height",
                  "fps",
                  "started_wall_time_us",
                  "started_monotonic_time_us",
                  "stopped_wall_time_us",
                  "stopped_monotonic_time_us",
                  "file"
                ]
              }
            }
          }
        },
        "410": {
          "description": "Recording service instance is stale.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "400": {
          "description": "Malformed request or parameters.",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "error": {
                    "type": "object",
                    "properties": {"code": {"type": "string"}, "message": {"type": "string"}},
                    "required": ["code", "message"]
                  }
                },
                "required": ["error"]
              }
            }
          }
        }
      }
    }
  },
  "/recording/start": {
    "post": {
      "summary": "Start native H.264 MP4 recording",
      "parameters": [{"name": "X-Recording-Instance", "in": "header", "required": true, "schema": {"type": "string"}, "description": "instance_id from /recording/capabilities. Retain this identity with the accepted operation."}],
      "responses": {
        "202": {
          "description": "Accepted; poll /recording/status until pending is false.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/200/content/application~1json/schema"}}}
        },
        "400": {
          "description": "Malformed request, parameters or missing X-Recording-Instance.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "Recording is already active or pending.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "Recording service instance is stale.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "500": {
          "description": "Recording or storage initialization failed.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "501": {
          "description": "Video or requested audio is unsupported.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "507": {
          "description": "Recording storage is full; delete retained files before recording again.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      },
      "requestBody": {
        "content": {
          "application/json": {
            "schema": {
              "type": "object",
              "properties": {
                "fps": {"type": "integer", "minimum": 1, "maximum": 60, "default": 30},
                "audio": {"type": "boolean", "description": "Capture application audio. Defaults to backend support; query /recording/capabilities."}
              },
              "additionalProperties": false,
              "description": "Records at the native window content size. Resizing the window stops and finalizes the recording. Integer values use decimal integer notation."
            }
          }
        }
      }
    }
  },
  "/recording/stop": {
    "post": {
      "summary": "Stop and finalize the current recording",
      "parameters": [{"name": "X-Recording-Instance", "in": "header", "required": true, "schema": {"type": "string"}, "description": "instance_id retained from the accepted start receipt."}],
      "responses": {
        "202": {
          "description": "Accepted; poll /recording/status until pending is false.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/200/content/application~1json/schema"}}}
        },
        "400": {
          "description": "Malformed request, parameters or missing X-Recording-Instance.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "No active recording is ready to stop.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "Recording service instance or operation ID is stale.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      },
      "requestBody": {
        "required": true,
        "content": {
          "application/json": {
            "schema": {
              "type": "object",
              "properties": {"operation_id": {"type": "integer", "minimum": 1, "description": "ID from the accepted start receipt."}},
              "additionalProperties": false,
              "required": ["operation_id"]
            }
          }
        }
      }
    }
  },
  "/recording/files/{id}": {
    "parameters": [{"name": "id", "in": "path", "required": true, "schema": {"type": "string"}}],
    "get": {
      "summary": "Download a finalized recording",
      "parameters": [
        {"name": "X-Recording-Instance", "in": "header", "schema": {"type": "string"}, "description": "Reject a download from a replacement recording service."},
        {
          "name": "Range",
          "in": "header",
          "required": false,
          "schema": {"type": "string"},
          "example": "bytes=0-1023",
          "description": "Optional closed inclusive byte range within the file. Omit to stream the whole MP4. File size and SHA-256 are in the status receipt."
        }
      ],
      "responses": {
        "200": {
          "description": "Complete MP4 streamed with a bounded buffer; verify its size and SHA-256 against the status receipt.",
          "headers": {
            "Content-Length": {"schema": {"type": "integer"}},
            "Accept-Ranges": {"schema": {"type": "string"}},
            "ETag": {"schema": {"type": "string"}}
          },
          "content": {"video/mp4": {"schema": {"type": "string", "format": "binary"}}}
        },
        "206": {
          "description": "MP4 bytes; verify the assembled file against its SHA-256.",
          "headers": {
            "Content-Length": {"schema": {"type": "integer"}},
            "Content-Range": {"schema": {"type": "string"}},
            "Accept-Ranges": {"schema": {"type": "string"}},
            "ETag": {"schema": {"type": "string"}}
          },
          "content": {"video/mp4": {"schema": {"type": "string", "format": "binary"}}}
        },
        "400": {
          "description": "Unexpected request body or query.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "File is still recording or being prepared.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "File was deleted or the runtime restarted.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "416": {
          "description": "Range is invalid or outside the file.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "500": {
          "description": "File could not be prepared or read.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    },
    "delete": {
      "summary": "Delete a retained recording",
      "parameters": [{"name": "X-Recording-Instance", "in": "header", "required": true, "schema": {"type": "string"}, "description": "instance_id retained with the recording file receipt."}],
      "responses": {
        "204": {"description": "Deleted."},
        "400": {
          "description": "Unexpected request body, query or missing X-Recording-Instance.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "Recording or file preparation is still active.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "Recording service instance is stale or the file no longer exists.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    }
  }
}
)json";
}

#endif
