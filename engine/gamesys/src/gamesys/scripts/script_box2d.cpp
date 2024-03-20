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

#include <dlib/log.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "../gamesys_private.h"

#include "components/comp_collision_object.h"

#include "script_box2d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static const float g_PhysicsScale = 0.02f; // TODO: Get this from the physics world (or settings)
    static const float g_InvPhysicsScale = 1.0f / g_PhysicsScale;

    //////////////////////////////////////////////////////////////////////////////
    // Types

    enum Box2DUserType
    {
        BOX2D_TYPE_BODY,
        BOX2D_TYPE_MAX,
    };

    static uint32_t TYPE_HASHES[BOX2D_TYPE_MAX];

    #define BOX2D_TYPE_NAME_BODY "b2body"

    //////////////////////////////////////////////////////////////////////////////
    // b2Vec2

    static b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        return b2Vec2(v->getX() * scale, v->getY() * scale);
    }

    inline dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale)
    {
        return dmVMath::Vector3(p.x * inv_scale, p.y * inv_scale, 0);
    }

    //////////////////////////////////////////////////////////////////////////////
    // b2Body

    static void PushBody(lua_State* L, b2Body* body)
    {
        b2Body** bodyp = (b2Body**)lua_newuserdata(L, sizeof(b2Body*));
        *bodyp = body;
        luaL_getmetatable(L, BOX2D_TYPE_NAME_BODY);
        lua_setmetatable(L, -2);
    }

    static b2Body* CheckBody(lua_State* L, int index)
    {
        b2Body** pbody = (b2Body**)dmScript::CheckUserType(L, index, TYPE_HASHES[BOX2D_TYPE_BODY], "Expected user type " BOX2D_TYPE_NAME_BODY);
        return *pbody;
    }

    static b2Body* ToBody(lua_State* L, int index)
    {
        return (b2Body*)dmScript::ToUserType(L, index, TYPE_HASHES[BOX2D_TYPE_BODY]);
    }

    static int Body_ApplyLinearImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 impulse = CheckVec2(L, 2, g_PhysicsScale);
        b2Vec2 position = CheckVec2(L, 3, g_PhysicsScale); // position relative center of body
        body->ApplyLinearImpulse(impulse, position);
        return 0;
    }

    static int Body_GetPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(body->GetPosition(), g_InvPhysicsScale));
        return 1;
    }

    static int Body_ApplyAngularImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        float impulse = luaL_checknumber(L, 2);
        body->ApplyAngularImpulse(impulse);
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

    static int Body_GetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        dmScript::PushVector3(L, FromB2(body->GetLinearVelocity(), g_InvPhysicsScale));
        return 1;
    }

    static int Body_SetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        b2Vec2 velocity = CheckVec2(L, 2, g_PhysicsScale);
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
        float velocity = luaL_checknumber(L, 2);
        body->SetAngularVelocity(velocity);
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
        float damping = luaL_checknumber(L, 2);
        body->SetLinearDamping(damping);
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

        // GetUserData - could return the game object id ?
        // SetUserData - we shouldn't be able to set this.

        {"get_mass", Body_GetMass},
        {"get_inertia", Body_GetInertia},
        {"get_angle", Body_GetAngle},

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


        // GetWorldPoint
        // GetWorldVector
        // GetLocalPoint
        // GetLocalVector

        // GetLinearVelocityFromWorldPoint
        // GetLinearVelocityFromLocalPoint

        // GetGravityScale
        // SetGravityScale

        //{"apply_force", Body_ApplyForce},
        //{"apply_force_to_center", Body_ApplyForceToCenter},
        //{"apply_torque", Body_ApplyTorque},

        {"apply_linear_impulse", Body_ApplyLinearImpulse},
        {"apply_angular_impulse", Body_ApplyAngularImpulse},

        {0,0}
    };

    //////////////////////////////////////////////////////////////////////////////

    static void GetCollisionObject(lua_State* L, int index, dmGameObject::HCollection collection, void** comp, void** comp_world)
    {
        dmGameObject::GetComponentUserDataFromLua(L, index, collection, COLLISION_OBJECT_EXT, (uintptr_t*)comp, 0, comp_world);
    }

    static int B2D_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        void* comp_world = dmGameObject::GetWorld(collection, component_type_index);
        b2World* world = dmGameSystem::CompCollisionObjectGetBox2DWorld(comp_world);

        if (world)
            lua_pushlightuserdata(L, world);
        else
            lua_pushnil(L);
        return 1;
    }

    //////////////////////////////////////////////////////////////////////////////

    static int B2D_GetBody(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));

        void* component = 0;
        GetCollisionObject(L, 1, collection, &component, 0);

        b2Body* body = dmGameSystem::CompCollisionObjectGetBox2DBody(component);

        if (body)
            PushBody(L, body);
        else
            lua_pushnil(L);
        return 1;
    }

    static const luaL_reg BOX2D_FUNCTIONS[] =
    {
        // {"ray_cast",        Physics_RayCastAsync}, // Deprecated
        // {"raycast_async",   Physics_RayCastAsync},
        // {"raycast",         Physics_RayCast},

        // {"create_joint",    Physics_CreateJoint},
        // {"destroy_joint",   Physics_DestroyJoint},
        // {"get_joint_properties", Physics_GetJointProperties},
        // {"set_joint_properties", Physics_SetJointProperties},
        // {"get_joint_reaction_force",  Physics_GetJointReactionForce},
        // {"get_joint_reaction_torque", Physics_GetJointReactionTorque},

        // {"set_gravity",     Physics_SetGravity},
        // {"get_gravity",     Physics_GetGravity},

        // {"set_hflip",       Physics_SetFlipH},
        // {"set_vflip",       Physics_SetFlipV},
        // {"wakeup",          Physics_Wakeup},
        // {"get_group",       Physics_GetGroup},
        // {"set_group",       Physics_SetGroup},
        // {"get_maskbit",     Physics_GetMaskBit},
        // {"set_maskbit",     Physics_SetMaskBit},
        // {"set_listener",    Physics_SetListener},
        // {"update_mass",     Physics_UpdateMass},

        // // Shapes
        // {"get_shape", Physics_GetShape},
        // {"set_shape", Physics_SetShape},


        {"get_world", B2D_GetWorld},
        {"get_body", B2D_GetBody},

        {0, 0}
    };

    void ScriptBox2DInitialize(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;

        struct
        {
            const char* m_Name;
            const luaL_reg* m_Methods;
            const luaL_reg* m_Metatable;
            uint32_t* m_TypeHash;
        } types[] =
        {
            {BOX2D_TYPE_NAME_BODY, Body_methods, Body_meta, &TYPE_HASHES[BOX2D_TYPE_BODY]},
            // {SCRIPT_TYPE_NAME_VECTOR, Vector_methods, Vector_meta, &TYPE_HASHES[SCRIPT_TYPE_VECTOR]},
            // {SCRIPT_TYPE_NAME_VECTOR3, Vector3_methods, Vector3_meta, &TYPE_HASHES[SCRIPT_TYPE_VECTOR3]},
            // {SCRIPT_TYPE_NAME_VECTOR4, Vector4_methods, Vector4_meta, &TYPE_HASHES[SCRIPT_TYPE_VECTOR4]},
            // {SCRIPT_TYPE_NAME_QUAT, Quat_methods, Quat_meta, &TYPE_HASHES[SCRIPT_TYPE_QUAT]},
            // {SCRIPT_TYPE_NAME_MATRIX4, Matrix4_methods, Matrix4_meta, &TYPE_HASHES[SCRIPT_TYPE_MATRIX4]}
        };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(types); ++i)
        {
            //*types[i].m_TypeHash = dmScript::RegisterUserType(L, types[i].m_Name, types[i].m_Methods, types[i].m_Metatable);
            *types[i].m_TypeHash = dmScript::RegisterUserTypeLocal(L, types[i].m_Name, types[i].m_Metatable);
        }

        luaL_register(L, "box2d", BOX2D_FUNCTIONS);


