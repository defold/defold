// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

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
        dmGameObject::HCollection m_Collection;
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

    static void ReadCommonAnchoredDef(lua_State* L, int table_index, b2Vec2* local_anchor_a, b2Vec2* local_anchor_b, bool* collide_connected)
    {
        *local_anchor_a    = GetVec2Field(L, table_index, "local_anchor_a", *local_anchor_a, GetPhysicsScale());
        *local_anchor_b    = GetVec2Field(L, table_index, "local_anchor_b", *local_anchor_b, GetPhysicsScale());
        *collide_connected = GetBoolField(L, table_index, "collide_connected", *collide_connected);
    }

    void PushJoint(lua_State* L, b2JointId joint_id, dmGameObject::HCollection collection)
    {
        B2DLuaJoint* luajoint = (B2DLuaJoint*)lua_newuserdata(L, sizeof(B2DLuaJoint));
        luajoint->m_Joint = joint_id;
        luajoint->m_Collection = collection;
        luaL_getmetatable(L, BOX2D_TYPE_NAME_JOINT);
        lua_setmetatable(L, -2);
    }

    static void PushCreatedJoint(lua_State* L, b2JointId joint_id, dmGameObject::HCollection collection)
    {
        TrackJoint(joint_id);
        PushJoint(L, joint_id, collection);
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

    static int Joint_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Joint_GetType(CheckJoint(L, 1)));
        return 1;
    }

    static int Joint_GetBodyA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        b2BodyId body = b2Joint_GetBodyA(joint);
        PushBody(L, &body, luajoint->m_Collection, 0);
        return 1;
    }

    static int Joint_GetBodyB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        b2BodyId body = b2Joint_GetBodyB(joint);
        PushBody(L, &body, luajoint->m_Collection, 0);
        return 1;
    }

    static int Joint_GetLocalAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(b2Joint_GetLocalAnchorA(CheckJoint(L, 1)), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetLocalAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(b2Joint_GetLocalAnchorB(CheckJoint(L, 1)), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPoint(b2Joint_GetBodyA(joint), b2Joint_GetLocalAnchorA(joint)), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(b2Body_GetWorldPoint(b2Joint_GetBodyB(joint), b2Joint_GetLocalAnchorB(joint)), GetInvPhysicsScale()));
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

    static int Joint_SetMouseTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        if (b2Joint_GetType(joint) != b2_mouseJoint)
        {
            return luaL_error(L, "b2d.joint.set_mouse_target can only be used with mouse joints.");
        }
        if (b2World_IsLocked(b2Joint_GetWorld(joint)))
        {
            return luaL_error(L, "Could not set mouse joint target. The world is locked.");
        }
        b2MouseJoint_SetTarget(joint, CheckVec2(L, 2, GetPhysicsScale()));
        return 0;
    }

    static int Joint_GetMouseTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJointType(L, 1, b2_mouseJoint, "get_mouse_target", "mouse");
        dmScript::PushVector3(L, FromB2(b2MouseJoint_GetTarget(joint), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_SetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_length");
        b2DistanceJoint_SetLength(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetLength(CheckJointType(L, 1, b2_distanceJoint, "get_length", "distance")) * GetInvPhysicsScale());
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
            case b2_mouseJoint: b2MouseJoint_SetSpringHertz(joint, hertz); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetSpringHertz(joint, hertz); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetSpringHertz(joint, hertz); break;
            case b2_wheelJoint: b2WheelJoint_SetSpringHertz(joint, hertz); break;
            default: return luaL_error(L, "b2d.joint.set_spring_hertz can only be used with distance, mouse, prismatic, revolute, or wheel joints.");
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
            case b2_mouseJoint: lua_pushnumber(L, b2MouseJoint_GetSpringHertz(joint)); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetSpringHertz(joint)); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetSpringHertz(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetSpringHertz(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_spring_hertz can only be used with distance, mouse, prismatic, revolute, or wheel joints.");
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
            case b2_mouseJoint: b2MouseJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            case b2_wheelJoint: b2WheelJoint_SetSpringDampingRatio(joint, damping_ratio); break;
            default: return luaL_error(L, "b2d.joint.set_spring_damping_ratio can only be used with distance, mouse, prismatic, revolute, or wheel joints.");
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
            case b2_mouseJoint: lua_pushnumber(L, b2MouseJoint_GetSpringDampingRatio(joint)); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetSpringDampingRatio(joint)); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetSpringDampingRatio(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetSpringDampingRatio(joint)); break;
            default: return luaL_error(L, "b2d.joint.get_spring_damping_ratio can only be used with distance, mouse, prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetLengthRange(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_length_range", "distance");
        CheckJointWorldUnlocked(L, joint, "set_length_range");
        b2DistanceJoint_SetLengthRange(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), (float)luaL_checknumber(L, 3) * GetPhysicsScale());
        return 0;
    }

    static int Joint_SetMinLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_min_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_min_length");
        b2DistanceJoint_SetLengthRange(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale(), b2DistanceJoint_GetMaxLength(joint));
        return 0;
    }

    static int Joint_GetMinLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetMinLength(CheckJointType(L, 1, b2_distanceJoint, "get_min_length", "distance")) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_distanceJoint, "set_max_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_max_length");
        b2DistanceJoint_SetLengthRange(joint, b2DistanceJoint_GetMinLength(joint), (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetMaxLength(CheckJointType(L, 1, b2_distanceJoint, "get_max_length", "distance")) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_GetCurrentLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2DistanceJoint_GetCurrentLength(CheckJointType(L, 1, b2_distanceJoint, "get_current_length", "distance")) * GetInvPhysicsScale());
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
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetLowerLimit(joint) * GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetLowerLimit(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetLowerLimit(joint) * GetInvPhysicsScale()); break;
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
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetUpperLimit(joint) * GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetUpperLimit(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetUpperLimit(joint) * GetInvPhysicsScale()); break;
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
            case b2_distanceJoint: b2DistanceJoint_SetMotorSpeed(joint, motor_speed * GetPhysicsScale()); break;
            case b2_prismaticJoint: b2PrismaticJoint_SetMotorSpeed(joint, motor_speed * GetPhysicsScale()); break;
            case b2_revoluteJoint: b2RevoluteJoint_SetMotorSpeed(joint, motor_speed); break;
            case b2_wheelJoint: b2WheelJoint_SetMotorSpeed(joint, motor_speed); break;
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
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetMotorSpeed(joint) * GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetMotorSpeed(joint) * GetInvPhysicsScale()); break;
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetMotorSpeed(joint)); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetMotorSpeed(joint)); break;
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
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetMaxMotorForce(joint) * GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetMaxMotorForce(joint) * GetInvPhysicsScale()); break;
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
            case b2_distanceJoint: lua_pushnumber(L, b2DistanceJoint_GetMotorForce(joint) * GetInvPhysicsScale()); break;
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetMotorForce(joint) * GetInvPhysicsScale()); break;
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
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetMaxMotorTorque(joint) * GetInvPhysicsScale()); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetMaxMotorTorque(joint) * GetInvPhysicsScale()); break;
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
            case b2_revoluteJoint: lua_pushnumber(L, b2RevoluteJoint_GetMotorTorque(joint) * GetInvPhysicsScale()); break;
            case b2_wheelJoint: lua_pushnumber(L, b2WheelJoint_GetMotorTorque(joint) * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_motor_torque can only be used with revolute or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMaxForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_max_force");
        float max_force = (float)luaL_checknumber(L, 2) * GetPhysicsScale();
        switch (b2Joint_GetType(joint))
        {
            case b2_mouseJoint: b2MouseJoint_SetMaxForce(joint, max_force); break;
            case b2_motorJoint: b2MotorJoint_SetMaxForce(joint, max_force); break;
            default: return luaL_error(L, "b2d.joint.set_max_force can only be used with mouse or motor joints.");
        }
        return 0;
    }

    static int Joint_GetMaxForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_mouseJoint: lua_pushnumber(L, b2MouseJoint_GetMaxForce(joint) * GetInvPhysicsScale()); break;
            case b2_motorJoint: lua_pushnumber(L, b2MotorJoint_GetMaxForce(joint) * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_max_force can only be used with mouse or motor joints.");
        }
        return 1;
    }

    static int Joint_SetMaxTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_max_torque", "motor");
        CheckJointWorldUnlocked(L, joint, "set_max_torque");
        b2MotorJoint_SetMaxTorque(joint, (float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2MotorJoint_GetMaxTorque(CheckJointType(L, 1, b2_motorJoint, "get_max_torque", "motor")) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetLinearOffset(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_linear_offset", "motor");
        CheckJointWorldUnlocked(L, joint, "set_linear_offset");
        b2MotorJoint_SetLinearOffset(joint, CheckVec2(L, 2, GetPhysicsScale()));
        return 0;
    }

    static int Joint_GetLinearOffset(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(b2MotorJoint_GetLinearOffset(CheckJointType(L, 1, b2_motorJoint, "get_linear_offset", "motor")), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_SetAngularOffset(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_angular_offset", "motor");
        CheckJointWorldUnlocked(L, joint, "set_angular_offset");
        b2MotorJoint_SetAngularOffset(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetAngularOffset(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2MotorJoint_GetAngularOffset(CheckJointType(L, 1, b2_motorJoint, "get_angular_offset", "motor")));
        return 1;
    }

    static int Joint_SetCorrectionFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_motorJoint, "set_correction_factor", "motor");
        CheckJointWorldUnlocked(L, joint, "set_correction_factor");
        b2MotorJoint_SetCorrectionFactor(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetCorrectionFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2MotorJoint_GetCorrectionFactor(CheckJointType(L, 1, b2_motorJoint, "get_correction_factor", "motor")));
        return 1;
    }

    static int Joint_GetJointTranslation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2JointId joint = CheckJoint(L, 1);
        switch (b2Joint_GetType(joint))
        {
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetTranslation(joint) * GetInvPhysicsScale()); break;
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
            case b2_prismaticJoint: lua_pushnumber(L, b2PrismaticJoint_GetSpeed(joint) * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_joint_speed can only be used with prismatic joints.");
        }
        return 1;
    }

    static int Joint_GetJointAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2RevoluteJoint_GetAngle(CheckJointType(L, 1, b2_revoluteJoint, "get_joint_angle", "revolute")));
        return 1;
    }

    static int Joint_SetReferenceAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_weldJoint, "set_reference_angle", "weld");
        CheckJointWorldUnlocked(L, joint, "set_reference_angle");
        b2WeldJoint_SetReferenceAngle(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetReferenceAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2WeldJoint_GetReferenceAngle(CheckJointType(L, 1, b2_weldJoint, "get_reference_angle", "weld")));
        return 1;
    }

    static int Joint_SetLinearHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_weldJoint, "set_linear_hertz", "weld");
        CheckJointWorldUnlocked(L, joint, "set_linear_hertz");
        b2WeldJoint_SetLinearHertz(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetLinearHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2WeldJoint_GetLinearHertz(CheckJointType(L, 1, b2_weldJoint, "get_linear_hertz", "weld")));
        return 1;
    }

    static int Joint_SetLinearDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_weldJoint, "set_linear_damping_ratio", "weld");
        CheckJointWorldUnlocked(L, joint, "set_linear_damping_ratio");
        b2WeldJoint_SetLinearDampingRatio(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetLinearDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2WeldJoint_GetLinearDampingRatio(CheckJointType(L, 1, b2_weldJoint, "get_linear_damping_ratio", "weld")));
        return 1;
    }

    static int Joint_SetAngularHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_weldJoint, "set_angular_hertz", "weld");
        CheckJointWorldUnlocked(L, joint, "set_angular_hertz");
        b2WeldJoint_SetAngularHertz(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetAngularHertz(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2WeldJoint_GetAngularHertz(CheckJointType(L, 1, b2_weldJoint, "get_angular_hertz", "weld")));
        return 1;
    }

    static int Joint_SetAngularDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2JointId joint = CheckJointType(L, 1, b2_weldJoint, "set_angular_damping_ratio", "weld");
        CheckJointWorldUnlocked(L, joint, "set_angular_damping_ratio");
        b2WeldJoint_SetAngularDampingRatio(joint, (float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetAngularDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2WeldJoint_GetAngularDampingRatio(CheckJointType(L, 1, b2_weldJoint, "get_angular_damping_ratio", "weld")));
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
        lua_pushnumber(L, b2Joint_GetConstraintTorque(CheckJoint(L, 1)) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_CreateDistance(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2DistanceJointDef def = b2DefaultDistanceJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB, &def.collideConnected);
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
        PushCreatedJoint(L, joint, collection);
        return 1;
    }

    static int Joint_CreateMouse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2MouseJointDef def = b2DefaultMouseJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        def.target = GetVec2Field(L, def_index, "target", def.target, GetPhysicsScale());
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.maxForce = GetNumberField(L, def_index, "max_force", def.maxForce) * GetPhysicsScale();
        def.collideConnected = GetBoolField(L, def_index, "collide_connected", def.collideConnected);

        PushCreatedJoint(L, b2CreateMouseJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreatePrismatic(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2PrismaticJointDef def = b2DefaultPrismaticJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB, &def.collideConnected);
        def.localAxisA = GetVec2Field(L, def_index, "local_axis_a", def.localAxisA, 1.0f);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerTranslation = GetNumberField(L, def_index, "lower_translation", def.lowerTranslation) * GetPhysicsScale();
        def.upperTranslation = GetNumberField(L, def_index, "upper_translation", def.upperTranslation) * GetPhysicsScale();
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorForce = GetNumberField(L, def_index, "max_motor_force", def.maxMotorForce) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed) * GetPhysicsScale();

        PushCreatedJoint(L, b2CreatePrismaticJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreateRevolute(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2RevoluteJointDef def = b2DefaultRevoluteJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB, &def.collideConnected);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerAngle = GetNumberField(L, def_index, "lower_angle", def.lowerAngle);
        def.upperAngle = GetNumberField(L, def_index, "upper_angle", def.upperAngle);
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);

        PushCreatedJoint(L, b2CreateRevoluteJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreateWeld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WeldJointDef def = b2DefaultWeldJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB, &def.collideConnected);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        float hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.linearHertz);
        float damping = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.linearDampingRatio);
        def.linearHertz = GetNumberField(L, def_index, "linear_hertz", hertz);
        def.angularHertz = GetNumberField(L, def_index, "angular_hertz", hertz);
        def.linearDampingRatio = GetNumberField(L, def_index, "linear_damping_ratio", damping);
        def.angularDampingRatio = GetNumberField(L, def_index, "angular_damping_ratio", damping);

        PushCreatedJoint(L, b2CreateWeldJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreateWheel(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WheelJointDef def = b2DefaultWheelJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB, &def.collideConnected);
        def.localAxisA = GetVec2Field(L, def_index, "local_axis_a", def.localAxisA, 1.0f);
        def.enableSpring = GetBoolField(L, def_index, "enable_spring", def.enableSpring);
        def.hertz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.hertz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerTranslation = GetNumberField(L, def_index, "lower_translation", def.lowerTranslation) * GetPhysicsScale();
        def.upperTranslation = GetNumberField(L, def_index, "upper_translation", def.upperTranslation) * GetPhysicsScale();
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);

        PushCreatedJoint(L, b2CreateWheelJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreateMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2MotorJointDef def = b2DefaultMotorJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;
        def.linearOffset = GetVec2Field(L, def_index, "linear_offset", def.linearOffset, GetPhysicsScale());
        def.angularOffset = GetNumberField(L, def_index, "angular_offset", def.angularOffset);
        def.maxForce = GetNumberField(L, def_index, "max_force", def.maxForce) * GetPhysicsScale();
        def.maxTorque = GetNumberField(L, def_index, "max_torque", def.maxTorque) * GetPhysicsScale();
        def.correctionFactor = GetNumberField(L, def_index, "correction_factor", def.correctionFactor);
        def.collideConnected = GetBoolField(L, def_index, "collide_connected", def.collideConnected);

        PushCreatedJoint(L, b2CreateMotorJoint(world, &def), collection);
        return 1;
    }

    static int Joint_CreateFilter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2BodyId* body_a = 0;
        b2BodyId* body_b = 0;
        b2WorldId world = b2_nullWorldId;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);

        b2FilterJointDef def = b2DefaultFilterJointDef();
        def.bodyIdA = *body_a;
        def.bodyIdB = *body_b;

        PushCreatedJoint(L, b2CreateFilterJoint(world, &def), collection);
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
        {"get_type", Joint_GetType},
        {"get_body_a", Joint_GetBodyA},
        {"get_body_b", Joint_GetBodyB},
        {"get_local_anchor_a", Joint_GetLocalAnchorA},
        {"get_local_anchor_b", Joint_GetLocalAnchorB},
        {"get_anchor_a", Joint_GetAnchorA},
        {"get_anchor_b", Joint_GetAnchorB},
        {"get_collide_connected", Joint_GetCollideConnected},
        {"set_collide_connected", Joint_SetCollideConnected},
        {"wake_bodies", Joint_WakeBodies},
        {"set_mouse_target", Joint_SetMouseTarget},
        {"get_mouse_target", Joint_GetMouseTarget},
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
        {"set_max_force", Joint_SetMaxForce},
        {"get_max_force", Joint_GetMaxForce},
        {"set_max_torque", Joint_SetMaxTorque},
        {"get_max_torque", Joint_GetMaxTorque},
        {"set_linear_offset", Joint_SetLinearOffset},
        {"get_linear_offset", Joint_GetLinearOffset},
        {"set_angular_offset", Joint_SetAngularOffset},
        {"get_angular_offset", Joint_GetAngularOffset},
        {"set_correction_factor", Joint_SetCorrectionFactor},
        {"get_correction_factor", Joint_GetCorrectionFactor},
        {"get_joint_translation", Joint_GetJointTranslation},
        {"get_joint_speed", Joint_GetJointSpeed},
        {"get_joint_angle", Joint_GetJointAngle},
        {"set_reference_angle", Joint_SetReferenceAngle},
        {"get_reference_angle", Joint_GetReferenceAngle},
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
        {"create_mouse", Joint_CreateMouse},
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
        SET_CONSTANT(b2_mouseJoint, "JOINT_TYPE_MOUSE");
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
 * @typedef
 * @name b2Joint
 * @param value [type:userdata]
 */

