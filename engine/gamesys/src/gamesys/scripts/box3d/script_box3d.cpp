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

#include <dlib/array.h>
#include <extension/extension.hpp>
#include <gameobject/script.h>
#include <script/script.h>

#include "gamesys.h"
#include "gamesys_private.h"
#include "components/comp_collision_object.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include <physics/box3d/physics_box3d.h>

extern "C" {
#include <lua/lauxlib.h>
}
/*# Box3D physics
 * Manipulate the bodies of collision components in the calling collection's Box3D world.
 * Positions and linear velocities use game units. Angular velocities use radians per second.
 * @document
 * @name Box3D
 * @namespace b3d
 */
namespace dmGameSystem
{
    struct LuaBody
    {
        b3BodyId  m_Id;
        b3WorldId m_World;
    };
    static const char* BODY = "b3d.body";
    static const char* WORLD = "b3d.world";

    static void SetType(lua_State* L, const char* name)
    {
        luaL_getmetatable(L, name);
        lua_setmetatable(L, -2);
    }

    static b3BodyId Body(lua_State* L)
    {
        LuaBody* b = (LuaBody*)luaL_checkudata(L, 1, BODY);
        if (!b3World_IsValid(b->m_World) || !b3Body_IsValid(b->m_Id))
        {
            luaL_error(L, "Box3D collision component has been destroyed");
        }

        return b->m_Id;
    }

    static b3WorldId World(lua_State* L)
    {
        b3WorldId id = *(b3WorldId*)luaL_checkudata(L, 1, WORLD);
        if (!b3World_IsValid(id))
        {
            luaL_error(L, "Box3D collection has been destroyed");
        }

        return id;
    }

    static void CheckBackend(lua_State* L, void* world)
    {
        if (!world)
        {
            luaL_error(L, "Collection has no collision component world");
        }

        if (GetPhysicsEngineType((CollisionWorld*)world) != PHYSICS_ENGINE_BOX3D)
        {
            luaL_error(L, "Expected a Box3D collection (backend %d)", GetPhysicsEngineType((CollisionWorld*)world));
        }
    }

    /*# get the Box3D world of the calling script's collection
     * @name b3d.get_world
     * @return world [type:userdata] engine-owned world
     */
    static int GetWorld(lua_State* L)
    {
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        void* world = dmGameObject::GetWorld(collection, dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH));
        if (!world)
        {
            lua_pushnil(L);
            return 1;
        }

