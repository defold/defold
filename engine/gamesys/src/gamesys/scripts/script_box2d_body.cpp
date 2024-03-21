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

#include <stdio.h>

#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/Joints/b2Joint.h>

#include <dlib/log.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "../gamesys_private.h"

#include "script_box2d.h"

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

    void PushBody(lua_State* L, b2Body* body)
    {
        b2Body** bodyp = (b2Body**)lua_newuserdata(L, sizeof(b2Body*));
        *bodyp = body;
        luaL_getmetatable(L, BOX2D_TYPE_NAME_BODY);
        lua_setmetatable(L, -2);
    }

    b2Body* CheckBody(lua_State* L, int index)
    {
        b2Body** pbody = (b2Body**)dmScript::CheckUserType(L, index, TYPE_HASH_BODY, "Expected user type " BOX2D_TYPE_NAME_BODY);
        return *pbody;
    }

    static b2Body* ToBody(lua_State* L, int index)
    {
        return (b2Body*)dmScript::ToUserType(L, index, TYPE_HASH_BODY);
    }

    static int Body_GetPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(body->GetPosition(), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_ApplyForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 force = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 position = CheckVec2(L, 3, GetPhysicsScale());
        body->ApplyForce(force, position);
        return 0;
    }

    static int Body_ApplyForceToCenter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 force = CheckVec2(L, 2, GetPhysicsScale());
        body->ApplyForceToCenter(force);
        return 0;
    }

    static int Body_ApplyTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->ApplyTorque(luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_ApplyLinearImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 impulse = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 position = CheckVec2(L, 3, GetPhysicsScale()); // position relative center of body
        body->ApplyLinearImpulse(impulse, position);
        return 0;
    }

    static int Body_ApplyAngularImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->ApplyAngularImpulse(luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetMass());
        return 1;
    }

    static int Body_GetInertia(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetInertia());
        return 1;
    }

    static int Body_GetAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetAngle());
        return 1;
    }

    static int Body_GetForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(body->GetForce(), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(body->GetLinearVelocity(), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_SetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 velocity = CheckVec2(L, 2, GetPhysicsScale());
        body->SetLinearVelocity(velocity);
        return 0;
    }

    static int Body_GetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetAngularVelocity());
        return 1;
    }

    static int Body_SetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->SetAngularVelocity(luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetLinearDamping());
        return 1;
    }

    static int Body_SetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->SetLinearDamping(luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetGravityScale(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetGravityScale());
        return 1;
    }

    static int Body_SetGravityScale(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->SetGravityScale(luaL_checknumber(L, 2));
        return 0;
    }

    static int Body_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushnumber(L, body->GetType());
        return 1;
    }

    static int Body_SetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->SetType((b2BodyType)luaL_checknumber(L, 2));
        return 0;
    }
    
    static int Body_IsBullet(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushboolean(L, body->IsBullet());
        return 1;
    }

    static int Body_SetBullet(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        body->SetBullet(enable);
        return 0;
    }

    static int Body_IsAwake(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushboolean(L, body->IsAwake());
        return 1;
    }

    static int Body_SetAwake(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        body->SetAwake(enable);
        return 0;
    }

    static int Body_IsFixedRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushboolean(L, body->IsFixedRotation());
        return 1;
    }

    static int Body_SetFixedRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        body->SetFixedRotation(enable);
        return 0;
    }

    static int Body_IsSleepingAllowed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushboolean(L, body->IsSleepingAllowed());
        return 1;
    }

    static int Body_SetSleepingAllowed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        body->SetSleepingAllowed(enable);
        return 0;
    }

    static int Body_IsActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        lua_pushboolean(L, body->IsActive());
        return 1;
    }

    static int Body_SetActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        bool enable = lua_toboolean(L, 2);
        body->SetActive(enable);
        return 0;
    }

    static int Body_GetWorldPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetWorldPoint(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetWorldVector(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetWorldVector(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLocalPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetLocalPoint(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLocalVector(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetLocalVector(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLinearVelocityFromWorldPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetLinearVelocityFromWorldPoint(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetLinearVelocityFromLocalPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 p = CheckVec2(L, 1, GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(body->GetLinearVelocityFromLocalPoint(p), GetInvPhysicsScale()));
        return 1;
    }

    static int Body_GetWorld(lua_State *L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        PushWorld(L, body->GetWorld()); // TODO: use meta table for this as well
        return 1;
    }

    static int Body_Dump(lua_State *L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        body->Dump();
        return 0;
    }

    static int Body_tostring(lua_State *L)
    {
        b2Body* body = CheckBody(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_BODY, body);
        return 1;
    }

    static int Body_eq(lua_State *L)
    {
        b2Body* a = ToBody(L, 1);
        b2Body* b = ToBody(L, 2);
        lua_pushboolean(L, a && b && a == b);
        return 1;
    }

    static int Body_newindex(lua_State *L)
    {
        return luaL_error(L, BOX2D_TYPE_NAME_BODY " does not support adding new elemnents");
    }

    static const luaL_reg Body_methods[] =
    {
        {0,0}
    };

    static const luaL_reg Body_meta[] =
    {
        {"__tostring", Body_tostring},
        {"__eq", Body_eq},

        {"get_position", Body_GetPosition},

        //{"get_user_data", Body_GetUserData}, - could return the game object id ?
        //{"set_user_data", Body_SetUserData}, - could attach the body to a game object?

        {"get_mass", Body_GetMass},
        {"get_inertia", Body_GetInertia},
        {"get_angle", Body_GetAngle},

        // {"get_mass_data", Body_GetMassData},
        // {"set_mass_data", Body_SetMassData},
        // {"reset_mass_data", Body_ResetMassData},
        // {"synchronize_fixtures", SynchronizeFixtures},
        // SynchronizeSingle(b2Shape* shape, int32 index)

        {"get_force", Body_GetForce},

        {"get_linear_velocity", Body_GetLinearVelocity},
        {"set_linear_velocity", Body_SetLinearVelocity},

        {"get_angular_velocity", Body_GetAngularVelocity},
        {"set_angular_velocity", Body_SetAngularVelocity},

        {"get_linear_damping", Body_GetLinearDamping},
        {"set_linear_damping", Body_SetLinearDamping},

        {"is_bullet", Body_IsBullet},
        {"set_bullet", Body_SetBullet},

        {"is_awake", Body_IsAwake},
        {"set_awake", Body_SetAwake},

        {"is_fixed_rotation", Body_IsFixedRotation},
        {"set_fixed_rotation", Body_SetFixedRotation},

        {"is_sleeping_allowed", Body_IsSleepingAllowed},
        {"set_sleeping_allowed", Body_SetSleepingAllowed},

        {"is_active", Body_IsActive},
        {"set_active", Body_SetActive},

        {"get_gravity_scale", Body_GetGravityScale},
        {"set_gravity_scale", Body_SetGravityScale},

        {"get_type", Body_GetType},
        {"set_type", Body_SetType},

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


        // {"GetNext",GetNext},
        // {"GetFixtureList",GetFixtureList},
        // {"GetContactList",GetContactList},
        // {"GetJointList",GetJointList},

        {"get_world", Body_GetWorld},
        {"dump", Body_Dump},

        {0,0}
    };

    void ScriptBox2DInitializeBody(struct lua_State* L)
    {
        TYPE_HASH_BODY = dmScript::RegisterUserTypeLocal(L, BOX2D_TYPE_NAME_BODY, Body_meta);
    }
}