/*# Distance joint type.
 * @name b2d.joint.JOINT_TYPE_DISTANCE
 * @constant
 */

/*# Filter joint type.
 * @name b2d.joint.JOINT_TYPE_FILTER
 * @constant
 */

/*# Motor joint type.
 * @name b2d.joint.JOINT_TYPE_MOTOR
 * @constant
 */

/*# Mouse joint type.
 * @name b2d.joint.JOINT_TYPE_MOUSE
 * @constant
 */

/*# Prismatic joint type.
 * @name b2d.joint.JOINT_TYPE_PRISMATIC
 * @constant
 */

/*# Revolute joint type.
 * @name b2d.joint.JOINT_TYPE_REVOLUTE
 * @constant
 */

/*# Weld joint type.
 * @name b2d.joint.JOINT_TYPE_WELD
 * @constant
 */

/*# Wheel joint type.
 * @name b2d.joint.JOINT_TYPE_WHEEL
 * @constant
 */

/*# Destroy a joint created by `b2d.joint`.
 * @name b2d.joint.destroy
 * @param joint [type: b2Joint] joint
 */

/*# Get the joint type.
 * @name b2d.joint.get_type
 * @param joint [type: b2Joint] joint
 * @return type [type: number] one of the `JOINT_TYPE_*` constants
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

/*# Get the local anchor on body A.
 * @name b2d.joint.get_local_anchor_a
 * @param joint [type: b2Joint] joint
 * @return anchor [type: vector3] local anchor
 */

