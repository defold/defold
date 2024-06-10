// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_SCRIPT_H
#define DMSDK_GAMESYS_SCRIPT_H

#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/script/script.h>
#include <dmsdk/gameobject/gameobject.h>
#include <dmsdk/resource/resource.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}


/*# SDK Script API documentation
 *
 * Built-in scripting functions.
 *
 * @document
 * @name Script
 * @namespace dmScript
 * @path engine/gamesys/src/dmsdk/gamesys/script.h
 */
namespace dmScript
{
    /*#
     * Get current game object instance
     * Works in both gameobjects and gui scripts
     *
     * @name CheckGOInstance
     * @param L [type: lua_State*] lua state
     * @return instance [type: dmGameObject::HInstance]
     */
    dmGameObject::HInstance CheckGOInstance(lua_State* L);

    /*#
     * Get gameobject instance
     *
     * The instance reference (url) at stack index "index" will be resolved to an instance.
     * @note The function only accepts instances in "this" collection. Otherwise a lua-error will be raised.
     *
     * @name CheckGOInstance
     * @param L [type: lua_State*] lua state
     * @param index [type: int] lua-arg
     * @return instance [type: dmGameObject::HInstance] gameobject instance
     *
     * @examples
     *
     * How to get the position of a gameobject in a script extension
     *
     * ```cpp
     * static int get_position(lua_State* L)
     * {
     *     DM_LUA_STACK_CHECK(L, 3);
     *     dmGameObject::HInstance instance = dmScript::CheckGOInstance(L, 1);
     *     dmVMath::Point3 position = dmGameObject::GetPosition(instance);
     *     lua_pushnumber(L, position.getX());
     *     lua_pushnumber(L, position.getY());
     *     lua_pushnumber(L, position.getZ());
     *     return 3;
     * }
     * ```
     */
    dmGameObject::HInstance CheckGOInstance(lua_State* L, int index);

    /*#
     * Get current gameobject's collection handle
     *
     * @note Works from both a .script/.gui_script
     *
     * @name CheckCollection
     * @param L [type: lua_State*] lua state
     * @param index [type: int] lua-arg
     * @return instance [type: lua_State*] gameobject instance
     */
    dmGameObject::HCollection CheckCollection(lua_State* L);

    /*#
     * Get component user data from a url.
     * @note The object referenced by the url must be in the same collection as the caller.
     *
     * @name GetComponentFromLua
     * @param L [type:lua_State*] Lua state
     * @param index [type:int] index to argument (a url)
     * @param component_type [type:const char*] E.g. "factoryc". The call will fail if the found component does not have the specified extension
     * @param world [type:dmGameObject::HComponentWorld*] The world associated owning the component. May be 0
     * @param component [type:dmGameObject::HComponent*] The component data associated with the url. May be 0
     * @param url [type:dmMessage::URL*] The resolved url. May be 0
     */
    void GetComponentFromLua(lua_State* L, int index, const char* component_type, dmGameObject::HComponentWorld* out_world, dmGameObject::HComponent* component, dmMessage::URL* url);

    /*# buffer ownership
     *
     * Buffer ownership.
     *  - OWNER_C   - m_Buffer is owned by C side, should not be destroyed when GCed
     *  - OWNER_LUA - m_Buffer is owned by Lua side, will be destroyed when GCed
     *  - OWNER_RES - m_Buffer not used, has a reference to a buffer resource instead. m_BufferRes is owned by C side, will be released when GCed
     * @enum
     * @name LuaBufferOwnership
     * @member dmScript::OWNER_C
     * @member dmScript::OWNER_LUA
     * @member dmScript::OWNER_RES
     *
     */
    enum LuaBufferOwnership
    {
        OWNER_C   = 0,
        OWNER_LUA = 1,
        OWNER_RES = 2,
    };

    /*# Lua wrapper for a dmBuffer::HBuffer
     *
     * Holds info about the buffer and who owns it.
     *
     * @struct
     * @name dmScript::LuaHBuffer
     * @member Union of
     *     - m_BufferRes [type:void*]                       A buffer resource
     *     - m_Buffer    [type:dmBuffer::HBuffer]           A buffer
     * @member m_Buffer [type:dmBuffer::HBuffer]            The buffer (or resource)
     * @member m_Owner  [type:dmScript::LuaBufferOwnership] What ownership the pointer has
     *
     * @examples
     *
     * See examples for dmScript::PushBuffer()
     *
     */
    struct LuaHBuffer
    {
        union
        {
            dmBuffer::HBuffer   m_Buffer;
            void*               m_BufferRes;
        };
        union
        {
            bool                m_UseLuaGC; // Deprecated
            LuaBufferOwnership  m_Owner;
        };

