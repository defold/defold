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

#include <string.h>

#include <box2d/box2d.h>

#include <dlib/array.h>
#include <dmsdk/dlib/hashtable.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "gamesys_private.h"
#include "script_box2d_v3.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static uint32_t TYPE_HASH_JOINT = 0;

    #define BOX2D_TYPE_NAME_JOINT "b2joint"

    struct B2DLuaJoint
    {
        b2JointId                 m_Joint;
    };

    static dmHashTable64<uint8_t> g_JointIds;

    static uint64_t JointIdToKey(b2JointId joint_id)
    {
        return b2StoreJointId(joint_id);
    }

    static void EnsureJointIdCapacity()
    {
        if (g_JointIds.Full())
        {
            g_JointIds.OffsetCapacity(32);
        }
    }

    static void TrackJoint(b2JointId joint_id)
    {
        EnsureJointIdCapacity();
        g_JointIds.Put(JointIdToKey(joint_id), 1);
    }

    static void UntrackJoint(b2JointId joint_id)
    {
        if (g_JointIds.Get(JointIdToKey(joint_id)))
        {
            g_JointIds.Erase(JointIdToKey(joint_id));
        }
    }

    bool IsJointTracked(b2JointId joint_id)
    {
        return g_JointIds.Get(JointIdToKey(joint_id)) != 0;
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

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

    static bool HasField(lua_State* L, int table_index, const char* field)
    {
        if (table_index == 0)
        {
            return false;
        }
        lua_getfield(L, table_index, field);
        bool has_field = !lua_isnil(L, -1);
        lua_pop(L, 1);
        return has_field;
    }

    static bool GetBoolField(lua_State* L, int table_index, const char* field, bool default_value)
    {
        if (table_index == 0)
        {
            return default_value;
        }
        lua_getfield(L, table_index, field);
        bool value = lua_isnil(L, -1) ? default_value : lua_toboolean(L, -1);
        lua_pop(L, 1);
        return value;
    }

    static float GetNumberField(lua_State* L, int table_index, const char* field, float default_value)
    {
        if (table_index == 0)
        {
            return default_value;
        }
        lua_getfield(L, table_index, field);
        float value = lua_isnil(L, -1) ? default_value : (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
        return value;
    }

    static b2Vec2 GetVec2Field(lua_State* L, int table_index, const char* field, b2Vec2 default_value, float scale)
    {
        if (table_index == 0)
        {
            return default_value;
        }
        lua_getfield(L, table_index, field);
        b2Vec2 value = lua_isnil(L, -1) ? default_value : CheckVec2(L, -1, scale);
        lua_pop(L, 1);
        return value;
    }

    static void CheckJointDefBodies(lua_State* L, int body_a_index, int body_b_index, b2BodyId** body_a, b2BodyId** body_b, b2WorldId* world)
    {
        body_a_index = AbsIndex(L, body_a_index);
        body_b_index = AbsIndex(L, body_b_index);
        *body_a = CheckBody(L, body_a_index);
        *body_b = CheckBody(L, body_b_index);
        b2WorldId world_a = b2Body_GetWorld(**body_a);
        b2WorldId world_b = b2Body_GetWorld(**body_b);
        if (world_a.index1 != world_b.index1 || world_a.generation != world_b.generation)
        {
            luaL_error(L, "joints can only be connected to bodies in the same Box2D world.");
            return;
        }
        if (b2World_IsLocked(world_a))
        {
            luaL_error(L, "Could not create joint. The world is locked.");
            return;
        }
        *world = world_a;
    }

    static int CheckDefinitionTable(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            return 0;
        }

        luaL_checktype(L, index, LUA_TTABLE);
        return AbsIndex(L, index);
    }

    static b2Transform GetLocalFrame(lua_State* L, int index, b2Transform frame)
    {
        luaL_checktype(L, index, LUA_TTABLE);
        frame.p = GetVec2Field(L, index, "position", frame.p, GetPhysicsScale());
        if (HasField(L, index, "angle"))
        {
            frame.q = b2MakeRot(GetNumberField(L, index, "angle", 0.0f));
        }
        return frame;
    }

    static void PushLocalFrame(lua_State* L, b2Transform frame)
    {
        lua_newtable(L);
        dmScript::PushVector3(L, FromB2(frame.p, GetInvPhysicsScale()));
        lua_setfield(L, -2, "position");
        lua_pushnumber(L, b2Rot_GetAngle(frame.q));
        lua_setfield(L, -2, "angle");
    }

    static void ReadCommonJointDef(lua_State* L, int table_index, b2JointDef* def)
    {
        if (!table_index)
        {
            return;
        }
        lua_getfield(L, table_index, "local_frame_a");
        if (!lua_isnil(L, -1))
        {
            def->localFrameA = GetLocalFrame(L, AbsIndex(L, -1), def->localFrameA);
        }
        lua_pop(L, 1);
        lua_getfield(L, table_index, "local_frame_b");
        if (!lua_isnil(L, -1))
        {
            def->localFrameB = GetLocalFrame(L, AbsIndex(L, -1), def->localFrameB);
        }
        lua_pop(L, 1);
        def->collideConnected = GetBoolField(L, table_index, "collide_connected", def->collideConnected);
    }

    void PushJoint(lua_State* L, b2JointId joint_id)
    {
        B2DLuaJoint* luajoint = (B2DLuaJoint*)lua_newuserdata(L, sizeof(B2DLuaJoint));
        luajoint->m_Joint = joint_id;
        luaL_getmetatable(L, BOX2D_TYPE_NAME_JOINT);
        lua_setmetatable(L, -2);
    }

    static void PushCreatedJoint(lua_State* L, b2JointId joint_id)
    {
        TrackJoint(joint_id);
        PushJoint(L, joint_id);
    }

    static B2DLuaJoint* CheckJointInternal(lua_State* L, int index)
    {
        return (B2DLuaJoint*)dmScript::CheckUserType(L, index, TYPE_HASH_JOINT, "Expected user type " BOX2D_TYPE_NAME_JOINT);
    }

    static b2JointId CheckJoint(lua_State* L, int index)
    {
        B2DLuaJoint* luajoint = CheckJointInternal(L, index);
        if (!b2Joint_IsValid(luajoint->m_Joint))
        {
            luaL_error(L, "Invalid b2joint handle.");
            return b2_nullJointId;
        }
        return luajoint->m_Joint;
    }

    static b2JointId CheckJointType(lua_State* L, int index, b2JointType joint_type, const char* function_name, const char* joint_type_name)
    {
        b2JointId joint = CheckJoint(L, index);
        if (b2Joint_GetType(joint) != joint_type)
        {
            luaL_error(L, "b2d.joint.%s can only be used with %s joints.", function_name, joint_type_name);
            return b2_nullJointId;
        }
        return joint;
    }

    static void CheckJointWorldUnlocked(lua_State* L, b2JointId joint, const char* function_name)
    {
        if (b2World_IsLocked(b2Joint_GetWorld(joint)))
        {
            luaL_error(L, "Could not call b2d.joint.%s. The world is locked.", function_name);
        }
    }

    static b2JointId* ToJoint(lua_State* L, int index)
    {
        B2DLuaJoint* luajoint = (B2DLuaJoint*)dmScript::ToUserType(L, index, TYPE_HASH_JOINT);
        return luajoint ? &luajoint->m_Joint : 0;
    }

    static int Joint_Destroy(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        if (!b2Joint_IsValid(luajoint->m_Joint))
        {
            return luaL_error(L, "Invalid b2joint handle.");
        }
        if (!IsJointTracked(luajoint->m_Joint))
        {
            return luaL_error(L, "Cannot destroy a joint not created by b2d.joint.");
        }
        if (b2World_IsLocked(b2Joint_GetWorld(luajoint->m_Joint)))
        {
            return luaL_error(L, "Could not destroy joint. The world is locked.");
        }
        UntrackJoint(luajoint->m_Joint);
        b2DestroyJoint(luajoint->m_Joint);
        luajoint->m_Joint = b2_nullJointId;
        return 0;
    }

    static int Joint_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        lua_pushboolean(L, b2Joint_IsValid(luajoint->m_Joint));
        return 1;
    }

    static int Joint_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushWorldId(L, b2Joint_GetWorld(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Joint_GetType(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_GetBodyA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        b2BodyId body = b2Joint_GetBodyA(joint);
        PushBody(L, &body, GetBodyInstance(body));
        return 1;
    }

    static int Joint_GetBodyB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        b2BodyId body = b2Joint_GetBodyB(joint);
        PushBody(L, &body, GetBodyInstance(body));
        return 1;
    }

    static int Joint_GetLocalFrameA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushLocalFrame(L, b2Joint_GetLocalFrameA(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_GetLocalFrameB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushLocalFrame(L, b2Joint_GetLocalFrameB(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_SetLocalFrameA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_local_frame_a");
        b2Joint_SetLocalFrameA(joint, GetLocalFrame(L, 2, b2Joint_GetLocalFrameA(joint)));
        return 0;
    }

    static int Joint_SetLocalFrameB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_local_frame_b");
        b2Joint_SetLocalFrameB(joint, GetLocalFrame(L, 2, b2Joint_GetLocalFrameB(joint)));
        return 0;
    }

    static int Joint_GetAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPoint(b2Joint_GetBodyA(joint), b2Joint_GetLocalFrameA(joint).p), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPoint(b2Joint_GetBodyB(joint), b2Joint_GetLocalFrameB(joint).p), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Joint_GetCollideConnected(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_SetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint_SetCollideConnected(CheckJoint(L, 1), lua_toboolean(L, 2));
        return 0;
    }

    static int Joint_WakeBodies(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint_WakeBodies(CheckJoint(L, 1));
        return 0;
    }

    static int Joint_SetConstraintTuning(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_constraint_tuning");
        b2Joint_SetConstraintTuning(joint, luaL_checknumber(L, 2), luaL_checknumber(L, 3));
        return 0;
    }

    static int Joint_GetConstraintTuning(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        float hertz;
        float damping_ratio;
        b2Joint_GetConstraintTuning(CheckJoint(L, 1), &hertz, &damping_ratio);
        lua_pushnumber(L, hertz);
        lua_pushnumber(L, damping_ratio);
        return 2;
    }

    static int Joint_SetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_length");
        b2DistanceJoint_SetRestLength(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetRestLength(CheckJointType(L, 1, b2_distanceJoint, "get_length", "distance")) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_EnableSpring(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "enable_spring");
        bool enable_spring = lua_toboolean(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_EnableSpring(joint, enable_spring); break;
            case b2_prismaticJoint: b2PrismaticJoint_EnableSpring(joint, enable_spring); break;
            case b2_revoluteJoint: b2RevoluteJoint_EnableSpring(joint, enable_spring); break;
            case b2_wheelJoint: b2WheelJoint_EnableSpring(joint, enable_spring); break;
            default: return luaL_error(L, "b2d.joint.enable_spring can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_IsSpringEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushboolean(L, b2DistanceJoint_IsSpringEnabled(joint)); break;
            case b2_prismaticJoint: lua_pushboolean(L, b2PrismaticJoint_IsSpringEnabled(joint)); break;
            case b2_revoluteJoint: lua_pushboolean(L, b2RevoluteJoint_IsSpringEnabled(joint)); break;
            case b2_wheelJoint: lua_pushboolean(L, b2WheelJoint_IsSpringEnabled(joint)); break;
            default: return luaL_error(L, "b2d.joint.is_spring_enabled can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetSpringHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_spring_hertz");
        float hertz = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_SetSpringHertz(joint, hertz); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetSpringHertz(joint, hertz); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetSpringHertz(joint, hertz); break;
            case b2_wheelJoint: b2WheelJoint_SetSpringHertz(joint, hertz); break;
            default: return luaL_error(L, "b2d.joint.set_spring_hertz can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetSpringHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetSpringHertz(joint)); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetSpringHertz(joint)); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetSpringHertz(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetSpringHertz(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_spring_hertz can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetSpringDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_spring_damping_ratio");
        float damping_ratio = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_wheelJoint: b2WheelJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            default: return luaL_error(L, "b2d.joint.set_spring_damping_ratio can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetSpringDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetSpringDampingRatio(joint)); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetSpringDampingRatio(joint)); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetSpringDampingRatio(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetSpringDampingRatio(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_spring_damping_ratio can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetLengthRange(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_length_range", "distance");
        CheckJointWorldUnlocked(L, joint, "set_length_range");
        b2DistanceJoint_SetLimits(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), (float)luaL_checknumber(L, 3) * GetPhysicsScale());
        return 0;
    }

    static int Joint_SetMinLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_min_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_min_length");
        b2DistanceJoint_SetLimits(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), b2DistanceJoint_GetUpperLimit(joint));
        return 0;
    }

    static int Joint_GetMinLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetLowerLimit(CheckJointType(L, 1, b2_distanceJoint, "get_min_length", "distance")) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_max_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_max_length");
        b2DistanceJoint_SetLimits(joint, b2DistanceJoint_GetLowerLimit(joint), (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetUpperLimit(CheckJointType(L, 1, b2_distanceJoint, "get_max_length", "distance")) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_GetCurrentLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetCurrentLength(CheckJointType(L, 1, b2_distanceJoint, "get_current_length", "distance")) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_EnableLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "enable_limit");
        bool enable_limit = lua_toboolean(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_EnableLimit(joint, enable_limit); break;
            case b2_prismaticJoint: b2PrismaticJoint_EnableLimit(joint, enable_limit); break;
            case b2_revoluteJoint: b2RevoluteJoint_EnableLimit(joint, enable_limit); break;
            case b2_wheelJoint: b2WheelJoint_EnableLimit(joint, enable_limit); break;
            default: return luaL_error(L, "b2d.joint.enable_limit can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_IsLimitEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushboolean(L, b2DistanceJoint_IsLimitEnabled(joint)); break;
            case b2_prismaticJoint: lua_pushboolean(L, b2PrismaticJoint_IsLimitEnabled(joint)); break;
            case b2_revoluteJoint: lua_pushboolean(L, b2RevoluteJoint_IsLimitEnabled(joint)); break;
            case b2_wheelJoint: lua_pushboolean(L, b2WheelJoint_IsLimitEnabled(joint)); break;
            default: return luaL_error(L, "b2d.joint.is_limit_enabled can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_limits");
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint:
                b2PrismaticJoint_SetLimits(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), (float)luaL_checknumber(L, 3) * GetPhysicsScale());
                break;
            case b2_revoluteJoint:
                b2RevoluteJoint_SetLimits(joint, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
                break;
            case b2_wheelJoint:
                b2WheelJoint_SetLimits(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), (float)luaL_checknumber(L, 3) * GetPhysicsScale());
                break;
            default: return luaL_error(L, "b2d.joint.set_limits can only be used with prismatic, revolute, or wheel joints. Use set_length_range for distance joints.");
        }
        return 0;
    }

    static int Joint_GetLowerLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetLowerLimit(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetLowerLimit(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetLowerLimit(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_lower_limit can only be used with prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetUpperLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetUpperLimit(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetUpperLimit(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetUpperLimit(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_upper_limit can only be used with prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_EnableMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "enable_motor");
        bool enable_motor = lua_toboolean(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_EnableMotor(joint, enable_motor); break;
            case b2_prismaticJoint: b2PrismaticJoint_EnableMotor(joint, enable_motor); break;
            case b2_revoluteJoint: b2RevoluteJoint_EnableMotor(joint, enable_motor); break;
            case b2_wheelJoint: b2WheelJoint_EnableMotor(joint, enable_motor); break;
            default: return luaL_error(L, "b2d.joint.enable_motor can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_IsMotorEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushboolean(L, b2DistanceJoint_IsMotorEnabled(joint)); break;
            case b2_prismaticJoint: lua_pushboolean(L, b2PrismaticJoint_IsMotorEnabled(joint)); break;
            case b2_revoluteJoint: lua_pushboolean(L, b2RevoluteJoint_IsMotorEnabled(joint)); break;
            case b2_wheelJoint: lua_pushboolean(L, b2WheelJoint_IsMotorEnabled(joint)); break;
            default: return luaL_error(L, "b2d.joint.is_motor_enabled can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMotorSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_motor_speed");
        float motor_speed = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_SetTargetMotorSpeed(joint, motor_speed * GetPhysicsScale()); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetTargetMotorSpeed(joint, motor_speed * GetPhysicsScale()); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetTargetMotorSpeed(joint, motor_speed); break;
            case b2_wheelJoint: b2WheelJoint_SetTargetMotorSpeed(joint, motor_speed); break;
            default: return luaL_error(L, "b2d.joint.set_motor_speed can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetMotorSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetTargetMotorSpeed(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetTargetMotorSpeed(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetTargetMotorSpeed(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetTargetMotorSpeed(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_motor_speed can only be used with distance, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMaxMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_max_motor_force");
        float max_motor_force = (float)luaL_checknumber(L, 2) * GetPhysicsScale();
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: b2DistanceJoint_SetMaxMotorForce(joint, max_motor_force); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetMaxMotorForce(joint, max_motor_force); break;
            default: return luaL_error(L, "b2d.joint.set_max_motor_force can only be used with distance or prismatic joints.");
        }
        return 0;
    }

    static int Joint_GetMaxMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetMaxMotorForce(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetMaxMotorForce(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_max_motor_force can only be used with distance or prismatic joints.");
        }
        return 1;
    }

    static int Joint_GetMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetCurrentMotorForce(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetCurrentMotorForce(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_motor_force can only be used with distance or prismatic joints.");
        }
        return 1;
    }

    static int Joint_SetMaxMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_max_motor_torque");
        float max_motor_torque = (float)luaL_checknumber(L, 2) * GetPhysicsScale();
        switch (b2Joint_GetType(joint))
        {
            case b2_revoluteJoint: b2RevoluteJoint_SetMaxMotorTorque(joint, max_motor_torque); break;
            case b2_wheelJoint: b2WheelJoint_SetMaxMotorTorque(joint, max_motor_torque); break;
            default: return luaL_error(L, "b2d.joint.set_max_motor_torque can only be used with revolute or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetMaxMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetMaxMotorTorque(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetMaxMotorTorque(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_max_motor_torque can only be used with revolute or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetCurrentMotorTorque(joint) * (lua_Number)GetInvPhysicsScale()); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetCurrentMotorTorque(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_motor_torque can only be used with revolute or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMaxVelocityTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_max_velocity_torque", "motor");
        CheckJointWorldUnlocked(L, joint, "set_max_velocity_torque");
        b2MotorJoint_SetMaxVelocityTorque(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxVelocityTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2MotorJoint_GetMaxVelocityTorque(CheckJointType(L, 1, b2_motorJoint, "get_max_velocity_torque", "motor")) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxVelocityForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_max_velocity_force", "motor");
        CheckJointWorldUnlocked(L, joint, "set_max_velocity_force");
        b2MotorJoint_SetMaxVelocityForce(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxVelocityForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "get_max_velocity_force", "motor");
        lua_pushnumber(L, b2MotorJoint_GetMaxVelocityForce(joint) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxSpringForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_max_spring_force", "motor");
        CheckJointWorldUnlocked(L, joint, "set_max_spring_force");
        b2MotorJoint_SetMaxSpringForce(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxSpringForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "get_max_spring_force", "motor");
        lua_pushnumber(L, b2MotorJoint_GetMaxSpringForce(joint) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxSpringTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_max_spring_torque", "motor");
        CheckJointWorldUnlocked(L, joint, "set_max_spring_torque");
        b2MotorJoint_SetMaxSpringTorque(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxSpringTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "get_max_spring_torque", "motor");
        lua_pushnumber(L, b2MotorJoint_GetMaxSpringTorque(joint) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_linear_velocity", "motor");
        CheckJointWorldUnlocked(L, joint, "set_linear_velocity");
        b2MotorJoint_SetLinearVelocity(joint, CheckVec2(L, 2, GetPhysicsScale()));
        return 0;
    }

    static int Joint_GetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "get_linear_velocity", "motor");
        dmScript::PushVector3(L, FromB2(b2MotorJoint_GetLinearVelocity(joint), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_SetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_angular_velocity", "motor");
        CheckJointWorldUnlocked(L, joint, "set_angular_velocity");
        b2MotorJoint_SetAngularVelocity(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "get_angular_velocity", "motor");
        lua_pushnumber(L, b2MotorJoint_GetAngularVelocity(joint));
        return 1;
    }

    static int Joint_GetJointTranslation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetCurrentTranslation(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_joint_translation can only be used with prismatic joints.");
        }
        return 1;
    }

    static int Joint_GetJointSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetCurrentSpeed(joint) * (lua_Number)GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_joint_speed can only be used with prismatic joints.");
        }
        return 1;
    }

    static int Joint_GetJointAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2RevoluteJoint_GetCurrentAngle(CheckJointType(L, 1, b2_revoluteJoint, "get_joint_angle", "revolute")));
        return 1;
    }

    static int Joint_SetLinearHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_linear_hertz");
        float hertz = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: b2WeldJoint_SetLinearHertz(joint, hertz); break;
            case b2_motorJoint: b2MotorJoint_SetLinearHertz(joint, hertz); break;
            default: return luaL_error(L, "b2d.joint.set_linear_hertz can only be used with weld or motor joints.");
        }
        return 0;
    }

    static int Joint_GetLinearHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: lua_pushnumber(L, b2WeldJoint_GetLinearHertz(joint)); break;
            case b2_motorJoint: lua_pushnumber(L, b2MotorJoint_GetLinearHertz(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_linear_hertz can only be used with weld or motor joints.");
        }
        return 1;
    }

    static int Joint_SetLinearDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_linear_damping_ratio");
        float damping_ratio = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: b2WeldJoint_SetLinearDampingRatio(joint, damping_ratio); break;
            case b2_motorJoint: b2MotorJoint_SetLinearDampingRatio(joint, damping_ratio); break;
            default: return luaL_error(L, "b2d.joint.set_linear_damping_ratio can only be used with weld or motor joints.");
        }
        return 0;
    }

    static int Joint_GetLinearDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: lua_pushnumber(L, b2WeldJoint_GetLinearDampingRatio(joint)); break;
            case b2_motorJoint: lua_pushnumber(L, b2MotorJoint_GetLinearDampingRatio(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_linear_damping_ratio can only be used with weld or motor joints.");
        }
        return 1;
    }

    static int Joint_SetAngularHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_angular_hertz");
        float hertz = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: b2WeldJoint_SetAngularHertz(joint, hertz); break;
            case b2_motorJoint: b2MotorJoint_SetAngularHertz(joint, hertz); break;
            default: return luaL_error(L, "b2d.joint.set_angular_hertz can only be used with weld or motor joints.");
        }
        return 0;
    }

    static int Joint_GetAngularHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: lua_pushnumber(L, b2WeldJoint_GetAngularHertz(joint)); break;
            case b2_motorJoint: lua_pushnumber(L, b2MotorJoint_GetAngularHertz(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_angular_hertz can only be used with weld or motor joints.");
        }
        return 1;
    }

    static int Joint_SetAngularDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_angular_damping_ratio");
        float damping_ratio = (float)luaL_checknumber(L, 2);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: b2WeldJoint_SetAngularDampingRatio(joint, damping_ratio); break;
            case b2_motorJoint: b2MotorJoint_SetAngularDampingRatio(joint, damping_ratio); break;
            default: return luaL_error(L, "b2d.joint.set_angular_damping_ratio can only be used with weld or motor joints.");
        }
        return 0;
    }

    static int Joint_GetAngularDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_weldJoint: lua_pushnumber(L, b2WeldJoint_GetAngularDampingRatio(joint)); break;
            case b2_motorJoint: lua_pushnumber(L, b2MotorJoint_GetAngularDampingRatio(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_angular_damping_ratio can only be used with weld or motor joints.");
        }
        return 1;
    }

    static int Joint_GetReactionForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(b2Joint_GetConstraintForce(CheckJoint(L, 1)), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetReactionTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Joint_GetConstraintTorque(CheckJoint(L, 1)) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int Joint_CreateDistance(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2DistanceJointDef def = b2DefaultDistanceJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        def.length = GetNumberField(L, def_index, "length", def.length) * GetPhysicsScale();
        def.minLength = GetNumberField(L, def_index, "min_length", def.minLength) * GetPhysicsScale();
        def.maxLength = GetNumberField(L, def_index, "max_length", def.maxLength) * GetPhysicsScale();
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorForce = GetNumberField(L, def_index, "max_motor_force", def.maxMotorForce) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed) * GetPhysicsScale();

        b2JointId joint = b2CreateDistanceJoint(world, &def);
        PushCreatedJoint(L, joint);
        return 1;
    }

    static int Joint_CreatePrismatic(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerTranslation = GetNumberField(L, def_index, "lower_translation", def.lowerTranslation) * GetPhysicsScale();
        def.upperTranslation = GetNumberField(L, def_index, "upper_translation", def.upperTranslation) * GetPhysicsScale();
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorForce = GetNumberField(L, def_index, "max_motor_force", def.maxMotorForce) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed) * GetPhysicsScale();

        PushCreatedJoint(L, b2CreatePrismaticJoint(world, &def));
        return 1;
    }

    static int Joint_CreateRevolute(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerAngle = GetNumberField(L, def_index, "lower_angle", def.lowerAngle);
        def.upperAngle = GetNumberField(L, def_index, "upper_angle", def.upperAngle);
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);

        PushCreatedJoint(L, b2CreateRevoluteJoint(world, &def));
        return 1;
    }

    static int Joint_CreateWeld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WeldJointDef def = b2DefaultWeldJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        float hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.linearHertz);
        float damping = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.linearDampingRatio);
        def.linearHertz = GetNumberField(L, def_index, "linear_hertz", hertz);
        def.angularHertz = GetNumberField(L, def_index, "angular_hertz", hertz);
        def.linearDampingRatio = GetNumberField(L, def_index, "linear_damping_ratio", damping);
        def.angularDampingRatio = GetNumberField(L, def_index, "angular_damping_ratio", damping);

        PushCreatedJoint(L, b2CreateWeldJoint(world, &def));
        return 1;
    }

    static int Joint_CreateWheel(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WheelJointDef def = b2DefaultWheelJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerTranslation = GetNumberField(L, def_index, "lower_translation", def.lowerTranslation) * GetPhysicsScale();
        def.upperTranslation = GetNumberField(L, def_index, "upper_translation", def.upperTranslation) * GetPhysicsScale();
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);

        PushCreatedJoint(L, b2CreateWheelJoint(world, &def));
        return 1;
    }

    static int Joint_CreateMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2MotorJointDef def = b2DefaultMotorJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;
        ReadCommonJointDef(L, def_index, &def.base);
        def.linearVelocity = GetVec2Field(L, def_index, "linear_velocity", def.linearVelocity, GetPhysicsScale());
        def.angularVelocity = GetNumberField(L, def_index, "angular_velocity", def.angularVelocity);
        def.maxVelocityForce = GetNumberField(L, def_index, "max_velocity_force", def.maxVelocityForce) * GetPhysicsScale();
        def.maxVelocityTorque = GetNumberField(L, def_index, "max_velocity_torque", def.maxVelocityTorque) * GetPhysicsScale();
        def.linearHertz = GetNumberField(L, def_index, "linear_hertz", def.linearHertz);
        def.linearDampingRatio = GetNumberField(L, def_index, "linear_damping_ratio", def.linearDampingRatio);
        def.angularHertz = GetNumberField(L, def_index, "angular_hertz", def.angularHertz);
        def.angularDampingRatio = GetNumberField(L, def_index, "angular_damping_ratio", def.angularDampingRatio);
        def.maxSpringForce = GetNumberField(L, def_index, "max_spring_force", def.maxSpringForce) * GetPhysicsScale();
        def.maxSpringTorque = GetNumberField(L, def_index, "max_spring_torque", def.maxSpringTorque) * GetPhysicsScale();

        PushCreatedJoint(L, b2CreateMotorJoint(world, &def));
        return 1;
    }

    static int Joint_CreateFilter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);

        b2FilterJointDef def = b2DefaultFilterJointDef();
        def.base.bodyIdA = *body_a;
        def.base.bodyIdB = *body_b;

        PushCreatedJoint(L, b2CreateFilterJoint(world, &def));
        return 1;
    }

    static int Joint_tostring(lua_State* L)
    {
        b2JointId joint = CheckJoint(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_JOINT, &joint);
        return 1;
    }

    static int Joint_eq(lua_State* L)
    {
        b2JointId* a = ToJoint(L, 1);
        b2JointId* b = ToJoint(L, 2);
        lua_pushboolean(L, a && b && memcmp(a, b, sizeof(b2JointId)) == 0);
        return 1;
    }

    static const luaL_reg Joint_methods[] =
    {
        {0, 0}
    };

    static const luaL_reg Joint_meta[] =
    {
        {"__tostring", Joint_tostring},
        {"__eq", Joint_eq},
        {0, 0}
    };

    static const luaL_reg Joint_functions[] =
    {
        {"destroy", Joint_Destroy},
        {"is_valid", Joint_IsValid},
        {"get_world", Joint_GetWorld},
        {"get_type", Joint_GetType},
        {"get_body_a", Joint_GetBodyA},
        {"get_body_b", Joint_GetBodyB},
        {"get_local_frame_a", Joint_GetLocalFrameA},
        {"get_local_frame_b", Joint_GetLocalFrameB},
        {"set_local_frame_a", Joint_SetLocalFrameA},
        {"set_local_frame_b", Joint_SetLocalFrameB},
        {"get_anchor_a", Joint_GetAnchorA},
        {"get_anchor_b", Joint_GetAnchorB},
        {"get_collide_connected", Joint_GetCollideConnected},
        {"set_collide_connected", Joint_SetCollideConnected},
        {"wake_bodies", Joint_WakeBodies},
        {"set_constraint_tuning", Joint_SetConstraintTuning},
        {"get_constraint_tuning", Joint_GetConstraintTuning},
        {"set_length", Joint_SetLength},
        {"get_length", Joint_GetLength},
        {"enable_spring", Joint_EnableSpring},
        {"is_spring_enabled", Joint_IsSpringEnabled},
        {"set_spring_hertz", Joint_SetSpringHertz},
        {"get_spring_hertz", Joint_GetSpringHertz},
        {"set_hertz", Joint_SetSpringHertz},
        {"get_hertz", Joint_GetSpringHertz},
        {"set_frequency", Joint_SetSpringHertz},
        {"get_frequency", Joint_GetSpringHertz},
        {"set_spring_damping_ratio", Joint_SetSpringDampingRatio},
        {"get_spring_damping_ratio", Joint_GetSpringDampingRatio},
        {"set_damping_ratio", Joint_SetSpringDampingRatio},
        {"get_damping_ratio", Joint_GetSpringDampingRatio},
        {"set_length_range", Joint_SetLengthRange},
        {"set_min_length", Joint_SetMinLength},
        {"get_min_length", Joint_GetMinLength},
        {"set_max_length", Joint_SetMaxLength},
        {"get_max_length", Joint_GetMaxLength},
        {"get_current_length", Joint_GetCurrentLength},
        {"enable_limit", Joint_EnableLimit},
        {"is_limit_enabled", Joint_IsLimitEnabled},
        {"set_limits", Joint_SetLimits},
        {"get_lower_limit", Joint_GetLowerLimit},
        {"get_upper_limit", Joint_GetUpperLimit},
        {"enable_motor", Joint_EnableMotor},
        {"is_motor_enabled", Joint_IsMotorEnabled},
        {"set_motor_speed", Joint_SetMotorSpeed},
        {"get_motor_speed", Joint_GetMotorSpeed},
        {"set_max_motor_force", Joint_SetMaxMotorForce},
        {"get_max_motor_force", Joint_GetMaxMotorForce},
        {"get_motor_force", Joint_GetMotorForce},
        {"set_max_motor_torque", Joint_SetMaxMotorTorque},
        {"get_max_motor_torque", Joint_GetMaxMotorTorque},
        {"get_motor_torque", Joint_GetMotorTorque},
        {"set_max_velocity_torque", Joint_SetMaxVelocityTorque},
        {"get_max_velocity_torque", Joint_GetMaxVelocityTorque},
        {"set_max_velocity_force", Joint_SetMaxVelocityForce},
        {"get_max_velocity_force", Joint_GetMaxVelocityForce},
        {"set_max_spring_force", Joint_SetMaxSpringForce},
        {"get_max_spring_force", Joint_GetMaxSpringForce},
        {"set_max_spring_torque", Joint_SetMaxSpringTorque},
        {"get_max_spring_torque", Joint_GetMaxSpringTorque},
        {"set_linear_velocity", Joint_SetLinearVelocity},
        {"get_linear_velocity", Joint_GetLinearVelocity},
        {"set_angular_velocity", Joint_SetAngularVelocity},
        {"get_angular_velocity", Joint_GetAngularVelocity},
        {"get_joint_translation", Joint_GetJointTranslation},
        {"get_joint_speed", Joint_GetJointSpeed},
        {"get_joint_angle", Joint_GetJointAngle},
        {"set_linear_hertz", Joint_SetLinearHertz},
        {"get_linear_hertz", Joint_GetLinearHertz},
        {"set_linear_damping_ratio", Joint_SetLinearDampingRatio},
        {"get_linear_damping_ratio", Joint_GetLinearDampingRatio},
        {"set_angular_hertz", Joint_SetAngularHertz},
        {"get_angular_hertz", Joint_GetAngularHertz},
        {"set_angular_damping_ratio", Joint_SetAngularDampingRatio},
        {"get_angular_damping_ratio", Joint_GetAngularDampingRatio},
        {"get_reaction_force", Joint_GetReactionForce},
        {"get_reaction_torque", Joint_GetReactionTorque},
        {"create_distance", Joint_CreateDistance},
        {"create_prismatic", Joint_CreatePrismatic},
        {"create_revolute", Joint_CreateRevolute},
        {"create_weld", Joint_CreateWeld},
        {"create_wheel", Joint_CreateWheel},
        {"create_motor", Joint_CreateMotor},
        {"create_filter", Joint_CreateFilter},
        {0, 0}
    };

    void ScriptBox2DInitializeJoint(lua_State* L)
    {
        TYPE_HASH_JOINT = dmScript::RegisterUserType(L, BOX2D_TYPE_NAME_JOINT, Joint_methods, Joint_meta);

        lua_newtable(L);
        luaL_register(L, 0, Joint_functions);

#define SET_CONSTANT(NAME, CUSTOM_NAME) \
        lua_pushnumber(L, (lua_Number)NAME); \
        lua_setfield(L, -2, CUSTOM_NAME);

        SET_CONSTANT(b2_distanceJoint, "JOINT_TYPE_DISTANCE");
        SET_CONSTANT(b2_filterJoint, "JOINT_TYPE_FILTER");
        SET_CONSTANT(b2_motorJoint, "JOINT_TYPE_MOTOR");
        SET_CONSTANT(b2_prismaticJoint, "JOINT_TYPE_PRISMATIC");
        SET_CONSTANT(b2_revoluteJoint, "JOINT_TYPE_REVOLUTE");
        SET_CONSTANT(b2_weldJoint, "JOINT_TYPE_WELD");
        SET_CONSTANT(b2_wheelJoint, "JOINT_TYPE_WHEEL");

#undef SET_CONSTANT

        lua_setfield(L, -2, "joint");
    }

    void ScriptBox2DFinalizeJoint()
    {
        TYPE_HASH_JOINT = 0;
        if (g_JointIds.Capacity() > 0)
        {
            g_JointIds.Clear();
        }
    }
}

