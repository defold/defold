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

#ifndef DM_WEBSERVER_OPENAPI_H
#define DM_WEBSERVER_OPENAPI_H

#include "array.h"
#include "hash.h"
#include "set.h"

namespace dmWebServer
{
    struct OpenAPIPath
    {
        dmhash_t m_Hash;
        uint32_t m_Offset;
        uint32_t m_Length;
    };

    struct OpenAPI
    {
        const char*          m_Json;
        uint32_t             m_Length;
        dmSet<dmhash_t>       m_Paths;
        dmArray<OpenAPIPath> m_PathInfo;
    };

    struct OpenAPIError
    {
        const char* m_Message;
        uint32_t    m_Offset;
    };

    // Validate JSON and the Paths/Path Item/Operation structure. Full schema
    // validation belongs in tooling, rather than in the engine's HTTP server.
    bool ParseOpenAPI(const char* prefix, const char* json, OpenAPI* openapi, OpenAPIError* error = 0);

    // Look up the original JSON key only when reporting a conflicting path.
    const OpenAPIPath* FindOpenAPIPath(const OpenAPI* openapi, dmhash_t hash);
}

#endif
