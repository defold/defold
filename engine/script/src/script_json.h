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

#ifndef DM_SCRIPT_JSON_H
#define DM_SCRIPT_JSON_H

extern "C"
{
#include <lua/lua.h>
}

namespace dmScript
{
    int DecodeJson(lua_State* L, const char* text, size_t size, int options, int protected_mode, char* error, size_t error_size);
    int EncodeJson(lua_State* L, int index, int options, char** json, size_t* length);
    void InitializeJson(lua_State* L);
}

#endif // DM_SCRIPT_JSON_H