/*# Box2D b2Joint documentation
 *
 * Functions for interacting with native Box2D joints.
 *
 * @document
 * @name b2d.joint
 * @namespace b2d.joint
 * @language Lua
 * @version 3
 */

/*# Box2D joint
 *
 * An opaque handle connecting two [type:b2Body] values. Create one with a
 * function such as [ref:b2d.joint.create_distance], use the functions in
 * `b2d.joint` to inspect or configure it, and release it with
 * [ref:b2d.joint.destroy]. Use [ref:b2d.joint.is_valid] before retaining a
 * handle across body or world destruction.
 *
 * @typedef
 * @name b2Joint
 * @param value [type:userdata] Box2D joint handle
 * @examples
 *
 * ```lua
 * local body_a = b2d.get_body("#collisionobject_a")
 * local body_b = b2d.get_body("#collisionobject_b")
 * local joint = b2d.joint.create_distance(body_a, body_b)
 * if b2d.joint.is_valid(joint) then
 *     print(b2d.joint.get_type(joint))
 * end
 * ```
 */

/*# Destroy a joint created by `b2d.joint`.
 * @name b2d.joint.destroy
 * @param joint [type: b2Joint] joint
 */

/*# Validate a joint handle.
 * @name b2d.joint.is_valid
 * @param joint [type: b2Joint] joint
 * @return valid [type: boolean] true if the joint handle still refers to a live Box2D joint
 */

