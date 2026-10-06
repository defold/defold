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

#ifndef DM_ENGINE_SERVICE_OPENAPI_H
#define DM_ENGINE_SERVICE_OPENAPI_H

namespace dmEngineService
{
    static const char PING_OPENAPI[] = R"json(
{
  "/ping": {
    "get": {
      "summary": "Check that the engine is responding",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "PONG\n"
            }
          }
        }
      }
    }
  }
}
)json";

    static const char INFO_OPENAPI[] = R"json(
{
  "/info": {
    "get": {
      "summary": "Get engine version, platform and log port",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": [
                  "version",
                  "platform",
                  "sha1",
                  "log_port"
                ],
                "properties": {
                  "version": {
                    "type": "string"
                  },
                  "platform": {
                    "type": "string"
                  },
                  "sha1": {
                    "type": "string"
                  },
                  "log_port": {
                    "type": "string",
                    "description": "Port used by the TCP log service."
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char STATE_OPENAPI[] = R"json(
{
  "/state": {
    "get": {
      "summary": "Get engine connection state",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": [
                  "connection_mode"
                ],
                "properties": {
                  "connection_mode": {
                    "type": "boolean",
                    "description": "True when the engine is waiting for a project connection."
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char OPENAPI_OPENAPI[] = R"json(
{
  "/openapi.json": {
    "get": {
      "summary": "Discover the engine HTTP API",
      "responses": {
        "200": {
          "description": "Includes all currently registered handlers, including native extensions.",
          "content": {
            "application/json": {
              "schema": {
                "type": "object"
              }
            }
          }
        },
        "405": {
          "description": "Method Not Allowed",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char PROFILE_OPENAPI[] = R"json(
{
  "/": {
    "get": {
      "summary": "Open the engine profiler",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "text/html": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char REDIRECT_OPENAPI[] = R"json(
{
  "/": {
    "get": {
      "summary": "Redirect to the engine service",
      "responses": {
        "302": {
          "description": "Preserves the requested path.",
          "headers": {
            "Location": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char RESOURCES_OPENAPI[] = R"json(
{
  "/resources_data": {
    "get": {
      "summary": "Get loaded resource statistics",
      "responses": {
        "200": {
          "description": "Uses the engine platform byte order. Strings have a uint16 byte length followed by UTF-8 bytes. Starts with the string RESS, followed by records containing name, extension, uint32 memory size, uint32 disk size and uint32 reference count.",
          "content": {
            "application/octet-stream": {
              "schema": {
                "type": "string",
                "format": "binary"
              }
            }
          }
        },
        "500": {
          "description": "Profiler state is unavailable",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char GAMEOBJECTS_OPENAPI[] = R"json(
{
  "/gameobjects_data": {
    "get": {
      "summary": "Get the game-object hierarchy",
      "responses": {
        "200": {
          "description": "Uses the engine platform byte order. Strings have a uint16 byte length followed by UTF-8 bytes. Starts with the string GOBJ, followed by records containing identifier, resource identifier, type, uint32 index and uint32 parent index.",
          "content": {
            "application/octet-stream": {
              "schema": {
                "type": "string",
                "format": "binary"
              }
            }
          }
        },
        "500": {
          "description": "Profiler state is unavailable",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char SCENE_GRAPH_OPENAPI[] = R"json(
{
  "/scene_graph": {
    "get": {
      "summary": "Get the runtime scene graph",
      "responses": {
        "200": {
          "description": "OK",
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": [
                  "children"
                ],
                "properties": {
                  "children": {
                    "type": "array",
                    "items": {
                      "$ref": "#/paths/~1scene_graph/get/responses/200/content/application~1json/schema"
                    }
                  }
                },
                "additionalProperties": true,
                "description": "Node properties depend on the component type. Values may be strings, numbers or numeric arrays; boolean properties are encoded as 0 or 1."
              }
            }
          }
        },
        "500": {
          "description": "Profiler state is unavailable",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      }
    }
  }
}
)json";

    static const char POST_OPENAPI[] = R"json(
{
  "/post/{socket}/{message_type}": {
    "post": {
      "summary": "Post an engine message",
      "description": "Encode the payload as protobuf using the registered DDF descriptor. The receiving socket determines which message types it handles.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "parameters": [
        {
          "name": "socket",
          "in": "path",
          "required": true,
          "schema": {
            "type": "string"
          },
          "example": "@system"
        },
        {
          "name": "message_type",
          "in": "path",
          "required": true,
          "schema": {
            "type": "string"
          },
          "example": "reboot"
        }
      ]
    }
  },
  "/post/@system/reboot": {
    "post": {
      "summary": "Reboot the engine",
      "description": "Optional string fields arg1 through arg6 supply startup arguments.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "x-defold-protobuf-message": "dmSystemDDF.Reboot",
      "externalDocs": {
        "url": "https://github.com/defold/defold/blob/dev/engine/script/src/script/sys_ddf.proto"
      }
    }
  },
  "/post/@system/exit": {
    "post": {
      "summary": "Exit the engine",
      "description": "Required int32 field code supplies the exit code. Example protobuf bytes for code=0: 08 00.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "x-defold-protobuf-message": "dmSystemDDF.Exit",
      "externalDocs": {
        "url": "https://github.com/defold/defold/blob/dev/engine/script/src/script/sys_ddf.proto"
      }
    }
  },
  "/post/@system/run_script": {
    "post": {
      "summary": "Run a compiled Lua module",
      "description": "Required field module is a dmLuaDDF.LuaModule.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "x-defold-protobuf-message": "dmEngineDDF.RunScript",
      "externalDocs": {
        "url": "https://github.com/defold/defold/blob/dev/engine/engine/proto/engine/engine_ddf.proto"
      }
    }
  },
  "/post/@render/resize": {
    "post": {
      "summary": "Resize the engine window",
      "description": "Required uint32 fields width and height specify the window size in pixels.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "x-defold-protobuf-message": "dmRenderDDF.Resize",
      "externalDocs": {
        "url": "https://github.com/defold/defold/blob/dev/engine/render/proto/render/render_ddf.proto"
      }
    }
  },
  "/post/@resource/reload": {
    "post": {
      "summary": "Reload engine resources",
      "description": "Repeated string field resources supplies project resource paths, for example /main/main.scriptc.",
      "requestBody": {
        "required": true,
        "content": {
          "application/octet-stream": {
            "schema": {
              "type": "string",
              "format": "binary",
              "maxLength": 1024
            }
          }
        }
      },
      "responses": {
        "200": {
          "description": "Delivery is asynchronous. This response does not confirm command execution and may also be returned if protobuf decoding or message posting fails.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              },
              "example": "OK"
            }
          }
        },
        "400": {
          "description": "Socket lookup, DDF descriptor lookup, body size validation, or body reading failed.",
          "content": {
            "text/plain": {
              "schema": {
                "type": "string"
              }
            }
          }
        }
      },
      "x-defold-protobuf-message": "dmResourceDDF.Reload",
      "externalDocs": {
        "url": "https://github.com/defold/defold/blob/dev/engine/resource/proto/resource/resource_ddf.proto"
      }
    }
  }
}
)json";

}

#endif
