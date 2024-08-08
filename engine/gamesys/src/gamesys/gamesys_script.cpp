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

#include "gamesys.h"

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <physics/physics.h>

#include <gamesys/gamesys_ddf.h>
#include "gamesys.h"
#include "gamesys_private.h"

#include "scripts/script_label.h"
#include "scripts/script_particlefx.h"
#include "scripts/script_tilemap.h"
#include "scripts/script_physics.h"
#include "scripts/script_sound.h"
#include "scripts/script_sprite.h"
#include "scripts/script_factory.h"
#include "scripts/script_collection_factory.h"
#include "scripts/script_resource.h"
#include "scripts/script_window.h"
#include "scripts/script_collectionproxy.h"
#include "scripts/script_buffer.h"
#include "scripts/script_sys_gamesys.h"
#include "scripts/script_camera.h"
#include "scripts/script_http.h"
#include "scripts/script_material.h"
#include "scripts/script_compute.h"

#include "components/comp_gui.h"

#include <dmsdk/gamesys/script.h>
#include <gameobject/script.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript {
    static inline dmGameObject::HInstance GetGOInstance(lua_State* L)
    {
        dmGameObject::HInstance instance = dmGameObject::GetInstanceFromLua(L);
        if (instance == 0) {
            dmGui::HScene scene = dmGui::GetSceneFromLua(L);
            if (scene != 0) {
                instance = (dmGameObject::HInstance)dmGameSystem::GuiGetUserDataCallback(scene);
            }
        }
        return instance;
    }

    dmGameObject::HInstance CheckGOInstance(lua_State* L) {
        dmGameObject::HInstance instance = GetGOInstance(L);
        // No instance for render scripts, ignored
        if (instance == 0) {
            luaL_error(L, "no instance could be found in the current script environment");
        }
        return instance;
    }

    // Inspired by the internal function dmGameObject::ResolveInstance
    // Modified to support both gameobject/gui scripts
    dmGameObject::HInstance CheckGOInstance(lua_State* L, int instance_arg)
    {
        dmGameObject::HInstance instance = GetGOInstance(L);

        if (!lua_isnil(L, instance_arg)) {
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

            dmMessage::URL receiver;
            dmScript::ResolveURL(L, instance_arg, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(collection))
            {
                luaL_error(L, "function called can only access instances within the same collection.");
            }

            instance = dmGameObject::GetInstanceFromIdentifier(collection, receiver.m_Path);
            if (!instance)
            {
                luaL_error(L, "Instance %s not found", lua_tostring(L, instance_arg));
                return 0; // Actually never reached
            }
        }
        return instance;
    }

    dmGameObject::HCollection CheckCollection(lua_State* L)
    {
        dmGameObject::HInstance instance = GetGOInstance(L);
        if (!instance)
            luaL_error(L, "Script context doesn't have a game object set");
        return instance ? dmGameObject::GetCollection(instance) : 0;
    }

    void GetComponentFromLua(lua_State* L, int index, const char* component_type, dmGameObject::HComponentWorld* out_world, dmGameObject::HComponent* component, dmMessage::URL* url)
    {
        dmGameObject::HInstance instance = CheckGOInstance(L, index);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
        dmGameObject::GetComponentFromLua(L, index, collection, component_type, component, url, out_world);
    }
}


namespace dmGameSystem
{
    int ReportPathError(lua_State* L, dmResource::Result result, dmhash_t path_hash)
    {
        char msg[256];
        const char* format = 0;
        switch(result)
        {
            case dmResource::RESULT_RESOURCE_NOT_FOUND:
                format = "The resource was not found (%d): %llu, %s";
                break;
            case dmResource::RESULT_NOT_SUPPORTED:
                format = "The resource type does not support this operation (%d): %llu, %s";
                break;
            default:
                format = "The resource was not updated (%d): %llu, %s";
                break;
        }
        dmSnPrintf(msg, sizeof(msg), format, result, (unsigned long long)path_hash, dmHashReverseSafe64(path_hash));
        return luaL_error(L, "%s", msg);
    }