/*# Get the world owning a joint.
 * @name b2d.joint.get_world
 * @param joint [type: b2Joint] joint
 * @return world [type: b2World] owning world
 */

/*# Get the joint type.
 * @name b2d.joint.get_type
 * @param joint [type: b2Joint] joint
 * @return type [type:b2d.joint.JOINT_TYPE] one of the `JOINT_TYPE_*` constants
 */

/*# Get the first body connected to a joint.
 * @name b2d.joint.get_body_a
 * @param joint [type: b2Joint] joint
 * @return body [type: b2Body] body A
 */

/*# Get the second body connected to a joint.
 * @name b2d.joint.get_body_b
 * @param joint [type: b2Joint] joint
 * @return body [type: b2Body] body B
 */

/*# Get the local frame on body A.
 * @name b2d.joint.get_local_frame_a
 * @param joint [type: b2Joint] joint
 * @return frame [type:b2d.transform] Local frame with position (vector3) and angle (radians).
 */

/*# Get the local frame on body B.
 * @name b2d.joint.get_local_frame_b
 * @param joint [type: b2Joint] joint
 * @return frame [type:b2d.transform] Local frame with position (vector3) and angle (radians).
 */

/*# Get the world anchor on body A.
 * @name b2d.joint.get_anchor_a
 * @param joint [type: b2Joint] joint
 * @return anchor [type: vector3] world anchor
 */