        CheckBackend(L, world);
        *(b3WorldId*)lua_newuserdata(L, sizeof(b3WorldId)) = *(b3WorldId*)CompCollisionObjectGetBullet3DWorld(world);
        SetType(L, WORLD);
        return 1;
    }

    /*# get the native body of a collision component
     * @name b3d.get_body
     * @param url [type:string|hash|url] address of the collision component
     * @param [with_id] [type:boolean] also return the generation-tagged Box3D body ID for native extensions
     * @return body [type:userdata] handle valid for the lifetime of this body and collection
     * @return [id] [type:hash] exact b3StoreBodyId bits when with_id is true
     */
    static int GetBody(lua_State* L)
    {
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        dmGameObject::HComponent component = 0;
        void* world = 0;
        dmGameObject::GetComponentFromLua(L, 1, collection, COLLISION_OBJECT_EXT, &component, 0, &world);
        CheckBackend(L, world);
        bool with_id = lua_toboolean(L, 2);
        LuaBody* b = (LuaBody*)lua_newuserdata(L, sizeof(LuaBody));
        b->m_Id = *(b3BodyId*)CompCollisionObjectGetBullet3DCollisionObject(component);
        b->m_World = b3Body_GetWorld(b->m_Id);
        SetType(L, BODY);
        if (with_id)
        {
            dmScript::PushHash(L, b3StoreBodyId(b->m_Id));
            return 2;
        }

        return 1;
    }

    static dmVMath::Vector3 CheckFiniteVector3(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3 v = *dmScript::CheckVector3(L, index) * scale;
        b3Vec3 r = { v.getX(), v.getY(), v.getZ() };
        luaL_argcheck(L, b3IsValidVec3(r), index, "expected a finite vector");
        return v;
    }

    static b3Vec3 Vec(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3 v = CheckFiniteVector3(L, index, scale);
        b3Vec3 result = { v.getX(), v.getY(), v.getZ() };
        return result;
    }

    static void PushVec(lua_State* L, b3Vec3 v, float scale)
    {
        dmScript::PushVector3(L, dmVMath::Vector3(v.x, v.y, v.z) * scale);
    }

    /*# Read the world position and orientation.
     * @name b3d.body.get_transform
     * @param body [type:userdata]
     * @return position [type:vector3] world position
     * @return rotation [type:quaternion] orientation
     */
    static int GetTransform(lua_State* L)
    {
        b3BodyId id = Body(L);
        b3WorldTransform t = b3Body_GetTransform(id);
        PushVec(L, t.p, 1.0f / dmPhysics::GetBox3DScale(b3Body_GetWorld(id)));
        dmScript::PushQuat(L, dmVMath::Quat(t.q.v.x, t.q.v.y, t.q.v.z, t.q.s));
        return 2;
    }

    /*# Move the body and its game object immediately.
     * @name b3d.body.set_transform
     * @param body [type:userdata]
     * @param position [type:vector3]
     * @param rotation [type:quaternion]
     */
    static int SetTransform(lua_State* L)
    {
        b3BodyId id = Body(L);
        b3Vec3 p = Vec(L, 2, dmPhysics::GetBox3DScale(b3Body_GetWorld(id)));
        dmVMath::Quat* q = dmScript::CheckQuat(L, 3);
        b3Quat rotation = { { q->getX(), q->getY(), q->getZ() }, q->getW() };
        luaL_argcheck(L, b3IsValidQuat(rotation), 3, "expected a unit quaternion");
        dmPhysics::SetBox3DTransform(id, p, rotation);
        return 0;
    }

    /*# Wake or put the dynamic body to sleep.
     * @name b3d.body.set_awake
     * @param body [type:userdata]
     * @param awake [type:boolean]
     */
    static int SetAwake(lua_State* L)
    {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        dmPhysics::SetBox3DAwake(Body(L), lua_toboolean(L, 2));
        return 0;
    }

    static dmPhysics::HContext3D BodyContext(b3BodyId id)
    {
        return dmPhysics::GetContext3D((dmPhysics::HWorld3D)b3World_GetUserData(b3Body_GetWorld(id)));
    }

    /*# Read the linear velocity in game units per second.
     * @name b3d.body.get_linear_velocity
     * @param body [type:userdata]
     * @return velocity [type:vector3]
     */
    static int GetLinear(lua_State* L)
    {
        b3BodyId id = Body(L);
        dmScript::PushVector3(L, dmPhysics::GetLinearVelocity3D(BodyContext(id), b3Body_GetUserData(id)));
        return 1;
    }

    /*# Set the linear velocity, including at factory spawn time.
     * @name b3d.body.set_linear_velocity
     * @param body [type:userdata]
     * @param velocity [type:vector3]
     */
    static int SetLinear(lua_State* L)
    {
        b3BodyId id = Body(L);
        dmPhysics::SetLinearVelocity3D(BodyContext(id), b3Body_GetUserData(id), CheckFiniteVector3(L, 2, 1.0f));
        return 0;
    }

    /*# Read angular velocity in radians per second.
     * @name b3d.body.get_angular_velocity
     * @param body [type:userdata]
     * @return velocity [type:vector3]
     */
    static int GetAngular(lua_State* L)
    {
        b3BodyId id = Body(L);
        dmScript::PushVector3(L, dmPhysics::GetAngularVelocity3D(BodyContext(id), b3Body_GetUserData(id)));
        return 1;
    }

    /*# Set angular velocity in radians per second.
     * @name b3d.body.set_angular_velocity
     * @param body [type:userdata]
     * @param velocity [type:vector3]
     */
    static int SetAngular(lua_State* L)
    {
        b3BodyId id = Body(L);
        dmPhysics::SetAngularVelocity3D(BodyContext(id), b3Body_GetUserData(id), CheckFiniteVector3(L, 2, 1.0f));
        return 0;
    }

    /*# Get the game object on the other side of each touching contact.
     * @name b3d.body.get_contact_ids
     * @param body [type:userdata]
     * @return ids [type:table] game object identifiers
     */
    static int GetContactIds(lua_State* L)
    {
        b3BodyId id = Body(L);
        int capacity = b3Body_GetContactCapacity(id);
        dmArray<b3ContactData> data;
        data.SetCapacity(capacity);
        int count = b3Body_GetContactData(id, data.Begin(), capacity);
        data.SetSize(count);
        lua_createtable(L, count, 0);
        for (int i = 0; i < count; ++i)
        {
            b3BodyId other = b3Shape_GetBody(data[i].shapeIdA);
            if (B3_ID_EQUALS(other, id))
            {
                other = b3Shape_GetBody(data[i].shapeIdB);
            }

            CollisionComponent* component = (CollisionComponent*)dmPhysics::GetCollisionObjectUserData3D(b3Body_GetUserData(other));
            dmScript::PushHash(L, CompCollisionObjectGetIdentifier(component));
            lua_rawseti(L, -2, i + 1);
        }

        return 1;
    }

    /*# Pause or resume simulation for this collection.
     * @name b3d.world.set_paused
     * @param world [type:userdata]
     * @param paused [type:boolean]
     */
    static int SetPaused(lua_State* L)
    {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        dmPhysics::SetBox3DPaused(World(L), lua_toboolean(L, 2));
        return 0;
    }

    /*# Prepare sleeping hull contacts over successive physics ticks.
     * The bodies keep their poses and gameplay contact events occur on the normal simulation step.
     * @name b3d.world.prepare_sleeping_contacts
     * @param world [type:userdata]
     */
    static int PrepareSleepingContacts(lua_State* L)
    {
        dmPhysics::PrepareBox3DSleepingContacts(World(L));
        return 0;
    }

    /*# Enable Defold collision, contact and trigger delivery in this collection.
     * Enabled by default. Disable when a native post-step observer consumes the Box3D buffers.
     * Raycasts and the native event buffers remain available.
     * @name b3d.world.set_component_events
     * @param world [type:userdata]
     * @param enabled [type:boolean]
     */
    static int SetComponentEvents(lua_State* L)
    {
        luaL_checktype(L, 2, LUA_TBOOLEAN);
        dmPhysics::SetBox3DComponentEvents(World(L), lua_toboolean(L, 2));
        return 0;
    }

    /*# Apply a radial impulse in this collection.
     * @name b3d.world.explode
     * @param world [type:userdata]
     * @param position [type:vector3]
     * @param radius [type:number]
     * @param falloff [type:number]
     * @param impulse_per_area [type:number]
     */
    static int Explode(lua_State* L)
    {
        b3WorldId world = World(L);
        float scale = dmPhysics::GetBox3DScale(world);
        b3ExplosionDef def = b3DefaultExplosionDef();
        def.position = Vec(L, 2, scale);
        def.radius = luaL_checknumber(L, 3) * scale;
        def.falloff = luaL_checknumber(L, 4) * scale;
        def.impulsePerArea = luaL_checknumber(L, 5) / scale;
        luaL_argcheck(L, b3IsValidFloat(def.radius) && def.radius >= 0, 3, "expected a nonnegative radius");
        luaL_argcheck(L, b3IsValidFloat(def.falloff) && def.falloff >= 0, 4, "expected a nonnegative falloff");
        luaL_argcheck(L, b3IsValidFloat(def.impulsePerArea), 5, "expected a finite impulse per area");
        b3World_Explode(world, &def);
        return 0;
    }

    static const luaL_Reg MODULE[] = { { "get_body", GetBody }, { "get_world", GetWorld }, { 0, 0 } };
    static const luaL_Reg BODIES[] = { { "get_transform", GetTransform }, { "set_transform", SetTransform }, { "set_awake", SetAwake },
        { "get_linear_velocity", GetLinear }, { "set_linear_velocity", SetLinear }, { "get_angular_velocity", GetAngular },
        { "set_angular_velocity", SetAngular }, { "get_contact_ids", GetContactIds }, { 0, 0 } };
    static const luaL_Reg WORLDS[] = { { "set_paused", SetPaused }, { "prepare_sleeping_contacts", PrepareSleepingContacts },
        { "set_component_events", SetComponentEvents }, { "explode", Explode }, { 0, 0 } };

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        lua_State* L = params->m_L;
        luaL_newmetatable(L, BODY);
        lua_pop(L, 1);
        luaL_newmetatable(L, WORLD);
        lua_pop(L, 1);
        luaL_register(L, "b3d", MODULE);
        lua_newtable(L);
        luaL_register(L, 0, BODIES);
        lua_setfield(L, -2, "body");
        lua_newtable(L);
        luaL_register(L, 0, WORLDS);
        lua_setfield(L, -2, "world");
        lua_pop(L, 1);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Register(dmExtension::HExtension extension)
    {
        dmExtension::SetInitializeFn(extension, Initialize);
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptBox3DExt, Register, 0)
}