// #define SETCONSTANT(name) \
//     lua_pushnumber(L, (lua_Number) dmPhysics::name); \
//     lua_setfield(L, -2, #name);\

//         SETCONSTANT(JOINT_TYPE_SPRING)
//         SETCONSTANT(JOINT_TYPE_FIXED)
//         SETCONSTANT(JOINT_TYPE_HINGE)
//         SETCONSTANT(JOINT_TYPE_SLIDER)
//         SETCONSTANT(JOINT_TYPE_WELD)
//         SETCONSTANT(JOINT_TYPE_WHEEL)

//  #undef SETCONSTANT

// #define SET_COLLISION_SHAPE_CONSTANT(name, enum_name) \
//     lua_pushnumber(L, (lua_Number) dmPhysicsDDF::CollisionShape::enum_name); \
//     lua_setfield(L, -2, #name);\

//         SET_COLLISION_SHAPE_CONSTANT(SHAPE_TYPE_SPHERE,  TYPE_SPHERE)
//         SET_COLLISION_SHAPE_CONSTANT(SHAPE_TYPE_BOX,     TYPE_BOX)
//         SET_COLLISION_SHAPE_CONSTANT(SHAPE_TYPE_CAPSULE, TYPE_CAPSULE)
//         SET_COLLISION_SHAPE_CONSTANT(SHAPE_TYPE_HULL,    TYPE_HULL)

// #undef SET_COLLISION_SHAPE_CONSTANT

        lua_pop(L, 1); // pop the lua module
    }

    void ScriptBox2DFinalize(const ScriptLibContext& context)
    {
        (void)context;
    }
}