    void* CheckResource(lua_State* L, dmResource::HFactory factory, dmhash_t path_hash, const char* resource_ext)
    {
        if (path_hash == 0)
        {
            luaL_error(L, "Could not get %s type resource from zero path hash", resource_ext);
            return 0;
        }

        HResourceDescriptor rd = dmResource::FindByHash(factory, path_hash);
        if (!rd)
        {
            luaL_error(L, "Could not get %s type resource: %s", resource_ext, dmHashReverseSafe64(path_hash));
            return 0;
        }

        HResourceType expected_resource_type;
        dmResource::Result r = dmResource::GetTypeFromExtension(factory, resource_ext, &expected_resource_type);
        if( r != dmResource::RESULT_OK )
        {
            ReportPathError(L, r, path_hash);
        }

        HResourceType resource_type = dmResource::GetType(rd);
        if (resource_type != expected_resource_type)
        {
            luaL_error(L, "Resource %s is not of type %s.", dmHashReverseSafe64(path_hash), resource_ext);
            return 0;
        }

        return dmResource::GetResource(rd);
    }

    void PushTextureInfo(lua_State* L, dmGraphics::HTexture texture_handle, dmhash_t texture_resource_path)
    {
        uint32_t texture_width               = dmGraphics::GetTextureWidth(texture_handle);
        uint32_t texture_height              = dmGraphics::GetTextureHeight(texture_handle);
        uint32_t texture_depth               = dmGraphics::GetTextureDepth(texture_handle);
        uint32_t texture_mipmaps             = dmGraphics::GetTextureMipmapCount(texture_handle);
        dmGraphics::TextureType texture_type = dmGraphics::GetTextureType(texture_handle);
        uint32_t texture_flags               = dmGraphics::GetTextureUsageHintFlags(texture_handle);

        if (texture_resource_path != 0)
        {
            dmScript::PushHash(L, texture_resource_path);
            lua_setfield(L, -2, "path");
        }

        lua_pushnumber(L, texture_handle);
        lua_setfield(L, -2, "handle");

        lua_pushinteger(L, texture_width);
        lua_setfield(L, -2, "width");

        lua_pushinteger(L, texture_height);
        lua_setfield(L, -2, "height");

        lua_pushinteger(L, texture_depth);
        lua_setfield(L, -2, "depth");

        lua_pushinteger(L, texture_mipmaps);
        lua_setfield(L, -2, "mipmaps");

        lua_pushinteger(L, texture_type);
        lua_setfield(L, -2, "type");

        lua_pushinteger(L, texture_flags);
        lua_setfield(L, -2, "flags");

        // JG: We should probably expose format as well.
    }

    void PushSampler(lua_State* L, dmRender::HSampler sampler)
    {
        dmhash_t name_hash;
        dmGraphics::TextureType texture_type;
        uint32_t location;
        dmGraphics::TextureWrap u_wrap;
        dmGraphics::TextureWrap v_wrap;
        dmGraphics::TextureFilter min_filter;
        dmGraphics::TextureFilter mag_filter;
        float max_anisotropy;
        dmRender::GetSamplerInfo(sampler, &name_hash, &texture_type, &location, &u_wrap, &v_wrap, &min_filter, &mag_filter, &max_anisotropy);

        dmScript::PushHash(L, name_hash);
        lua_setfield(L, -2, "name");

        lua_pushinteger(L, (lua_Integer) texture_type);
        lua_setfield(L, -2, "type");

        lua_pushinteger(L, (lua_Integer) u_wrap);
        lua_setfield(L, -2, "u_wrap");

        lua_pushinteger(L, (lua_Integer) v_wrap);
        lua_setfield(L, -2, "v_wrap");

        lua_pushinteger(L, (lua_Integer) min_filter);
        lua_setfield(L, -2, "min_filter");

        lua_pushinteger(L, (lua_Integer) mag_filter);
        lua_setfield(L, -2, "mag_filter");

        lua_pushnumber(L, max_anisotropy);
        lua_setfield(L, -2, "max_anisotropy");
    }