/*# Get the world anchor on body B.
 * @name b2d.joint.get_anchor_b
 * @param joint [type: b2Joint] joint
 * @return anchor [type: vector3] world anchor
 */

/*# Get whether connected bodies can collide.
 * @name b2d.joint.get_collide_connected
 * @param joint [type: b2Joint] joint
 * @return collide [type: boolean] true if connected bodies can collide
 */

/*# Set whether connected bodies can collide.
 * @name b2d.joint.set_collide_connected
 * @param joint [type: b2Joint] joint
 * @param collide [type: boolean] true if connected bodies can collide
 */

/*# Wake the bodies connected to a joint.
 * @name b2d.joint.wake_bodies
 * @param joint [type: b2Joint] joint
 */

/*# Set the distance joint length.
 * @name b2d.joint.set_length
 * @param joint [type: b2Joint] distance joint
 * @param length [type: number] length in project units
 */

/*# Get the distance joint length.
 * @name b2d.joint.get_length
 * @param joint [type: b2Joint] distance joint
 * @return length [type: number] length in project units
 */

/*# Enable or disable joint spring behavior.
 * @name b2d.joint.enable_spring
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param enable [type: boolean] true to enable the spring
 */

/*# Get whether joint spring behavior is enabled.
 * @name b2d.joint.is_spring_enabled
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return enabled [type: boolean] true if the spring is enabled
 */

