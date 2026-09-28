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

#ifndef DM_GUI_SCRIPT_H
#define DM_GUI_SCRIPT_H

#include <script/script.h>

extern "C"
{
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

namespace dmGui
{
    lua_State* InitializeScript(dmScript::HContext script_context);
    void FinalizeScript(lua_State* L, dmScript::HContext script_context);

    // Read a live node's subtype name without invoking Lua or changing the stack.
    // Returns false for non-nodes, deleted nodes, and nodes outside the current scene.
    bool GetNodeTypeName(lua_State* L, int index, char* buffer, uint32_t buffer_size);
}

#endif