    void PushRenderConstant(lua_State* L, dmRender::HConstant constant)
    {
        dmhash_t name_hash = dmRender::GetConstantName(constant);

        dmScript::PushHash(L, name_hash);
        lua_setfield(L, -2, "name");

        dmRenderDDF::MaterialDesc::ConstantType type = dmRender::GetConstantType(constant);
        lua_pushinteger(L, (lua_Integer) type);
        lua_setfield(L, -2, "type");

        if (type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
        {
            uint32_t num_values;
            dmVMath::Vector4* values   = dmRender::GetConstantValues(constant, &num_values);
            dmVMath::Matrix4* matrices = (dmVMath::Matrix4*) values;
            uint32_t num_matrices      = num_values / 4;

            if (num_matrices > 1)
            {
                lua_newtable(L);
                for (uint32_t i = 0; i < num_matrices; ++i)
                {
                    dmScript::PushMatrix4(L, matrices[i]);
                    lua_rawseti(L, -2, i + 1);
                }
            }
            else
            {
                dmScript::PushMatrix4(L, matrices[0]);
            }

            lua_setfield(L, -2, "value");
        }
        else if (type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER)
        {
            uint32_t num_values;
            dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);

            if (num_values > 1)
            {
                lua_newtable(L);
                for (uint32_t i = 0; i < num_values; ++i)
                {
                    dmScript::PushVector4(L, values[i]);
                    lua_rawseti(L, -2, i + 1);
                }
            }
            else
            {
                dmScript::PushVector4(L, values[0]);
            }

            lua_setfield(L, -2, "value");
        }
        // Other constant types doesn't have a value
    }

    void PushVertexAttribute(lua_State* L, const dmGraphics::VertexAttribute* attribute, const uint8_t* value_ptr)
    {
        float values[4] = {};
        VertexAttributeToFloats(attribute, value_ptr, values);

        if (attribute->m_ElementCount == 4)
        {
            dmVMath::Vector4 v(values[0], values[1], values[2], values[3]);
            dmScript::PushVector4(L, v);
        }
        else if (attribute->m_ElementCount == 3 || attribute->m_ElementCount == 2)
        {
            dmVMath::Vector3 v(values[0], values[1], values[2]);
            dmScript::PushVector3(L, v);
        }
        else if (attribute->m_ElementCount == 1)
        {
            lua_pushnumber(L, values[0]);
        }
        else
        {
            // We need to catch this error because it means there are type combinations
            // that we have added, but don't have support for here.
            assert("Not supported!");
        }
    }