/*# Set spring frequency.
 * @name b2d.joint.set_spring_hertz
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get spring frequency.
 * @name b2d.joint.get_spring_hertz
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.set_spring_hertz`.
 * @name b2d.joint.set_hertz
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.get_spring_hertz`.
 * @name b2d.joint.get_hertz
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.set_spring_hertz`.
 * @name b2d.joint.set_frequency
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param frequency [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.get_spring_hertz`.
 * @name b2d.joint.get_frequency
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return frequency [type: number] frequency in hertz
 */

/*# Set spring damping ratio.
 * @name b2d.joint.set_spring_damping_ratio
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Get spring damping ratio.
 * @name b2d.joint.get_spring_damping_ratio
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.set_spring_damping_ratio`.
 * @name b2d.joint.set_damping_ratio
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.get_spring_damping_ratio`.
 * @name b2d.joint.get_damping_ratio
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return ratio [type: number] damping ratio
 */

/*# Set the distance joint length range.
 * @name b2d.joint.set_length_range
 * @param joint [type: b2Joint] distance joint
 * @param min_length [type: number] minimum length in project units
 * @param max_length [type: number] maximum length in project units
 */

/*# Set the distance joint minimum length.
 * @name b2d.joint.set_min_length
 * @param joint [type: b2Joint] distance joint
 * @param length [type: number] minimum length in project units
 */

