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
      "tags": ["Recording"],
      "summary": "Get native recording capabilities",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "version": {"type": "integer", "enum": [1]},
                  "supported": {"type": "boolean"},
                  "audio": {"type": "boolean"},
                  "backend": {"type": "string"},
                  "format": {"type": "string"},
                  "codec": {"type": "string"},
                  "minimum_platform_version": {"type": "string"},
                  "unsupported_reason": {"type": "string"},
                  "max_fps": {"type": "integer", "minimum": 0},
                  "max_dimension": {"type": "integer", "minimum": 0}
                },
                "additionalProperties": false,
                "required": [
                  "version",
                  "supported",
                  "audio",
                  "backend",
                  "format",
                  "codec",
                  "minimum_platform_version",
                  "unsupported_reason",
                  "max_fps",
                  "max_dimension"
                ]
              }
            }
          }
        },
        "400": {
          "description": "Malformed request or parameters.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "405": {
          "description": "Unsupported method; see the Allow header.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body must be smaller than 1024 bytes.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    }
  },
  "/recording/status": {
    "get": {
      "tags": ["Recording"],
      "summary": "Get the current recording and download receipt",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "operation_id": {"type": "integer", "minimum": 0},
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
                  "width": {"type": "integer", "minimum": 0},
                  "height": {"type": "integer", "minimum": 0},
                  "fps": {"type": "integer", "minimum": 0},
                  "started_wall_time_us": {"type": "integer", "minimum": 0},
                  "started_monotonic_time_us": {"type": "integer", "minimum": 0},
                  "stopped_wall_time_us": {"type": "integer", "minimum": 0},
                  "stopped_monotonic_time_us": {"type": "integer", "minimum": 0},
                  "file": {
                    "type": "object",
                    "properties": {
                      "id": {"type": "string"},
                      "url": {"type": "string"},
                      "state": {"type": "string", "enum": ["pending", "complete", "failed"]},
                      "size": {"type": "integer", "minimum": 0},
                      "sha256": {"type": "string", "description": "SHA-256 hex digest, available when state is complete."}
                    },
                    "additionalProperties": false,
                    "required": ["id", "url", "state", "size", "sha256"],
                    "nullable": true
                  }
                },
                "additionalProperties": false,
                "required": [
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
                    "additionalProperties": false,
                    "required": ["code", "message"]
                  }
                },
                "additionalProperties": false,
                "required": ["error"]
              }
            }
          }
        },
        "405": {
          "description": "Unsupported method; see the Allow header.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body must be smaller than 1024 bytes.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    }
  },
  "/recording/start": {
    "post": {
      "tags": ["Recording"],
      "summary": "Start native H.264 MP4 recording",
      "responses": {
        "202": {
          "description": "Accepted; poll /recording/status until pending is false.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/200/content/application~1json/schema"}}}
        },
        "400": {
          "description": "Malformed request or parameters.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "405": {
          "description": "Unsupported method; see the Allow header.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body must be smaller than 1024 bytes.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "Recording is already active or pending.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "415": {
          "description": "Use application/json.",
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
          "description": "128 recordings are retained; delete files before recording again.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      },
      "requestBody": {
        "required": false,
        "content": {
          "application/json": {
            "schema": {
              "type": "object",
              "properties": {
                "width": {"type": "integer", "minimum": 1, "maximum": 16384},
                "height": {"type": "integer", "minimum": 1, "maximum": 16384},
                "fps": {"type": "integer", "minimum": 1, "maximum": 60, "default": 30},
                "audio": {"type": "boolean", "description": "Capture application audio. Defaults to backend support; query /recording/capabilities. Windows audio requires build 20348 or newer."}
              },
              "additionalProperties": false,
              "required": [],
              "oneOf": [{"required": ["width", "height"]}, {"not": {"anyOf": [{"required": ["width"]}, {"required": ["height"]}]}}],
              "description": "Omit both dimensions to use the native window content size. No filesystem path is accepted. Integer values use decimal integer notation."
            }
          }
        }
      }
    }
  },
  "/recording/stop": {
    "post": {
      "tags": ["Recording"],
      "summary": "Stop and finalize the current recording",
      "responses": {
        "202": {
          "description": "Accepted; poll /recording/status until pending is false.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/200/content/application~1json/schema"}}}
        },
        "400": {
          "description": "Malformed request or parameters.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "405": {
          "description": "Unsupported method; see the Allow header.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body must be smaller than 1024 bytes.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "No active recording is ready to stop.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "Operation ID is stale.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "415": {
          "description": "Use application/json.",
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
      "tags": ["Recording"],
      "summary": "Download a finalized recording in chunks of at most 1 MiB",
      "parameters": [
        {
          "name": "Range",
          "in": "header",
          "required": true,
          "schema": {"type": "string", "pattern": "^bytes=[0-9]+-[0-9]+$"},
          "description": "Closed inclusive byte range, for example bytes=0-1023. File size and SHA-256 are in the status receipt."
        }
      ],
      "responses": {
        "206": {
          "description": "MP4 bytes; verify the assembled file against its SHA-256.",
          "headers": {
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
        "405": {
          "description": "Unsupported method.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body too large.",
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
          "description": "Range is missing, invalid or larger than 1 MiB.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "500": {
          "description": "File could not be prepared or read.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    },
    "delete": {
      "tags": ["Recording"],
      "summary": "Delete a retained recording",
      "responses": {
        "204": {"description": "Deleted."},
        "400": {
          "description": "Unexpected request body or query.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "405": {
          "description": "Unsupported method.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "413": {
          "description": "Request body too large.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "409": {
          "description": "Recording or file preparation is still active.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        },
        "410": {
          "description": "File no longer exists.",
          "content": {"application/json": {"schema": {"$ref": "#/paths/~1recording~1status/get/responses/400/content/application~1json/schema"}}}
        }
      }
    }
  }
}
)json";
}

#endif