/*# Get the local anchor on body B.
 * @name b2d.joint.get_local_anchor_b
 * @param joint [type: b2Joint] joint
 * @return anchor [type: vector3] local anchor
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

/*# Set the target for a mouse joint.
 * @name b2d.joint.set_mouse_target
 * @param joint [type: b2Joint] mouse joint
 * @param target [type: vector3] world target
 */

/*# Get the target for a mouse joint.
 * @name b2d.joint.get_mouse_target
 * @param joint [type: b2Joint] mouse joint
 * @return target [type: vector3] world target
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
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get spring frequency.
 * @name b2d.joint.get_spring_hertz
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.set_spring_hertz`.
 * @name b2d.joint.set_hertz
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.get_spring_hertz`.
 * @name b2d.joint.get_hertz
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.set_spring_hertz`.
 * @name b2d.joint.set_frequency
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @param frequency [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.get_spring_hertz`.
 * @name b2d.joint.get_frequency
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @return frequency [type: number] frequency in hertz
 */

/*# Set spring damping ratio.
 * @name b2d.joint.set_spring_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Get spring damping ratio.
 * @name b2d.joint.get_spring_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @return ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.set_spring_damping_ratio`.
 * @name b2d.joint.set_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.get_spring_damping_ratio`.
 * @name b2d.joint.get_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, prismatic, revolute, or wheel joint
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

/*# Set maximum force.
 * @name b2d.joint.set_max_force
 * @param joint [type: b2Joint] mouse or motor joint
 * @param force [type: number] maximum force
 */