/*# Get the distance joint minimum length.
 * @name b2d.joint.get_min_length
 * @param joint [type: b2Joint] distance joint
 * @return length [type: number] minimum length in project units
 */

/*# Set the distance joint maximum length.
 * @name b2d.joint.set_max_length
 * @param joint [type: b2Joint] distance joint
 * @param length [type: number] maximum length in project units
 */

/*# Get the distance joint maximum length.
 * @name b2d.joint.get_max_length
 * @param joint [type: b2Joint] distance joint
 * @return length [type: number] maximum length in project units
 */

/*# Get the current distance joint length.
 * @name b2d.joint.get_current_length
 * @param joint [type: b2Joint] distance joint
 * @return length [type: number] current length in project units
 */

/*# Enable or disable joint limits.
 * @name b2d.joint.enable_limit
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param enable [type: boolean] true to enable limits
 */

/*# Get whether joint limits are enabled.
 * @name b2d.joint.is_limit_enabled
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return enabled [type: boolean] true if limits are enabled
 */

/*# Set joint limits.
 * @name b2d.joint.set_limits
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @param lower [type: number] lower limit
 * @param upper [type: number] upper limit
 */

/*# Get the lower joint limit.
 * @name b2d.joint.get_lower_limit
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @return lower [type: number] lower limit
 */

