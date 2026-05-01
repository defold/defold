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

#include <stdio.h>
#include <string.h>


#include <box2d/box2d.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "gamesys_private.h"
#include "components/comp_collision_object.h"

#include "script_box2d_v3.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

//////////////////////////////////////////////////////////////////////////////
// b2Body
namespace dmGameSystem
{
    static uint32_t TYPE_HASH_BODY = 0;

    #define BOX2D_TYPE_NAME_BODY "b2body"

    struct B2DLuaBody
    {
        b2BodyId                  m_Body;
        dmGameObject::HCollection m_Collection;
        dmhash_t                  m_InstanceId;
        uint32_t                  m_InstanceGeneration;
    };

    void PushBody(lua_State* L, void* body, dmGameObject::HCollection collection, dmhash_t instance_id)
    {
        B2DLuaBody* luabody   = (B2DLuaBody*) lua_newuserdata(L, sizeof(B2DLuaBody));
        luabody->m_Body       = *(b2BodyId*) body;
        luabody->m_Collection = collection;
        luabody->m_InstanceId = instance_id;
        luabody->m_InstanceGeneration = 0;

        if (instance_id)
        {
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, instance_id);
            if (instance)
            {
                luabody->m_InstanceGeneration = dmGameObject::GetGeneration(instance);
            }
        }

