// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <stdint.h>

#include <dlib/opaque_handle_container.h>
#include <dmsdk/dlib/hashtable.h>
#include <script/script.h>

#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
#define BULLET3D_TYPE_NAME_WORLD "bullet3d_world"

    struct Bullet3DLuaWorld
    {
        HOpaqueHandle m_Handle;
        void*         m_ComponentWorld;
    };

    static uint32_t                           g_Bullet3DWorldTypeHash = 0;
    static dmOpaqueHandleContainer<uintptr_t> g_Bullet3DWorldHandles;
    static dmHashTable64<HOpaqueHandle>       g_Bullet3DWorldToHandle;

    static uint64_t                           WorldPtrToKey(const btDiscreteDynamicsWorld* world)
    {
        return (uint64_t)(uintptr_t)world;
    }

    static void EnsureWorldHandleCapacity()
    {
        if (g_Bullet3DWorldHandles.Full())
        {
            g_Bullet3DWorldHandles.Allocate(16);
            g_Bullet3DWorldToHandle.OffsetCapacity(16);
        }
    }

    static void ClearWorldHandles()
    {
        for (uint32_t i = 0; i < g_Bullet3DWorldHandles.Capacity(); ++i)
        {
            if (g_Bullet3DWorldHandles.GetByIndex(i))
            {
                g_Bullet3DWorldHandles.Release(g_Bullet3DWorldHandles.IndexToHandle(i));
            }
        }
    }

    static void InvalidateWorldHandle(HOpaqueHandle handle)
    {
        uintptr_t* world_ptr = g_Bullet3DWorldHandles.Get(handle);
        if (!world_ptr)
        {
            return;
        }

        uint64_t       key = WorldPtrToKey((btDiscreteDynamicsWorld*)world_ptr);
        HOpaqueHandle* mapped_handle = g_Bullet3DWorldToHandle.Get(key);
        if (mapped_handle && *mapped_handle == handle)
        {
            g_Bullet3DWorldToHandle.Erase(key);
        }
        g_Bullet3DWorldHandles.Release(handle);
    }

    static Bullet3DLuaWorld* CheckWorldUserdata(lua_State* L, int index)
    {
        return (Bullet3DLuaWorld*)dmScript::CheckUserType(L, index, g_Bullet3DWorldTypeHash, "Expected user type " BULLET3D_TYPE_NAME_WORLD);
    }

    static Bullet3DLuaWorld* ToWorldUserdata(lua_State* L, int index)
    {
        return (Bullet3DLuaWorld*)dmScript::ToUserType(L, index, g_Bullet3DWorldTypeHash);
    }

    btDiscreteDynamicsWorld* ToBullet3DWorld(lua_State* L, int index)
    {
        Bullet3DLuaWorld* lua_world = ToWorldUserdata(L, index);
        if (!lua_world)
        {
            return 0;
        }
        return (btDiscreteDynamicsWorld*)g_Bullet3DWorldHandles.Get(lua_world->m_Handle);
    }

    bool IsBullet3DWorldValid(lua_State* L, int index)
    {
        return ToBullet3DWorld(L, index) != 0;
    }

    btDiscreteDynamicsWorld* CheckBullet3DWorld(lua_State* L, int index)
    {
        Bullet3DLuaWorld*        lua_world = CheckWorldUserdata(L, index);
        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)g_Bullet3DWorldHandles.Get(lua_world->m_Handle);
        if (!world)
        {
            luaL_error(L, "Invalid bullet3d world handle.");
            return 0;
        }
        return world;
    }

    void PushBullet3DWorld(lua_State* L, void* world_ptr, void* component_world)
    {
        if (!world_ptr)
        {
            lua_pushnil(L);
            return;
        }

        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)world_ptr;
        EnsureWorldHandleCapacity();

        HOpaqueHandle  handle = INVALID_OPAQUE_HANDLE;
        uint64_t       key = WorldPtrToKey(world);
        HOpaqueHandle* existing_handle = g_Bullet3DWorldToHandle.Get(key);
        if (existing_handle && g_Bullet3DWorldHandles.Get(*existing_handle))
        {
            handle = *existing_handle;
        }
        else
        {
            if (existing_handle)
            {
                g_Bullet3DWorldToHandle.Erase(key);
            }
            handle = g_Bullet3DWorldHandles.Put((uintptr_t*)world);
            g_Bullet3DWorldToHandle.Put(key, handle);
        }

        Bullet3DLuaWorld* lua_world = (Bullet3DLuaWorld*)lua_newuserdata(L, sizeof(Bullet3DLuaWorld));
        lua_world->m_Handle = handle;
        lua_world->m_ComponentWorld = component_world;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_WORLD);
        lua_setmetatable(L, -2);
    }

    void ScriptBullet3DInvalidateWorld(void* world_ptr)
    {
        if (!world_ptr)
        {
            return;
        }

        HOpaqueHandle* handle = g_Bullet3DWorldToHandle.Get(WorldPtrToKey((btDiscreteDynamicsWorld*)world_ptr));
        if (handle)
        {
            InvalidateWorldHandle(*handle);
        }
    }

    static int World_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, IsBullet3DWorldValid(L, 1));
        return 1;
    }

    static int World_GetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DWorld(L, 1)->getGravity(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int World_SetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaWorld* lua_world = CheckWorldUserdata(L, 1);
        CheckBullet3DWorld(L, 1);
        CompCollisionObjectSetBullet3DWorldGravity(lua_world->m_ComponentWorld, *dmScript::CheckVector3(L, 2));
        return 0;
    }

    static int World_GetCollisionObjectCount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DWorld(L, 1)->getNumCollisionObjects());
        return 1;
    }

    static int World_ToString(lua_State* L)
    {
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        lua_pushfstring(L, "Bullet3D.%s = %p", BULLET3D_TYPE_NAME_WORLD, world);
        return 1;
    }

    static int World_Equal(lua_State* L)
    {
        Bullet3DLuaWorld* a = ToWorldUserdata(L, 1);
        Bullet3DLuaWorld* b = ToWorldUserdata(L, 2);
        lua_pushboolean(L, a && b && a->m_Handle == b->m_Handle);
        return 1;
    }

    static const luaL_reg WORLD_METHODS[] = {
        { 0, 0 }
    };

    static const luaL_reg WORLD_META[] = {
        { "__tostring", World_ToString },
        { "__eq", World_Equal },
        { 0, 0 }
    };

    static const luaL_reg WORLD_FUNCTIONS[] = {
        { "is_valid", World_IsValid },
        { "get_gravity", World_GetGravity },
        { "set_gravity", World_SetGravity },
        { "get_collision_object_count", World_GetCollisionObjectCount },
        { "get_num_collision_objects", World_GetCollisionObjectCount },
        { 0, 0 }
    };

    void ScriptBullet3DInitializeWorld(lua_State* L)
    {
        g_Bullet3DWorldTypeHash = dmScript::RegisterUserType(L, BULLET3D_TYPE_NAME_WORLD, WORLD_METHODS, WORLD_META);

        lua_newtable(L);
        luaL_register(L, 0, WORLD_FUNCTIONS);
        lua_setfield(L, -2, "world");
    }

    void ScriptBullet3DFinalizeWorld()
    {
        g_Bullet3DWorldTypeHash = 0;
        ClearWorldHandles();
        g_Bullet3DWorldToHandle.Clear();
    }
} // namespace dmGameSystem

/*# Bullet dynamics world API
 *
 * Read and tune the Bullet dynamics world owned by the current collection.
 * Defold remains responsible for world lifetime, stepping, collision objects,
 * callbacks, and debug drawing.
 *
 * @document
 * @name bullet3d.world
 * @namespace bullet3d.world
 * @language Lua
 */

/*# Test whether a world handle is valid
 * @name bullet3d.world.is_valid
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return valid [type:boolean] `true` if the native world still exists
 */

/*# Get world gravity
 * @name bullet3d.world.get_gravity
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Set world gravity
 * @name bullet3d.world.set_gravity
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Get the number of collision objects in the world
 * @name bullet3d.world.get_collision_object_count
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return count [type:number] number of collision objects
 */

/*# Get the number of collision objects in the world
 *
 * Alias for `bullet3d.world.get_collision_object_count`.
 *
 * @name bullet3d.world.get_num_collision_objects
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return count [type:number] number of collision objects
 */