/*# Get the upper joint limit.
 * @name b2d.joint.get_upper_limit
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @return upper [type: number] upper limit
 */

/*# Enable or disable the joint motor.
 * @name b2d.joint.enable_motor
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param enable [type: boolean] true to enable the motor
 */

/*# Get whether the joint motor is enabled.
 * @name b2d.joint.is_motor_enabled
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return enabled [type: boolean] true if the motor is enabled
 */

/*# Set motor speed.
 * @name b2d.joint.set_motor_speed
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @param speed [type: number] motor speed
 */

/*# Get motor speed.
 * @name b2d.joint.get_motor_speed
 * @param joint [type: b2Joint] distance, prismatic, revolute, or wheel joint
 * @return speed [type: number] motor speed
 */

/*# Set maximum motor force.
 * @name b2d.joint.set_max_motor_force
 * @param joint [type: b2Joint] distance or prismatic joint
 * @param force [type: number] maximum motor force
 */

/*# Get maximum motor force.
 * @name b2d.joint.get_max_motor_force
 * @param joint [type: b2Joint] distance or prismatic joint
 * @return force [type: number] maximum motor force
 */

/*# Get current motor force.
 * @name b2d.joint.get_motor_force
 * @param joint [type: b2Joint] distance or prismatic joint
 * @return force [type: number] motor force
 */

/*# Set maximum motor torque.
 * @name b2d.joint.set_max_motor_torque
 * @param joint [type: b2Joint] revolute or wheel joint
 * @param torque [type: number] maximum motor torque
 */

/*# Get maximum motor torque.
 * @name b2d.joint.get_max_motor_torque
 * @param joint [type: b2Joint] revolute or wheel joint
 * @return torque [type: number] maximum motor torque
 */

/*# Get current motor torque.
 * @name b2d.joint.get_motor_torque
 * @param joint [type: b2Joint] revolute or wheel joint
 * @return torque [type: number] motor torque
 */

