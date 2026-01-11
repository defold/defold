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

#ifndef DM_GAMEOBJECT_RES_LUA_H
#define DM_GAMEOBJECT_RES_LUA_H

#include <dmsdk/gameobject/res_lua.h>

namespace dmGameObject
{
    /**
     * Apply a delta/diff with changes to a sequence of bytes. The delta is in
     * the following format:
     * 
     * * index - The index where to apply the next change. 1-4 bytes, depending on size of bytes
     *           If the bytes are fewer than 256 then the index is represented by a single byte
     *           If the bytes are fewer than 65536 then the index is represented by two bytes
     * * count - The number of consecutive bytes to alter. 1 byte (ie max 255 changes)
     * * bytes - The new byte values to set
     * 
     * @param bytes The bytes to apply delta to
     * @param bytes_size The size of the bytes
     * @param delta The delta to apply
     * @param delta_size The size of the delta
     */
    void PatchBytes(uint8_t* bytes, uint32_t bytes_size, const uint8_t* delta, uint32_t delta_size);

    /**
     * Patch Lua 64-bit bytecode with the changes required to transform the
     * bytecode to a 32-bit equivalent.
     */
    void PatchLuaBytecode(dmLuaDDF::LuaSource *source);
}

#endif // DM_GAMEOBJECT_RES_LUA_H