        luaL_getmetatable(L, BOX2D_TYPE_NAME_BODY);
        lua_setmetatable(L, -2);
    }

    void PushBox2DVersion(lua_State* L)
    {
        b2Version version = b2GetVersion();
        lua_newtable(L);

        lua_pushfstring(L, "%d.%d.%d", version.major, version.minor, version.revision);
        lua_setfield(L, -2, "version");

        lua_pushinteger(L, version.major);
        lua_setfield(L, -2, "major");

        lua_pushinteger(L, version.minor);
        lua_setfield(L, -2, "middle");

        lua_pushinteger(L, version.revision);
        lua_setfield(L, -2, "minor");
    }

    //////////////////////////////////////////////////////////////////////////////
    // b2Vec2
    static b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        b2Vec2 b2v = { v->getX() * scale, v->getY() * scale };
        return b2v;
    }

    static dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale)
    {
        return dmVMath::Vector3(p.x * inv_scale, p.y * inv_scale, 0);
    }

    static void PushTransform(lua_State* L, const b2Transform& transform)
    {
        lua_newtable(L);

        dmScript::PushVector3(L, FromB2(transform.p, GetInvPhysicsScale()));
        lua_setfield(L, -2, "position");

        lua_pushnumber(L, b2Rot_GetAngle(transform.q));
        lua_setfield(L, -2, "angle");
    }

    static void PushMassData(lua_State* L, const b2MassData& mass_data)
    {
        lua_newtable(L);

        lua_pushnumber(L, mass_data.mass);
        lua_setfield(L, -2, "mass");

        dmScript::PushVector3(L, FromB2(mass_data.center, GetInvPhysicsScale()));
        lua_setfield(L, -2, "center");

        lua_pushnumber(L, mass_data.rotationalInertia);
        lua_setfield(L, -2, "inertia");
    }

    static b2MassData CheckMassData(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2MassData mass_data = {};

        lua_getfield(L, index, "mass");
        mass_data.mass = luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "center");
        mass_data.center = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        lua_getfield(L, index, "inertia");
        mass_data.rotationalInertia = luaL_checknumber(L, -1);
        lua_pop(L, 1);

        return mass_data;
    }

    static B2DLuaBody* CheckBodyInternal(lua_State* L, int index)
    {
        return (B2DLuaBody*)dmScript::CheckUserType(L, index, TYPE_HASH_BODY, "Expected user type " BOX2D_TYPE_NAME_BODY);
    }

    static int VerifyBodyInternal(lua_State* L, B2DLuaBody* luabody)
    {
        if (luabody->m_InstanceId) // check if the instance is alive
        {
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(luabody->m_Collection, luabody->m_InstanceId);
            if (!instance || dmGameObject::GetGeneration(instance) != luabody->m_InstanceGeneration)
            {
                return luaL_error(L, "Cannot get b2body for game object instance '%s'. Has the game object been deleted?", dmHashReverseSafe64(luabody->m_InstanceId));
            }
        }

        if (!b2Body_IsValid(luabody->m_Body))
        {
            return luaL_error(L, "Invalid b2body handle.");
        }
        return 0;
    }

    b2BodyId* CheckBody(lua_State* L, int index)
    {
        B2DLuaBody* luabody = CheckBodyInternal(L, index);
        VerifyBodyInternal(L, luabody);
        return &luabody->m_Body;
    }

    static b2BodyId* ToBody(lua_State* L, int index)
    {
        B2DLuaBody* luabody = (B2DLuaBody*)dmScript::ToUserType(L, index, TYPE_HASH_BODY);
        return luabody ? &luabody->m_Body : 0;
    }

    static int GetShapeCount(b2BodyId body)
    {
        return b2Body_GetShapeCount(body);
    }

    b2ShapeId GetShapeByIndex(b2BodyId body, int shape_index)
    {
        if (shape_index <= 0)
        {
            return b2_nullShapeId;
        }

        const int shape_count = GetShapeCount(body);
        if (shape_index > shape_count)
        {
            return b2_nullShapeId;
        }

        dmArray<b2ShapeId> shapes;
        shapes.SetCapacity(shape_count);
        shapes.SetSize(shape_count);
        b2Body_GetShapes(body, shapes.Begin(), shape_count);
        return shapes[shape_index - 1];
    }

    static int GetShapeIndex(b2BodyId body, b2ShapeId shape)
    {
        const int shape_count = GetShapeCount(body);
        if (shape_count == 0)
        {
            return 0;
        }

        dmArray<b2ShapeId> shapes;
        shapes.SetCapacity(shape_count);
        shapes.SetSize(shape_count);
        b2Body_GetShapes(body, shapes.Begin(), shape_count);
        for (int i = 0; i < shape_count; ++i)
        {
            if (B2_ID_EQUALS(shapes[i], shape))
            {
                return i + 1;
            }
        }
        return 0;
    }

    static void PushShapeInfo(lua_State* L, b2ShapeId shape, int shape_index)
    {
        lua_newtable(L);

        lua_pushinteger(L, shape_index);
        lua_setfield(L, -2, "index");

        lua_pushinteger(L, NormalizeShapeTypeForLua(b2Shape_GetType(shape)));
        lua_setfield(L, -2, "type");

        lua_pushboolean(L, b2Shape_IsSensor(shape));
        lua_setfield(L, -2, "sensor");

        lua_pushnumber(L, b2Shape_GetDensity(shape));
        lua_setfield(L, -2, "density");

        lua_pushnumber(L, b2Shape_GetFriction(shape));
        lua_setfield(L, -2, "friction");

        lua_pushnumber(L, b2Shape_GetRestitution(shape));
        lua_setfield(L, -2, "restitution");

        lua_pushinteger(L, 1);
        lua_setfield(L, -2, "child_count");

        b2ChainId parent_chain = b2Shape_GetParentChain(shape);
        lua_pushboolean(L, b2Chain_IsValid(parent_chain));
        lua_setfield(L, -2, "is_chain_segment");
    }

    static int Body_GetPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetPosition(*body), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        PushTransform(L, b2Body_GetTransform(*body));
        return 1;
    }

    static int Body_SetTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 position = CheckVec2(L, 2, GetPhysicsScale());
        float angle = luaL_checknumber(L, 3);
        b2Rot rot = b2MakeRot(angle);
        b2Body_SetTransform(*body, position, rot);
        return 0;
    }

    static int Body_ApplyForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 force = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 position = CheckVec2(L, 3, GetPhysicsScale());
        b2Body_ApplyForce(*body, force, position, true);
        return 0;
    }

    static int Body_ApplyForceToCenter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 force = CheckVec2(L, 2, GetPhysicsScale());
        b2Body_ApplyForceToCenter(*body, force, true);
        return 0;
    }

    static int Body_ApplyTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_ApplyTorque(*body, luaL_checknumber(L, 2), true);
        return 0;
    }

    static int Body_ApplyLinearImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 impulse = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 position = CheckVec2(L, 3, GetPhysicsScale()); // position relative center of body
        b2Body_ApplyLinearImpulse(*body, impulse, position, true);
        return 0;
    }

    static int Body_ApplyAngularImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_ApplyAngularImpulse(*body, luaL_checknumber(L, 2), true);
        return 0;
    }

    static int Body_GetMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetMass(*body));
        return 1;
    }

    static int Body_GetInertia(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetRotationalInertia(*body));
        return 1;
    }

    // Old name: use Body_GetInertia
    static int Body_GetRotationalInertia(lua_State* L)
    {
        return Body_GetInertia(L);
    }

    static int Body_GetMassData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        PushMassData(L, b2Body_GetMassData(*body));
        return 1;
    }

    static int Body_SetMassData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetMassData(*body, CheckMassData(L, 2));
        return 0;
    }

    static int Body_ResetMassData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_ApplyMassFromShapes(*body);
        return 0;
    }

    static int Body_GetAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Rot rot = b2Body_GetRotation(*body);
        float angle = b2Rot_GetAngle(rot);
        lua_pushnumber(L, angle);
        return 1;
    }

    // Old name: Body_GetWorldCenter
    static int Body_GetWorldCenterOfMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldCenterOfMass(*body), GetInvPhysicsScale()));
        return 1;
    }

    // Old name: Body_GetLocalCenter
    static int Body_GetLocalCenterOfMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetLocalCenterOfMass(*body), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetWorldCenter(lua_State* L)
    {
        return Body_GetWorldCenterOfMass(L);
    }

    static int Body_GetLocalCenter(lua_State* L)
    {
        return Body_GetLocalCenterOfMass(L);
    }

    static int Body_GetForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetTotalForce(*body), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetShapes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);

        const int shape_count = GetShapeCount(*body);
        dmArray<b2ShapeId> shapes;
        shapes.SetCapacity(shape_count);
        shapes.SetSize(shape_count);
        b2Body_GetShapes(*body, shapes.Begin(), shape_count);

        lua_newtable(L);
        for (int i = 0; i < shape_count; ++i)
        {
            PushShapeInfo(L, shapes[i], i + 1);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }

    static int Body_CreateShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        B2DShapeDef shape_def = {};
        b2ShapeDef shape_create_def = b2DefaultShapeDef();
        CheckShapeCreateDef(L, 2, &shape_def, &shape_create_def);
        shape_create_def.userData = b2Body_GetUserData(*body);

        b2ShapeId shape = b2_nullShapeId;
        switch (shape_def.m_Type)
        {
            case B2DShapeDef::TYPE_CIRCLE:
                shape = b2CreateCircleShape(*body, &shape_create_def, &shape_def.m_Circle);
                break;
            case B2DShapeDef::TYPE_CAPSULE:
                shape = b2CreateCapsuleShape(*body, &shape_create_def, &shape_def.m_Capsule);
                break;
            case B2DShapeDef::TYPE_SEGMENT:
                shape = b2CreateSegmentShape(*body, &shape_create_def, &shape_def.m_Segment);
                break;
            case B2DShapeDef::TYPE_POLYGON:
                shape = b2CreatePolygonShape(*body, &shape_create_def, &shape_def.m_Polygon);
                break;
        }

        if (!b2Shape_IsValid(shape))
        {
            return luaL_error(L, "Could not create shape. The world may be locked.");
        }

        int shape_index = GetShapeIndex(*body, shape);
        if (shape_index == 0)
        {
            b2DestroyShape(shape, true);
            return luaL_error(L, "Could not resolve created shape.");
        }

        PushShapeInfo(L, shape, shape_index);
        return 1;
    }

    static int Body_DestroyShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        int shape_index = luaL_checkinteger(L, 2);
        b2ShapeId shape = GetShapeByIndex(*body, shape_index);
        if (!b2Shape_IsValid(shape))
        {
            return luaL_error(L, "shape_index %d out of range.", shape_index);
        }

        b2ChainId parent_chain = b2Shape_GetParentChain(shape);
        if (b2Chain_IsValid(parent_chain))
        {
            b2DestroyChain(parent_chain);
        }
        else
        {
            b2DestroyShape(shape, true);
        }
        return 0;
    }

    static int Body_GetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetLinearVelocity(*body), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_SetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 velocity = CheckVec2(L, 2, GetPhysicsScale());
        b2Body_SetLinearVelocity(*body, velocity);
        return 0;
    }

    static int Body_GetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetAngularVelocity(*body));
        return 1;
    }

    static int Body_SetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetAngularVelocity(*body, luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetLinearDamping(*body));
        return 1;
    }

    static int Body_SetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetLinearDamping(*body, luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetAngularDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetAngularDamping(*body));
        return 1;
    }

    static int Body_SetAngularDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetAngularDamping(*body, luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetGravityScale(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetGravityScale(*body));
        return 1;
    }

    static int Body_SetGravityScale(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetGravityScale(*body, luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushnumber(L, b2Body_GetType(*body));
        return 1;
    }

    static int Body_SetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        b2Body_SetType(*body, (b2BodyType)luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_IsBullet(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushboolean(L, b2Body_IsBullet(*body));
        return 1;
    }

    static int Body_SetBullet(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        b2Body_SetBullet(*body, enable);
        return 0;
    }

    static int Body_IsAwake(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushboolean(L, b2Body_IsAwake(*body));
        return 1;
    }

    static int Body_SetAwake(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        b2Body_SetAwake(*body, enable);
        return 0;
    }

    static int Body_IsFixedRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushboolean(L, b2Body_IsFixedRotation(*body));
        return 1;
    }

    static int Body_SetFixedRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        b2Body_SetFixedRotation(*body, enable);
        return 0;
    }

    // Old name: Body_IsSleepingAllowed
    static int Body_IsSleepingEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushboolean(L, b2Body_IsSleepEnabled(*body));
        return 1;
    }

    static int Body_IsSleepingAllowed(lua_State* L)
    {
        return Body_IsSleepingEnabled(L);
    }

    // Old name: Body_SetSleepingAllowed
    static int Body_EnableSleep(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        b2Body_EnableSleep(*body, enable);
        return 0;
    }

    static int Body_SetSleepingAllowed(lua_State* L)
    {
        return Body_EnableSleep(L);
    }

    static int Body_IsActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        lua_pushboolean(L, b2Body_IsEnabled(*body));
        return 1;
    }

    static int Body_SetActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2BodyId* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        if (enable)
        {
            b2Body_Enable(*body);
        }
        else
        {
            b2Body_Disable(*body);
        }
        return 0;
    }

    static int Body_GetWorldPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPoint(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetWorldVector(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldVector(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLocalPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetLocalPoint(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLocalVector(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetLocalVector(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLinearVelocityFromWorldPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPointVelocity(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLinearVelocityFromLocalPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 2, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Body_GetLocalPointVelocity(*body, p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetWorld(lua_State *L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body = CheckBody(L, 1);
        b2WorldId* world_id_ptr = (b2WorldId*) lua_newuserdata(L, sizeof(b2WorldId));
        *world_id_ptr = b2Body_GetWorld(*body);
        return 1;
    }

    static int Body_tostring(lua_State *L)
    {
        b2BodyId* body = CheckBody(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_BODY, body);
        return 1;
    }

    static int Body_eq(lua_State *L)
    {
        b2BodyId* a = ToBody(L, 1);
        b2BodyId* b = ToBody(L, 2);
        lua_pushboolean(L, a && b && memcmp(a, b, sizeof(b2BodyId)) == 0);
        return 1;
    }

    static const luaL_reg Body_methods[] =
    {
        {0,0}
    };

    static const luaL_reg Body_meta[] =
    {
        {"__tostring", Body_tostring},
        {"__eq", Body_eq},
        {0,0}
    };

    static const luaL_reg Body_functions[] =
    {
        {"create_shape", Body_CreateShape},
        {"get_position", Body_GetPosition},
        {"get_transform", Body_GetTransform},
        {"set_transform", Body_SetTransform},

        //{"get_user_data", Body_GetUserData}, - could return the game object id ? ur url?
        //{"set_user_data", Body_SetUserData}, - could attach the body to a game object?

        {"get_mass", Body_GetMass},
        {"get_inertia", Body_GetInertia},
        {"get_rotational_inertia", Body_GetRotationalInertia},
        {"get_mass_data", Body_GetMassData},
        {"set_mass_data", Body_SetMassData},
        {"reset_mass_data", Body_ResetMassData},
        {"get_angle", Body_GetAngle},
        {"get_shapes", Body_GetShapes},
        {"destroy_shape", Body_DestroyShape},

        {"get_linear_velocity", Body_GetLinearVelocity},
        {"set_linear_velocity", Body_SetLinearVelocity},

        {"get_angular_velocity", Body_GetAngularVelocity},
        {"set_angular_velocity", Body_SetAngularVelocity},

        {"get_linear_damping", Body_GetLinearDamping},
        {"set_linear_damping", Body_SetLinearDamping},

        {"get_angular_damping", Body_GetAngularDamping},
        {"set_angular_damping", Body_SetAngularDamping},

        {"is_bullet", Body_IsBullet},
        {"set_bullet", Body_SetBullet},

        {"is_awake", Body_IsAwake},
        {"set_awake", Body_SetAwake},

        {"is_fixed_rotation", Body_IsFixedRotation},
        {"set_fixed_rotation", Body_SetFixedRotation},

        {"is_sleeping_enabled", Body_IsSleepingEnabled},
        {"enable_sleep", Body_EnableSleep},
        {"is_sleeping_allowed", Body_IsSleepingAllowed},
        {"set_sleeping_allowed", Body_SetSleepingAllowed},

        {"is_active", Body_IsActive},
        {"set_active", Body_SetActive},

        {"get_gravity_scale", Body_GetGravityScale},
        {"set_gravity_scale", Body_SetGravityScale},

        {"get_type", Body_GetType},
        {"set_type", Body_SetType},

        {"get_world_center_of_mass", Body_GetWorldCenterOfMass},
        {"get_world_center", Body_GetWorldCenter},
        {"get_local_center_of_mass", Body_GetLocalCenterOfMass},
        {"get_local_center", Body_GetLocalCenter},
        {"get_force", Body_GetForce},

        {"get_world_point", Body_GetWorldPoint},
        {"get_world_vector", Body_GetWorldVector},

        {"get_local_point", Body_GetLocalPoint},
        {"get_local_vector", Body_GetLocalVector},

        {"get_linear_velocity_from_world_point", Body_GetLinearVelocityFromWorldPoint},
        {"get_linear_velocity_from_local_point", Body_GetLinearVelocityFromLocalPoint},

        {"apply_force", Body_ApplyForce},
        {"apply_force_to_center", Body_ApplyForceToCenter},
        {"apply_torque", Body_ApplyTorque},

        {"apply_linear_impulse", Body_ApplyLinearImpulse},
        {"apply_angular_impulse", Body_ApplyAngularImpulse},

        {"get_world", Body_GetWorld},

        {0,0}
    };

    void ScriptBox2DInitializeBody(struct lua_State* L)
    {
        TYPE_HASH_BODY = dmScript::RegisterUserType(L, BOX2D_TYPE_NAME_BODY, Body_methods, Body_meta);

        lua_newtable(L);
        luaL_register(L, 0, Body_functions);

#define SET_CONSTANT(NAME, CUSTOM_NAME) \
        lua_pushnumber(L, (lua_Number) NAME); \
        lua_setfield(L, -2, CUSTOM_NAME);

        SET_CONSTANT(b2_staticBody, "B2_STATIC_BODY");
        SET_CONSTANT(b2_kinematicBody, "B2_KINEMATIC_BODY");
        SET_CONSTANT(b2_dynamicBody, "B2_DYNAMIC_BODY");

#undef SET_CONSTANT

        lua_setfield(L, -2, "body");

        ScriptBox2DInitializeShape(L);
    }

    void ScriptBox2DInvalidateBody(void* body)
    {
        (void)body;
    }

    void ScriptBox2DFinalizeBody()
    {
        TYPE_HASH_BODY = 0;
    }
}

/*# Box2D b2Body documentation
 *
 * Functions for interacting with Box2D bodies.
 *
 * @document
 * @name b2d.body
 * @namespace b2d.body
 * @language Lua
 */

/*# Box2D world
 * @typedef
 * @name b2World
 * @param value [type:userdata]
 */

/*# Box2D body
 * @typedef
 * @name b2Body
 * @param value [type:userdata]
 */

/*# Static (immovable) body
 *
 * @name b2d.body.B2_STATIC_BODY
 * @constant
 */
/*# Kinematic body
 *
 * @name b2d.body.B2_KINEMATIC_BODY
 * @constant
 */
/*# Dynamic body
 *
 * @name b2d.body.B2_DYNAMIC_BODY
 * @constant
 */

/**
 * Creates a shape and attaches it to this body.
 * If the density is non-zero, this function automatically updates the mass of the body.
 * Contacts are not created until the next time step.
 * @warning This function is locked during callbacks.
 * @name b2d.body.create_shape
 * @param body [type: b2Body] body
 * @param definition [type: table] the shape definition.
 */

/**
 * Destroy a shape. This removes the shape from the broad-phase and
 * destroys all contacts associated with this shape. This will
 * automatically adjust the mass of the body if the body is dynamic and the
 * shape has positive density.
 * All shapes attached to a body are implicitly destroyed when the body is destroyed.
 * @warning This function is locked during callbacks.
 * @name b2d.body.destroy_shape
 * @param body [type: b2Body] body
 * @param shape_index [type: number] 1-based shape index from `b2d.body.get_shapes`
 */

/*#
 * Set the position of the body's origin and rotation.
 * This breaks any contacts and wakes the other bodies.
 * Manipulating a body's transform may cause non-physical behavior.
 * @name b2d.body.set_transform
 * @param body [type: b2Body] body
 * @param position [type: vector3] the world position of the body's local origin.
 * @param angle [type: number] the world position of the body's local origin.
 */

/** Get the body transform for the body's origin.
 * @name b2d.body.get_transform
 * @param body [type: b2Body] body
 * @return transform [type: b2Transform] the world position of the body's origin.
 */

/*# Get the world body origin position.
 * @name b2d.body.get_position
 * @param body [type: b2Body] body
 * @return position [type: vector3] the world position of the body's origin.
 */

/*# Get the angle in radians.
 * @name b2d.body.get_angle
 * @param body [type: b2Body] body
 * @return angle [type: number] the current world rotation angle in radians.
 */

/*# Get the world position of the center of mass.
 * @name b2d.body.get_world_center_of_mass
 * @param body [type: b2Body] body
 * @return center [type: vector3] Get the world position of the center of mass.
 */

/*# Get the local position of the center of mass.
 * @name b2d.body.get_local_center_of_mass
 * @param body [type: b2Body] body
 * @return center [type: vector3] Get the local position of the center of mass.
 */

/*# Set the linear velocity of the center of mass.
 * @name b2d.body.set_linear_velocity
 * @param body [type: b2Body] body
 * @param velocity [type: vector3] the new linear velocity of the center of mass.
 */

/*# Get the linear velocity of the center of mass.
 * @name b2d.body.get_linear_velocity
 * @param body [type: b2Body] body
 * @return velocity [type: vector3] the linear velocity of the center of mass.
 */

/*# Set the angular velocity.
 * @name b2d.body.set_angular_velocity
 * @param body [type: b2Body] body
 * @param omega [type: number] the new angular velocity in radians/second.
 */

/*# Get the angular velocity.
 * @name b2d.body.get_angular_velocity
 * @param body [type: b2Body] body
 * @return velocity [type: number] the angular velocity in radians/second.
 */

/*#
 * Apply a force at a world point. If the force is not
 * applied at the center of mass, it will generate a torque and
 * affect the angular velocity. This wakes up the body.
 * @name b2d.body.apply_force
 * @param body [type: b2Body] body
 * @param force [type: vector3] the world force vector, usually in Newtons (N).
 * @param point [type: vector3] the world position of the point of application.
 */

/*# Apply a force to the center of mass. This wakes up the body.
 * @name b2d.body.apply_force_to_center
 * @param body [type: b2Body] body
 * @param force [type: vector3] the world force vector, usually in Newtons (N).
 */

/*#
 * Apply a torque. This affects the angular velocity
 * without affecting the linear velocity of the center of mass.
 * This wakes up the body.
 * @name b2d.body.apply_torque
 * @param body [type: b2Body] body
 * @param torque [type: number] torque about the z-axis (out of the screen), usually in N-m.
 */

/*#
 * Apply an impulse at a point. This immediately modifies the velocity.
 * It also modifies the angular velocity if the point of application
 * is not at the center of mass. This wakes up the body.
 * @name b2d.body.apply_linear_impulse
 * @param body [type: b2Body] body
 * @param impulse [type: vector3] the world impulse vector, usually in N-seconds or kg-m/s.
 * @param point [type: vector3] the world position of the point of application.
 */

/*# Apply an angular impulse.
 * @name b2d.body.apply_angular_impulse
 * @param body [type: b2Body] body
 * @param impulse [type: number] impulse the angular impulse in units of kg*m*m/s
 */

/*# Get the total mass of the body.
 * @name b2d.body.get_mass
 * @param body [type: b2Body] body
 * @return mass [type: number] the mass, usually in kilograms (kg).
 */

/*# Get the rotational inertia of the body about the local origin.
 * @name b2d.body.get_rotational_inertia
 * @param body [type: b2Body] body
 * @return inertia [type: number] the rotational inertia, usually in kg-m^2.
 */

/**
 * Get the mass data of the body.
 * @name b2d.body.get_mass_data
 * @param body [type: b2Body] body
 * @return data [type: b2MassData] a struct containing the mass, inertia and center of the body.
 */

/**
 * Set the mass properties to override the mass properties of the shapes.
 * @note This function has no effect if the body isn't dynamic.
 * @note This changes the center of mass position.
 * @note Creating or destroying shapes can also alter the mass.
 * @name b2d.body.set_mass_data
 * @param body [type: b2Body] body
 * @param data [type: b2MassData] the mass properties.
 */

/*#
 * This resets the mass properties to the sum of the mass properties of the shapes.
 * This normally does not need to be called unless you called SetMassData to override
 * @name b2d.body.reset_mass_data
 * @param body [type: b2Body] body
 */

/*# Get the world coordinates of a point given the local coordinates.
 * @name b2d.body.get_world_point
 * @param body [type: b2Body] body
 * @param local_vector [type: vector3] localPoint a point on the body measured relative the the body's origin.
 * @return vector [type: vector3] the same point expressed in world coordinates.
 */

/*# Get the world coordinates of a vector given the local coordinates.
 * @name b2d.body.get_world_vector
 * @param body [type: b2Body] body
 * @param local_vector [type: vector3] a vector fixed in the body.
 * @return vector [type: vector3] the same vector expressed in world coordinates.
 */

/*# Gets a local point relative to the body's origin given a world point.
 * @name b2d.body.get_local_point
 * @param body [type: b2Body] body
 * @param world_point [type: vector3] a point in world coordinates.
 * @return vector [type: vector3] the corresponding local point relative to the body's origin.
 */

/*# Gets a local vector given a world vector.
 * @name b2d.body.get_local_vector
 * @param body [type: b2Body] body
 * @param world_vector [type: vector3] a vector in world coordinates.
 * @return vector [type: vector3] the corresponding local vector.
 */

/*# Get the world linear velocity of a world point attached to this body.
 * @name b2d.body.get_linear_velocity_from_world_point
 * @param body [type: b2Body] body
 * @param world_point [type: vector3] a point in world coordinates.
 * @return velocity [type: vector3] the world velocity of a point.
 */

/*# Get the world velocity of a local point.
 * @name b2d.body.get_linear_velocity_from_local_point
 * @param body [type: b2Body] body
 * @param local_point [type: vector3] a point in local coordinates.
 * @return velocity [type: vector3] the world velocity of a point.
 */

/*# Set the linear damping of the body.
 * @name b2d.body.set_linear_damping
 * @param body [type: b2Body] body
 * @param damping [type: number] the damping
 */

/*# Get the linear damping of the body.
 * @name b2d.body.get_linear_damping
 * @param body [type: b2Body] body
 * @return damping [type: number] the damping
 */

/*# Set the angular damping of the body.
 * @name b2d.body.set_angular_damping
 * @param body [type: b2Body] body
 * @param damping [type: number] the damping
 */

/*# Get the angular damping of the body.
 * @name b2d.body.get_angular_damping
 * @param body [type: b2Body] body
 * @return damping [type: number] the damping
 */

/*# Set the gravity scale of the body.
 * @name b2d.body.set_gravity_scale
 * @param body [type: b2Body] body
 * @param scale [type: number] the scale
 */

/*# Get the gravity scale of the body.
 * @name b2d.body.get_gravity_scale
 * @param body [type: b2Body] body
 * @return scale [type: number] the scale
 */

/*# Set the type of this body. This may alter the mass and velocity.
 * @name b2d.body.set_type
 * @param body [type: b2Body] body
 * @param type [type: b2BodyType] the body type
 */

/*# Get the type of this body.
 * @name b2d.body.get_type
 * @param body [type: b2Body] body
 * @return type [type: b2BodyType] the body type
 */


/*# Should this body be treated like a bullet for continuous collision detection?
 * @name b2d.body.set_bullet
 * @param body [type: b2Body] body
 * @param enable [type: boolean] if true, the body will be in bullet mode
 */

/*# Is this body in bullet mode
 * @name b2d.body.is_bullet
 * @param body [type: b2Body] body
 * @return enabled [type: boolean] true if the body is in bullet mode
 */

/*# You can disable sleeping on this body. If you disable sleeping, the body will be woken.
 * @name b2d.body.enable_sleep
 * @param body [type: b2Body] body
 * @param enable [type: boolean] if false, the body will never sleep, and consume more CPU
 */

/*# Is this body allowed to sleep
 * @name b2d.body.is_sleeping_enabled
 * @param body [type: b2Body] body
 * @return enabled [type: boolean] true if the body is allowed to sleep
 */

/*# Set the sleep state of the body. A sleeping body has very low CPU cost.
 * @name b2d.body.set_awake
 * @param body [type: b2Body] body
 * @param enable [type: boolean] flag set to false to put body to sleep, true to wake it.
 */

/*# Get the sleeping state of this body.
 * @name b2d.body.is_awake
 * @param body [type: b2Body] body
 * @return enabled [type: boolean] true if the body is awake, false if it's sleeping.
 */

/*# Set the active state of the body
 * Set the active state of the body. An inactive body is not
 * simulated and cannot be collided with or woken up.
 * If you pass a flag of true, all shapes will be added to the
 * broad-phase.
 * If you pass a flag of false, all shapes will be removed from
 * the broad-phase and all contacts will be destroyed.
 * Shapes and joints are otherwise unaffected. You may continue
 * to create/destroy shapes and joints on inactive bodies.
 * Shapes on an inactive body are implicitly inactive and will
 * not participate in collisions, ray-casts, or queries.
 * Joints connected to an inactive body are implicitly inactive.
 * An inactive body is still owned by a b2World object and remains
 * in the body list.
 *
 * @name b2d.body.set_active
 * @param body [type: b2Body] body
 * @param enable [type: boolean] true if the body should be active
 */

/*# Get the active state of the body.
 * @name b2d.body.is_active
 * @param body [type: b2Body] body
 * @return enabled [type: boolean] is the body active
 */


/*# Set this body to have fixed rotation. This causes the mass to be reset.
 * @name b2d.body.set_fixed_rotation
 * @param body [type: b2Body] body
 * @param enable [type: boolean] true if the rotation should be fixed
 */

/*# Does this body have fixed rotation?
 * @name b2d.body.is_fixed_rotation
 * @param body [type: b2Body] body
 * @return enabled [type: boolean] is the rotation fixed
 */

/** Get the list of all shapes attached to this body.
 * @name b2d.body.get_shapes
 * @param body [type: b2Body] body
 * @return shapes [type: table] a table of functional shape entries
 */

/** Get the list of all joints attached to this body.
 * @name b2d.body.get_joint_list
 * @param body [type: b2Body] body
 * @return edge [type: b2JointEdge] the first joint
 */

/** Get the list of all contacts attached to this body.
 * @name b2d.body.get_contact_list
 * @param body [type: b2Body] body
 * @return edge [type: b2ContactEdge] the first edge
 */

/** Get the user data pointer that was provided in the body definition.
 * @name b2d.body.get_user_data
 * @param body [type: b2Body] body
 * @return id [type: hash] the game object id this body is connected to
 */

/** Set the user data. Use this to store your application specific data.
 * @name b2d.body.set_user_data
 * @param body [type: b2Body] body
 * @param id [type: hash] the game object id
 */

/*# Get the parent world of this body.
 * @name b2d.body.get_world
 * @param body [type: b2Body] body
 * @return world [type: b2World]
 */

/** Get the total force currently applied on this object
 * @name b2d.body.get_force
 * @note Defold Specific
 * @param body [type: b2Body] body
 * @return force [type: vector3]
 */