        dmhash_t m_BufferResPathHash;
        uint16_t m_BufferResVersion;

        LuaHBuffer();
        LuaHBuffer(dmBuffer::HBuffer buffer, LuaBufferOwnership ownership);
        LuaHBuffer(dmResource::HFactory factory, void* buffer_resource);
        // Deprecated
        LuaHBuffer(dmBuffer::HBuffer buffer, bool use_lua_gc);
    };

    /*# check if the value is a dmScript::LuaHBuffer
     *
     * Check if the value is a dmScript::LuaHBuffer
     *
     * @name dmScript::IsBuffer
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return boolean [type:boolean] True if value at index is a LuaHBuffer
     */
    bool IsBuffer(lua_State* L, int index);

    /*# push a LuaHBuffer onto the supplied lua state
     *
     * Will increase the stack by 1.
     *
     * @name dmScript::PushBuffer
     * @param L [type:lua_State*] lua state
     * @param buffer [type:dmScript::LuaHBuffer] buffer to push
     * @examples
     *
     * How to push a buffer and give Lua ownership of the buffer (GC)
     *
     * ```cpp
     * dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
     * PushBuffer(L, luabuf);
     * ```
     *
     * How to push a buffer and keep ownership in C++
     *
     * ```cpp
     * dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_C);
     * PushBuffer(L, luabuf);
     * ```
     */
    void PushBuffer(lua_State* L, const LuaHBuffer& buffer);

    /*# retrieve a LuaHBuffer from the supplied lua state
     * Retrieve a LuaHBuffer from the supplied lua state.
     * Check if the value in the supplied index on the lua stack is a LuaHBuffer and returns it.
     *
     * @name dmScript::CheckBuffer
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:LuaHBuffer*] pointer to dmScript::LuaHBuffer
     * @note The dmBuffer::IsBufferValid is already called on the returned buffer
     */
    LuaHBuffer* CheckBuffer(lua_State* L, int index);

    /*# retrieve a LuaHBuffer from the supplied lua state.
     * Retrieve a LuaHBuffer from the supplied lua state.
     * Check if the value in the supplied index on the lua stack is a LuaHBuffer and returns it.
     * @note Returns 0 on error. Does not invoke lua_error.
     * @name dmScript::CheckBufferNoError
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:LuaHBuffer*] pointer to dmScript::LuaHBuffer or 0 if not valid
     * @note The dmBuffer::IsBufferValid is already called on the returned buffer
     */
    LuaHBuffer* CheckBufferNoError(lua_State* L, int index);

    /*# retrieve a HBuffer from the supplied lua state
     * Retrieve a HBuffer from the supplied lua state
     *
     * Check if the value in the supplied index on the lua stack is a LuaHBuffer and it's valid, returns the HBuffer.
     *
     * @note The dmBuffer::IsBufferValid is already called on the returned buffer
     *
     * @name dmScript::CheckBufferUnpack
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:dmBuffer::HBuffer] buffer if valid, 0 otherwise
     */
    dmBuffer::HBuffer CheckBufferUnpack(lua_State* L, int index);

    /*# retrieve a HBuffer from the supplied lua state
     * Retrieve a HBuffer from the supplied lua state
     *
     * Check if the value in the supplied index on the lua stack is a LuaHBuffer and it's valid, returns the HBuffer.
     *
     * @note The dmBuffer::IsBufferValid is already called on the returned buffer
     *
     * @name dmScript::CheckBufferUnpackNoError
     * @param L [type:lua_State*] lua state
     * @param index [type:int] Index of the value
     * @return buffer [type:dmBuffer::HBuffer] buffer if valid, 0 otherwise
     */
    dmBuffer::HBuffer CheckBufferUnpackNoError(lua_State* L, int index);
}

#endif // DMSDK_GAMESYS_SCRIPT_H