/*# Set maximum torque.
 * @name b2d.joint.set_max_velocity_torque
 * @param joint [type: b2Joint] motor joint
 * @param torque [type: number] maximum torque
 */

/*# Get maximum torque.
 * @name b2d.joint.get_max_velocity_torque
 * @param joint [type: b2Joint] motor joint
 * @return torque [type: number] maximum torque
 */

/*# Get joint translation.
 * @name b2d.joint.get_joint_translation
 * @param joint [type: b2Joint] prismatic joint
 * @return translation [type: number] translation in project units
 */

/*# Get joint speed.
 * @name b2d.joint.get_joint_speed
 * @param joint [type: b2Joint] prismatic joint
 * @return speed [type: number] joint speed
 */

/*# Get revolute joint angle.
 * @name b2d.joint.get_joint_angle
 * @param joint [type: b2Joint] revolute joint
 * @return angle [type: number] angle in radians
 */

/*# Set weld or motor joint linear frequency.
 * @name b2d.joint.set_linear_hertz
 * @param joint [type: b2Joint] weld or motor joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get weld or motor joint linear frequency.
 * @name b2d.joint.get_linear_hertz
 * @param joint [type: b2Joint] weld or motor joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Set weld or motor joint linear damping ratio.
 * @name b2d.joint.set_linear_damping_ratio
 * @param joint [type: b2Joint] weld or motor joint
 * @param ratio [type: number] damping ratio
 */

/*# Get weld or motor joint linear damping ratio.
 * @name b2d.joint.get_linear_damping_ratio
 * @param joint [type: b2Joint] weld or motor joint
 * @return ratio [type: number] damping ratio
 */

/*# Set weld or motor joint angular frequency.
 * @name b2d.joint.set_angular_hertz
 * @param joint [type: b2Joint] weld or motor joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get weld or motor joint angular frequency.
 * @name b2d.joint.get_angular_hertz
 * @param joint [type: b2Joint] weld or motor joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Set weld or motor joint angular damping ratio.
 * @name b2d.joint.set_angular_damping_ratio
 * @param joint [type: b2Joint] weld or motor joint
 * @param ratio [type: number] damping ratio
 */

/*# Get weld or motor joint angular damping ratio.
 * @name b2d.joint.get_angular_damping_ratio
 * @param joint [type: b2Joint] weld or motor joint
 * @return ratio [type: number] damping ratio
 */

/*# Get reaction force.
 * @name b2d.joint.get_reaction_force
 * @param joint [type: b2Joint] joint
 * @return force [type: vector3] reaction force
 */

/*# Get reaction torque.
 * @name b2d.joint.get_reaction_torque
 * @param joint [type: b2Joint] joint
 * @return torque [type: number] reaction torque
 */

/*# Create a distance joint.
 * @name b2d.joint.create_distance
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.distance_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a prismatic joint.
 * @name b2d.joint.create_prismatic
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.prismatic_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a revolute joint.
 * @name b2d.joint.create_revolute
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.revolute_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a weld joint.
 * @name b2d.joint.create_weld
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.weld_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a wheel joint.
 * @name b2d.joint.create_wheel
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.wheel_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a motor joint.
 * @name b2d.joint.create_motor
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.motor_definition] optional joint definition
 * @return joint [type: b2Joint] created joint
 */

/*# Create a filter joint.
 * @name b2d.joint.create_filter
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param [definition] [type:b2d.joint.filter_definition] optional definition table
 * @return joint [type: b2Joint] created joint
 */

/*# Set motor joint max velocity force.
 * @name b2d.joint.set_max_velocity_force
 * @param joint [type:b2Joint] motor joint
 * @param value [type:number] max velocity force
 */

/*# Get motor joint max velocity force.
 * @name b2d.joint.get_max_velocity_force
 * @param joint [type:b2Joint] motor joint
 * @return value [type:number] max velocity force
 */

/*# Set motor joint max spring force.
 * @name b2d.joint.set_max_spring_force
 * @param joint [type:b2Joint] motor joint
 * @param value [type:number] max spring force
 */

/*# Get motor joint max spring force.
 * @name b2d.joint.get_max_spring_force
 * @param joint [type:b2Joint] motor joint
 * @return value [type:number] max spring force
 */

/*# Set motor joint max spring torque.
 * @name b2d.joint.set_max_spring_torque
 * @param joint [type:b2Joint] motor joint
 * @param value [type:number] max spring torque
 */

/*# Get motor joint max spring torque.
 * @name b2d.joint.get_max_spring_torque
 * @param joint [type:b2Joint] motor joint
 * @return value [type:number] max spring torque
 */

/*# Set motor joint linear velocity.
 * @name b2d.joint.set_linear_velocity
 * @param joint [type:b2Joint] motor joint
 * @param value [type:vector3] linear velocity
 */

/*# Get motor joint linear velocity.
 * @name b2d.joint.get_linear_velocity
 * @param joint [type:b2Joint] motor joint
 * @return value [type:vector3] linear velocity
 */

/*# Set motor joint angular velocity.
 * @name b2d.joint.set_angular_velocity
 * @param joint [type:b2Joint] motor joint
 * @param value [type:number] angular velocity
 */

/*# Get motor joint angular velocity.
 * @name b2d.joint.get_angular_velocity
 * @param joint [type:b2Joint] motor joint
 * @return value [type:number] angular velocity
 */


/*# Set the local frame on body A.
 * @name b2d.joint.set_local_frame_a
 * @param joint [type:b2Joint] joint
 * @param frame [type:b2d.transform] Local frame with optional position (vector3) and angle (radians). Omitted fields are preserved.
 */

/*# Set the local frame on body B.
 * @name b2d.joint.set_local_frame_b
 * @param joint [type:b2Joint] joint
 * @param frame [type:b2d.transform] Local frame with optional position (vector3) and angle (radians). Omitted fields are preserved.
 */

/*# Set the stiffness of an individual joint constraint (Box2D v3).
 * Replaces world-wide joint tuning in Box2D 3.2.
 * @name b2d.joint.set_constraint_tuning
 * @param joint [type:b2Joint] joint
 * @param hertz [type:number] constraint frequency in hertz
 * @param damping_ratio [type:number] constraint damping ratio
 */

/*# Get the stiffness of an individual joint constraint (Box2D v3).
 * @name b2d.joint.get_constraint_tuning
 * @param joint [type:b2Joint] joint
 * @return hertz [type:number] constraint frequency in hertz
 * @return damping_ratio [type:number] constraint damping ratio
 */