/*# Get maximum force.
 * @name b2d.joint.get_max_force
 * @param joint [type: b2Joint] mouse or motor joint
 * @return force [type: number] maximum force
 */

/*# Set maximum torque.
 * @name b2d.joint.set_max_torque
 * @param joint [type: b2Joint] motor joint
 * @param torque [type: number] maximum torque
 */

/*# Get maximum torque.
 * @name b2d.joint.get_max_torque
 * @param joint [type: b2Joint] motor joint
 * @return torque [type: number] maximum torque
 */

/*# Set motor joint linear offset.
 * @name b2d.joint.set_linear_offset
 * @param joint [type: b2Joint] motor joint
 * @param offset [type: vector3] linear offset in project units
 */

/*# Get motor joint linear offset.
 * @name b2d.joint.get_linear_offset
 * @param joint [type: b2Joint] motor joint
 * @return offset [type: vector3] linear offset in project units
 */

/*# Set motor joint angular offset.
 * @name b2d.joint.set_angular_offset
 * @param joint [type: b2Joint] motor joint
 * @param offset [type: number] angular offset in radians
 */

/*# Get motor joint angular offset.
 * @name b2d.joint.get_angular_offset
 * @param joint [type: b2Joint] motor joint
 * @return offset [type: number] angular offset in radians
 */