    void GetSamplerParametersFromLua(lua_State* L, dmGraphics::TextureWrap* u_wrap, dmGraphics::TextureWrap* v_wrap, dmGraphics::TextureFilter* min_filter, dmGraphics::TextureFilter* mag_filter, float* max_anisotropy)
    {
        // parse u_wrap
        {
            lua_getfield(L, -1, "u_wrap");
            if (!lua_isnil(L, -1))
            {
                *u_wrap = (dmGraphics::TextureWrap) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse v_wrap
        {
            lua_getfield(L, -1, "v_wrap");
            if (!lua_isnil(L, -1))
            {
                *v_wrap = (dmGraphics::TextureWrap) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse min_filter
        {
            lua_getfield(L, -1, "min_filter");
            if (!lua_isnil(L, -1))
            {
                *min_filter = (dmGraphics::TextureFilter) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse mag_filter
        {
            lua_getfield(L, -1, "mag_filter");
            if (!lua_isnil(L, -1))
            {
                *mag_filter = (dmGraphics::TextureFilter) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse max_anisotropy
        {
            lua_getfield(L, -1, "max_anisotropy");
            if (!lua_isnil(L, -1))
            {
                *max_anisotropy = lua_tonumber(L, -1);
            }
            lua_pop(L, 1);
        }
    }

    static dmVMath::Vector4* FillConstantsFromLua(lua_State* L, int index, dmVMath::Vector4* v4_in)
    {
        if (dmScript::IsVector4(L, index))
        {
            dmVMath::Vector4* v4 = dmScript::CheckVector4(L, index);
            *v4_in = *v4;
            v4_in++;
        }
        else if (dmScript::IsVector3(L, index))
        {
            dmVMath::Vector3* v3 = dmScript::CheckVector3(L, index);
            v4_in->setXYZ(*v3);
            v4_in++;
        }
        else if (dmScript::IsMatrix4(L, index))
        {
            dmVMath::Matrix4* m4 = dmScript::CheckMatrix4(L, index);
            memcpy(v4_in, m4, sizeof(dmVMath::Vector4) * 4);
            v4_in += 4;
        }
        else
        {
            float value = luaL_checknumber(L, index);
            dmVMath::Vector4 v4;
            v4.setX(value);
            v4_in++;
        }
        return v4_in;
    }

    void GetConstantValuesFromLua(lua_State* L, dmRenderDDF::MaterialDesc::ConstantType* type, dmArray<dmVMath::Vector4>* scratch_values)
    {
        // parse type
        {
            lua_getfield(L, -1, "type");
            if (!lua_isnil(L, -1))
            {
                *type = (dmRenderDDF::MaterialDesc::ConstantType) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse value
        {
            lua_getfield(L, -1, "value");
            if (!lua_isnil(L, -1))
            {
                if (lua_istable(L, -1))
                {
                    uint32_t count = 0;

                    // Count number of values. We can't use lua_objlen here because a matrix types
                    // equals four value slots, so we need to know exactly what is in the table.
                    lua_pushvalue(L, -1);
                    lua_pushnil(L);
                    while (lua_next(L, -2) != 0)
                    {
                        if (dmScript::IsVector4(L, -1) || dmScript::IsVector3(L, -1) || lua_isnumber(L, -1))
                        {
                            count++;
                        }
                        else if (dmScript::IsMatrix4(L, -1))
                        {
                            count += 4;
                        }
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1);

                    if (scratch_values->Capacity() < count)
                    {
                        scratch_values->SetCapacity(count);
                    }
                    scratch_values->SetSize(count);

                    dmVMath::Vector4* write_ptr = scratch_values->Begin();

                    lua_pushvalue(L, -1);
                    lua_pushnil(L);
                    while (lua_next(L, -2) != 0)
                    {
                        write_ptr = FillConstantsFromLua(L, -1, write_ptr);
                        lua_pop(L, 1);
                    }
                    lua_pop(L, 1);
                }
                else
                {
                    uint32_t count = 1;
                    if (dmScript::IsMatrix4(L, -1))
                    {
                        count = 4;
                    }

                    if (scratch_values->Capacity() < count)
                    {
                        scratch_values->SetCapacity(count);
                    }

                    scratch_values->SetSize(count);

                    FillConstantsFromLua(L, -1, scratch_values->Begin());
                }
            }
            lua_pop(L, 1);
        }
    }

    ScriptLibContext::ScriptLibContext()
    {
        memset(this, 0, sizeof(*this));
    }

    bool InitializeScriptLibs(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;

        int top = lua_gettop(L);
        (void)top;

        bool result = true;

        ScriptBufferRegister(context);
        ScriptCameraRegister(context);
        ScriptLabelRegister(context);
        ScriptParticleFXRegister(context);
        ScriptTileMapRegister(context);
        ScriptPhysicsRegister(context);
        ScriptFactoryRegister(context);
        ScriptCollectionFactoryRegister(context);
        ScriptSpriteRegister(context);
        ScriptSoundRegister(context);
        ScriptResourceRegister(context);
        ScriptWindowRegister(context);
        ScriptCollectionProxyRegister(context);
        ScriptSysGameSysRegister(context);
        ScriptHttpRegister(context);
        ScriptMaterialRegister(context);
        ScriptComputeRegister(context);

        assert(top == lua_gettop(L));
        return result;
    }

    void FinalizeScriptLibs(const ScriptLibContext& context)
    {
        ScriptCollectionProxyFinalize(context);
        ScriptLabelFinalize(context);
        ScriptPhysicsFinalize(context);
        ScriptResourceFinalize(context);
        ScriptWindowFinalize(context);
        ScriptSysGameSysFinalize(context);
        ScriptHttpFinalize(context);
    }

    void UpdateScriptLibs(const ScriptLibContext& context)
    {
        ScriptSysGameSysUpdate(context);
    }

    dmGameObject::HInstance CheckGoInstance(lua_State* L) {
        return dmScript::CheckGOInstance(L);
    }


    void OnWindowFocus(bool focus)
    {
        ScriptWindowOnWindowFocus(focus);
        // We need to call ScriptWindowOnWindowFocus before ScriptSoundOnWindowFocus to
        // allow the is_music_playing() script function to return the correct result.
        // When the window activation is received the application sound is not yet playing
        // any sounds and the Android platform function will return the correct result.
        // Once ScriptSoundOnWindowFocus is called when focus is gained we always say
        // that background music is off if the game is playing music and the app has focus
        ScriptSoundOnWindowFocus(focus);
    }

    void OnWindowIconify(bool iconify)
    {
        ScriptWindowOnWindowIconify(iconify);
    }

    void OnWindowResized(int width, int height)
    {
        ScriptWindowOnWindowResized(width, height);
    }

    void OnWindowCreated(int width, int height)
    {
        ScriptWindowOnWindowCreated(width, height);
    }
}
