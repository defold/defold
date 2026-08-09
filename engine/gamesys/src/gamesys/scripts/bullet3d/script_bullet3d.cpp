// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <extension/extension.hpp>
#include <gameobject/script.h>
#include <script/script.h>

#include "components/comp_collision_object.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "gamesys.h"
#include "gamesys_private.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static float g_Bullet3DPhysicsScale = 1.0f;
    static float g_Bullet3DInvPhysicsScale = 1.0f;

    void         SetBullet3DPhysicsScale(float scale)
    {
        g_Bullet3DPhysicsScale = scale;
        g_Bullet3DInvPhysicsScale = 1.0f / scale;
    }

    float GetBullet3DPhysicsScale()
    {
        return g_Bullet3DPhysicsScale;
    }

    float GetBullet3DInvPhysicsScale()
    {
        return g_Bullet3DInvPhysicsScale;
    }

    btVector3 CheckBullet3DVector3(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* value = dmScript::CheckVector3(L, index);
        return btVector3(value->getX() * scale, value->getY() * scale, value->getZ() * scale);
    }

    btQuaternion CheckBullet3DQuat(lua_State* L, int index)
    {
        dmVMath::Quat* value = dmScript::CheckQuat(L, index);
        return btQuaternion(value->getX(), value->getY(), value->getZ(), value->getW());
    }

    void PushBullet3DVector3(lua_State* L, const btVector3& value, float scale)
    {
        dmScript::PushVector3(L, dmVMath::Vector3(value.getX() * scale, value.getY() * scale, value.getZ() * scale));
    }

    void PushBullet3DQuat(lua_State* L, const btQuaternion& value)
    {
        dmScript::PushQuat(L, dmVMath::Quat(value.getX(), value.getY(), value.getZ(), value.getW()));
    }

    static void GetCollisionObject(lua_State* L, int index, dmGameObject::HCollection collection, dmMessage::URL* url, dmGameObject::HComponent* component, void** component_world)
    {
        dmGameObject::GetComponentFromLua(L, index, collection, COLLISION_OBJECT_EXT, component, url, component_world);
    }

    static bool CheckBullet3DWorldBackend(lua_State* L, void* component_world)
    {
        if (!component_world)
        {
            return false;
        }

        if (GetPhysicsEngineType((CollisionWorld*)component_world) != PHYSICS_ENGINE_BULLET3D)
        {
            luaL_error(L, "bullet3d is only available when the active physics type is 3D.");
            return false;
        }
        return true;
    }

    static int Bullet3D_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        uint32_t                  component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        void*                     component_world = dmGameObject::GetWorld(collection, component_type_index);
        if (!component_world)
        {
            lua_pushnil(L);
            return 1;
        }

        if (!CheckBullet3DWorldBackend(L, component_world))
        {
            lua_pushnil(L);
            return 1;
        }

        void* world = CompCollisionObjectGetBullet3DWorld(component_world);
        if (world)
        {
            PushBullet3DWorld(L, world);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    static btCollisionObject* GetNativeCollisionObject(lua_State* L, dmGameObject::HCollection* out_collection, dmMessage::URL* out_url)
    {
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        dmGameObject::HComponent  component = 0;
        void*                     component_world = 0;
        GetCollisionObject(L, 1, collection, out_url, &component, &component_world);
        if (!CheckBullet3DWorldBackend(L, component_world))
        {
            return 0;
        }

        if (out_collection)
        {
            *out_collection = collection;
        }
        return (btCollisionObject*)CompCollisionObjectGetBullet3DCollisionObject(component);
    }

    static int Bullet3D_GetCollisionObject(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = 0;
        dmMessage::URL            url;
        btCollisionObject*        collision_object = GetNativeCollisionObject(L, &collection, &url);
        if (collision_object)
        {
            PushBullet3DCollisionObject(L, collision_object, collection, url.m_Path);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    static int Bullet3D_GetRigidBody(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = 0;
        dmMessage::URL            url;
        btCollisionObject*        collision_object = GetNativeCollisionObject(L, &collection, &url);
        if (collision_object && btRigidBody::upcast(collision_object))
        {
            PushBullet3DCollisionObject(L, collision_object, collection, url.m_Path);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    static int Bullet3D_GetVersion(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        int  version = btGetVersion();
        int  major = version / 100;
        int  minor = version % 100;
        char version_string[16];
        dmSnPrintf(version_string, sizeof(version_string), "%d.%02d", major, minor);

        lua_newtable(L);
        lua_pushstring(L, version_string);
        lua_setfield(L, -2, "version");
        lua_pushinteger(L, version);
        lua_setfield(L, -2, "number");
        lua_pushinteger(L, major);
        lua_setfield(L, -2, "major");
        lua_pushinteger(L, minor);
        lua_setfield(L, -2, "minor");
        return 1;
    }

    static const luaL_reg BULLET3D_FUNCTIONS[] = {
        { "get_world", Bullet3D_GetWorld },
        { "get_collision_object", Bullet3D_GetCollisionObject },
        { "get_rigid_body", Bullet3D_GetRigidBody },
        { "get_version", Bullet3D_GetVersion },
        { 0, 0 }
    };

    static dmExtension::Result ScriptBullet3DInitialize(dmExtension::Params* params)
    {
        float physics_scale = params->m_ConfigFile ? dmConfigFile::GetFloat(params->m_ConfigFile, "physics.scale", 1.0f) : 1.0f;
        SetBullet3DPhysicsScale(physics_scale);

        lua_State* L = params->m_L;
        luaL_register(L, "bullet3d", BULLET3D_FUNCTIONS);

        ScriptBullet3DInitializeWorld(L);
        ScriptBullet3DInitializeCollisionObject(L);
        ScriptBullet3DInitializeRigidBody(L);

        CompCollisionObjectSetBullet3DInvalidateWorldCallback(ScriptBullet3DInvalidateWorld);
        CompCollisionObjectSetBullet3DInvalidateCollisionObjectCallback(ScriptBullet3DInvalidateCollisionObject);

        lua_pop(L, 1);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptBullet3DFinalize(dmExtension::Params* params)
    {
        (void)params;
        CompCollisionObjectSetBullet3DInvalidateWorldCallback(0);
        CompCollisionObjectSetBullet3DInvalidateCollisionObjectCallback(0);
        ScriptBullet3DFinalizeRigidBody();
        ScriptBullet3DFinalizeCollisionObject();
        ScriptBullet3DFinalizeWorld();
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptBullet3DExt, "ScriptBullet3D", 0, 0, ScriptBullet3DInitialize, 0, 0, ScriptBullet3DFinalize)
} // namespace dmGameSystem

/*# Bullet 3D documentation
 *
 * Native-style access to the Bullet 3D world and collision objects owned by
 * Defold. World creation, destruction and stepping remain controlled by Defold.
 * The backend name refers to three-dimensional physics; this Defold version
 * bundles Bullet 2.77, reported by `bullet3d.get_version()`.
 *
 * @document
 * @name bullet3d
 * @namespace bullet3d
 * @language Lua
 */

/*# Bullet dynamics world
 * @typedef
 * @name btDiscreteDynamicsWorld
 * @param value [type:userdata]
 */

/*# Bullet collision object
 * @typedef
 * @name btCollisionObject
 * @param value [type:userdata]
 */

/*# Bullet rigid body
 *
 * Rigid bodies use the same Lua userdata representation as collision objects,
 * but rigid-body functions validate the native type before upcasting it.
 *
 * @typedef
 * @name btRigidBody
 * @param value [type:userdata]
 */

/*# Get the Bullet world for the current collection
 *
 * This function raises an error unless the collection uses 3D physics.
 *
 * @name bullet3d.get_world
 * @return world [type:btDiscreteDynamicsWorld] the world, or `nil` if the collection has no physics world
 */

/*# Get a Bullet collision object
 *
 * This returns both rigid bodies and ghost trigger objects.
 * This function raises an error unless the collection uses 3D physics.
 *
 * @name bullet3d.get_collision_object
 * @param url [type:string|hash|url] collision object component URL
 * @return object [type:btCollisionObject] the collision object, or `nil`
 */

/*# Get a Bullet rigid body
 *
 * Trigger components are ghost objects, so this function returns `nil` for them.
 * This function raises an error unless the collection uses 3D physics.
 *
 * @name bullet3d.get_rigid_body
 * @param url [type:string|hash|url] collision object component URL
 * @return body [type:btRigidBody] the rigid body handle, or `nil`
 */

/*# Get the bundled Bullet version
 * @name bullet3d.get_version
 * @return info [type:table] fields `version`, `number`, `major`, and `minor`
 */