/*# Set motor joint correction factor.
 * @name b2d.joint.set_correction_factor
 * @param joint [type: b2Joint] motor joint
 * @param factor [type: number] correction factor
 */

/*# Get motor joint correction factor.
 * @name b2d.joint.get_correction_factor
 * @param joint [type: b2Joint] motor joint
 * @return factor [type: number] correction factor
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

/*# Set weld joint reference angle.
 * @name b2d.joint.set_reference_angle
 * @param joint [type: b2Joint] weld joint
 * @param angle [type: number] reference angle in radians
 */

/*# Get weld joint reference angle.
 * @name b2d.joint.get_reference_angle
 * @param joint [type: b2Joint] weld joint
 * @return angle [type: number] reference angle in radians
 */

/*# Set weld joint linear frequency.
 * @name b2d.joint.set_linear_hertz
 * @param joint [type: b2Joint] weld joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get weld joint linear frequency.
 * @name b2d.joint.get_linear_hertz
 * @param joint [type: b2Joint] weld joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Set weld joint linear damping ratio.
 * @name b2d.joint.set_linear_damping_ratio
 * @param joint [type: b2Joint] weld joint
 * @param ratio [type: number] damping ratio
 */

/*# Get weld joint linear damping ratio.
 * @name b2d.joint.get_linear_damping_ratio
 * @param joint [type: b2Joint] weld joint
 * @return ratio [type: number] damping ratio
 */

