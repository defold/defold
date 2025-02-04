// Copyright 2020-2025 The Defold Foundation
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

#include <stdio.h>

#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/Joints/b2Joint.h>

#include <dlib/log.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "gamesys_private.h"

#include "components/box2d/comp_collision_object_box2d.h"

#include <extension/extension.hpp>

#include "script_box2d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    //////////////////////////////////////////////////////////////////////////////

    static float g_PhysicsScale = 1.0f;
    static float g_InvPhysicsScale = 1.0f / g_PhysicsScale;

    //////////////////////////////////////////////////////////////////////////////
    // b2Vec2

    b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        return b2Vec2(v->getX() * scale, v->getY() * scale);
    }

    dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale)
    {
        return dmVMath::Vector3(p.x * inv_scale, p.y * inv_scale, 0);
    }

    void PushWorld(struct lua_State* L, class b2World* world)
    {
        lua_pushlightuserdata(L, world);
    }

    void SetPhysicsScale(float scale)
    {
        g_PhysicsScale = scale;
        g_InvPhysicsScale = 1.0f / g_PhysicsScale;
    }

    float GetPhysicsScale()
    {
        return g_PhysicsScale;
    }

    float GetInvPhysicsScale()
    {
        return g_InvPhysicsScale;
    }

    //////////////////////////////////////////////////////////////////////////////

    static void GetCollisionObject(lua_State* L, int index, dmGameObject::HCollection collection, dmMessage::URL* url, dmGameObject::HComponent* comp, void** comp_world)
    {
        dmGameObject::GetComponentFromLua(L, index, collection, COLLISION_OBJECT_EXT, comp, url, comp_world);
    }

    static int B2D_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        void* comp_world = dmGameObject::GetWorld(collection, component_type_index);
        b2World* world = dmGameSystem::CompCollisionObjectGetBox2DWorld(comp_world);

        if (world)
            PushWorld(L, world);
        else
            lua_pushnil(L);
        return 1;
    }

    static int B2D_GetBody(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        dmMessage::URL url;
        dmGameObject::HComponent component = 0;
        GetCollisionObject(L, 1, collection, &url, &component, 0);

        b2Body* body = dmGameSystem::CompCollisionObjectGetBox2DBody(component);

        if (body)
            PushBody(L, body, collection, url.m_Path);
        else
            lua_pushnil(L);
        return 1;
    }

    static const luaL_reg BOX2D_FUNCTIONS[] =
    {
        {"get_world", B2D_GetWorld},
        {"get_body", B2D_GetBody},

        {0, 0}
    };

    static dmExtension::Result ScriptBox2DInitialize(dmExtension::Params* params)
    {
        float physics_scale_default = 1.0f;
        float physics_scale = params->m_ConfigFile ? dmConfigFile::GetFloat(params->m_ConfigFile, "physics.scale", physics_scale_default) : physics_scale_default;
        dmGameSystem::SetPhysicsScale(physics_scale);

        lua_State* L = params->m_L;
        luaL_register(L, "b2d", dmGameSystem::BOX2D_FUNCTIONS);

            dmGameSystem::ScriptBox2DInitializeBody(L);

        lua_pop(L, 1); // pop the lua module
        return dmExtension::RESULT_OK;
    }


    static dmExtension::Result ScriptBox2DFinalize(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }


    DM_DECLARE_EXTENSION(ScriptBox2DExt, "ScriptBox2d", 0, 0, ScriptBox2DInitialize, 0, 0, ScriptBox2DFinalize)

}

/*# Box2D documentation
 *
 * Functions for interacting with Box2D.
 *
 * @document
 * @name b2d
 * @namespace b2d
 */

/*# Get the Box2D world from the current collection
 * @name b2d.get_world
 * @return world [type: b2World] the world if successful. Otherwise `nil`.
 */

/*# Get the Box2D body from a collision object
 * @name b2d.get_body
 * @param url [type: string|hash|url] the url to the game object collision component
 * @return body [type: b2Body] the body if successful. Otherwise `nil`.
 */

