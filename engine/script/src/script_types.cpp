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

#include <extension/extension.hpp>
#include "script.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#define LIB_NAME "types"

namespace dmScript
{
    /*# Types API documentation
     *
     * Functions for checking Defold userdata types.
     *
     * @document
     * @name Types
     * @namespace types
     * @language Lua
     */

    /*# Check if passed type is vector.
    *
    * @name types.is_vector
    * @param var [type:any] Variable to check type
    * 
    * @note 'vector3' and 'vector4' types is not a 'vector' type under the hood. 
    * So if called `types.is_vector(vmath.vector3())` it returns 'false'
    *
    * @return result [type:boolean] True if passed type is vector
    */
    static int Types_IsVector(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsVector(L, 1));
        return 1;
    }

    /*# Check if passed type is matrix4.
    *
    * @name types.is_matrix4
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is matrix4
    */
    static int Types_IsMatrix4(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsMatrix4(L, 1));
        return 1;
    }

    /*# Check if passed type is vector3.
    *
    * @name types.is_vector3
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is vector3
    */
    static int Types_IsVector3(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsVector3(L, 1));
        return 1;
    }

    /*# Check if passed type is vector4.
    *
    * @name types.is_vector4
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is vector4
    */
    static int Types_IsVector4(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsVector4(L, 1));
        return 1;
    }

    /*# Check if passed type is quaternion.
    *
    * @name types.is_quat
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is quaternion
    */
    static int Types_IsQuat(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsQuat(L, 1));
        return 1;
    }

    /*# Check if passed type is hash.
    *
    * @name types.is_hash
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is hash
    */
    static int Types_IsHash(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsHash(L, 1));
        return 1;
    }

    /*# Check if passed type is URL.
    *
    * @name types.is_url
    * @param var [type:any] Variable to check type
    *
    * @return result [type:boolean] True if passed type is URL
    */
    static int Types_IsUrl(lua_State* L)
    {
        lua_pushboolean(L, dmScript::IsURL(L, 1));
        return 1;
    }

    static const luaL_reg Types_methods[] =
    {
        { "is_vector", Types_IsVector },
        { "is_matrix4", Types_IsMatrix4 },
        { "is_vector3", Types_IsVector3 },
        { "is_vector4", Types_IsVector4 },
        { "is_quat", Types_IsQuat },
        { "is_hash", Types_IsHash },
        { "is_url", Types_IsUrl },
        {0, 0}
    };

    static dmExtension::Result ScriptTypesInitialize(dmExtension::Params* params)
    {
        lua_State* L = params->m_L;
        int top = lua_gettop(L);
        luaL_register(L, LIB_NAME, Types_methods);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptTypesFinalize(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }
}

DM_DECLARE_EXTENSION(ScriptTypesExt, "ScriptTypes", 0, 0, dmScript::ScriptTypesInitialize, 0, 0, dmScript::ScriptTypesFinalize)