/*# Set weld joint angular frequency.
 * @name b2d.joint.set_angular_hertz
 * @param joint [type: b2Joint] weld joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Get weld joint angular frequency.
 * @name b2d.joint.get_angular_hertz
 * @param joint [type: b2Joint] weld joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Set weld joint angular damping ratio.
 * @name b2d.joint.set_angular_damping_ratio
 * @param joint [type: b2Joint] weld joint
 * @param ratio [type: number] damping ratio
 */

/*# Get weld joint angular damping ratio.
 * @name b2d.joint.get_angular_damping_ratio
 * @param joint [type: b2Joint] weld joint
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
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `length`, `min_length`, `max_length`, `enable_spring`, `hertz` or `frequency`, `damping_ratio`, `enable_limit`, `enable_motor`, `max_motor_force`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a mouse joint.
 * @name b2d.joint.create_mouse
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `target`, `hertz` or `frequency`, `damping_ratio`, `max_force`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a prismatic joint.
 * @name b2d.joint.create_prismatic
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `local_axis_a`, `reference_angle`, `enable_spring`, `hertz` or `frequency`, `damping_ratio`, `enable_limit`, `lower_translation`, `upper_translation`, `enable_motor`, `max_motor_force`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a revolute joint.
 * @name b2d.joint.create_revolute
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `reference_angle`, `enable_spring`, `hertz` or `frequency`, `damping_ratio`, `enable_limit`, `lower_angle`, `upper_angle`, `enable_motor`, `max_motor_torque`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a weld joint.
 * @name b2d.joint.create_weld
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `reference_angle`, `linear_hertz`, `angular_hertz`, `linear_damping_ratio`, `angular_damping_ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a wheel joint.
 * @name b2d.joint.create_wheel
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `local_axis_a`, `enable_spring`, `hertz` or `frequency`, `damping_ratio`, `enable_limit`, `lower_translation`, `upper_translation`, `enable_motor`, `max_motor_torque`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a motor joint.
 * @name b2d.joint.create_motor
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `linear_offset`, `angular_offset`, `max_force`, `max_torque`, `correction_factor`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a filter joint.
 * @name b2d.joint.create_filter
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition table
 * @return joint [type: b2Joint] created joint
 */
