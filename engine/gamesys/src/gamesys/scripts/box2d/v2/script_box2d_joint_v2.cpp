// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2World.h>
#include <Box2D/Dynamics/Joints/b2DistanceJoint.h>
#include <Box2D/Dynamics/Joints/b2FrictionJoint.h>
#include <Box2D/Dynamics/Joints/b2GearJoint.h>
#include <Box2D/Dynamics/Joints/b2Joint.h>
#include <Box2D/Dynamics/Joints/b2MouseJoint.h>
#include <Box2D/Dynamics/Joints/b2PrismaticJoint.h>
#include <Box2D/Dynamics/Joints/b2PulleyJoint.h>
#include <Box2D/Dynamics/Joints/b2RevoluteJoint.h>
#include <Box2D/Dynamics/Joints/b2RopeJoint.h>
#include <Box2D/Dynamics/Joints/b2WeldJoint.h>
#include <Box2D/Dynamics/Joints/b2WheelJoint.h>

#include <dlib/opaque_handle_container.h>
#include <dmsdk/dlib/hashtable.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "gamesys_private.h"
#include "script_box2d_v2.h"

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
        HOpaqueHandle             m_Handle;
    };

    struct B2DJointMeta
    {
        dmGameObject::HCollection m_Collection;
    };

    static dmOpaqueHandleContainer<uintptr_t> g_JointHandles;
    static dmHashTable64<HOpaqueHandle>       g_JointToHandle;
    static dmHashTable32<B2DJointMeta>        g_JointMeta;

    static uint64_t JointPtrToKey(const b2Joint* joint)
    {
        return (uint64_t)(uintptr_t)joint;
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        return b2Vec2(v->getX() * scale, v->getY() * scale);
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

    static int CheckDefinitionTable(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            return 0;
        }
        luaL_checktype(L, index, LUA_TTABLE);
        return AbsIndex(L, index);
    }

    static void EnsureJointHandleCapacity()
    {
        if (g_JointHandles.Full())
        {
            g_JointHandles.Allocate(32);
            g_JointToHandle.OffsetCapacity(32);
            g_JointMeta.OffsetCapacity(32);
        }
    }

    static void InvalidateJointHandle(HOpaqueHandle handle)
    {
        uintptr_t* joint_ptr = g_JointHandles.Get(handle);
        if (!joint_ptr)
            return;

        HOpaqueHandle* mapped_handle = g_JointToHandle.Get(JointPtrToKey((b2Joint*)joint_ptr));
        if (mapped_handle && *mapped_handle == handle)
        {
            g_JointToHandle.Erase(JointPtrToKey((b2Joint*)joint_ptr));
        }

        if (g_JointMeta.Get(handle))
        {
            g_JointMeta.Erase(handle);
        }
        g_JointHandles.Release(handle);
    }

    static void RegisterJointHandle(b2Joint* joint, dmGameObject::HCollection collection, HOpaqueHandle* out_handle)
    {
        assert(joint);
        EnsureJointHandleCapacity();

        HOpaqueHandle* existing_handle = g_JointToHandle.Get(JointPtrToKey(joint));
        if (existing_handle && g_JointHandles.Get(*existing_handle))
        {
            B2DJointMeta* joint_meta = g_JointMeta.Get(*existing_handle);
            if (joint_meta)
            {
                joint_meta->m_Collection = collection;
            }
            *out_handle = *existing_handle;
            return;
        }

        HOpaqueHandle handle = g_JointHandles.Put((uintptr_t*)joint);
        g_JointToHandle.Put(JointPtrToKey(joint), handle);

        B2DJointMeta joint_meta = {};
        joint_meta.m_Collection = collection;
        g_JointMeta.Put(handle, joint_meta);

        *out_handle = handle;
    }

    bool IsJointTracked(b2Joint* joint)
    {
        if (!joint)
        {
            return false;
        }

        HOpaqueHandle* handle = g_JointToHandle.Get(JointPtrToKey(joint));
        return handle && g_JointHandles.Get(*handle);
    }

    void PushJoint(lua_State* L, b2Joint* joint, dmGameObject::HCollection collection)
    {
        B2DLuaJoint* luajoint = (B2DLuaJoint*)lua_newuserdata(L, sizeof(B2DLuaJoint));
        RegisterJointHandle(joint, collection, &luajoint->m_Handle);
        luaL_getmetatable(L, BOX2D_TYPE_NAME_JOINT);
        lua_setmetatable(L, -2);
    }

    static B2DLuaJoint* CheckJointInternal(lua_State* L, int index)
    {
        return (B2DLuaJoint*)dmScript::CheckUserType(L, index, TYPE_HASH_JOINT, "Expected user type " BOX2D_TYPE_NAME_JOINT);
    }

    static b2Joint* CheckJoint(lua_State* L, int index)
    {
        B2DLuaJoint* luajoint = CheckJointInternal(L, index);
        uintptr_t* joint_ptr = g_JointHandles.Get(luajoint->m_Handle);
        if (!joint_ptr)
        {
            luaL_error(L, "Invalid b2joint handle.");
            return 0;
        }
        return (b2Joint*)joint_ptr;
    }

    static b2Joint* CheckJoint(lua_State* L, int index, HOpaqueHandle* out_handle)
    {
        B2DLuaJoint* luajoint = CheckJointInternal(L, index);
        uintptr_t* joint_ptr = g_JointHandles.Get(luajoint->m_Handle);
        if (!joint_ptr)
        {
            luaL_error(L, "Invalid b2joint handle.");
            return 0;
        }

        if (out_handle)
        {
            *out_handle = luajoint->m_Handle;
        }

        return (b2Joint*)joint_ptr;
    }

    static b2Joint* CheckJoint(lua_State* L, int index, B2DJointMeta** out_joint_meta)
    {
        B2DLuaJoint* luajoint = CheckJointInternal(L, index);
        uintptr_t* joint_ptr = g_JointHandles.Get(luajoint->m_Handle);
        if (!joint_ptr)
        {
            luaL_error(L, "Invalid b2joint handle.");
            return 0;
        }

        if (out_joint_meta)
        {
            *out_joint_meta = g_JointMeta.Get(luajoint->m_Handle);
            if (!*out_joint_meta)
            {
                luaL_error(L, "Invalid b2joint handle.");
                return 0;
            }
        }

        return (b2Joint*)joint_ptr;
    }

    static b2Joint* CheckJointType(lua_State* L, int index, B2DJointMeta** out_joint_meta, b2JointType joint_type, const char* function_name, const char* joint_type_name)
    {
        b2Joint* joint = CheckJoint(L, index, out_joint_meta);
        if (joint->GetType() != joint_type)
        {
            luaL_error(L, "b2d.joint.%s can only be used with %s joints.", function_name, joint_type_name);
            return 0;
        }
        return joint;
    }

    static b2Joint* CheckJointType(lua_State* L, int index, b2JointType joint_type, const char* function_name, const char* joint_type_name)
    {
        return CheckJointType(L, index, 0, joint_type, function_name, joint_type_name);
    }

    static void CheckJointWorldUnlocked(lua_State* L, b2Joint* joint, const char* function_name)
    {
        if (joint->GetBodyA()->GetWorld()->IsLocked())
        {
            luaL_error(L, "Could not call b2d.joint.%s. The world is locked.", function_name);
        }
    }

    static B2DLuaJoint* ToJoint(lua_State* L, int index)
    {
        return (B2DLuaJoint*)dmScript::ToUserType(L, index, TYPE_HASH_JOINT);
    }

    static void CheckJointDefBodies(lua_State* L, int body_a_index, int body_b_index, b2Body** body_a, b2Body** body_b, b2World** world)
    {
        body_a_index = AbsIndex(L, body_a_index);
        body_b_index = AbsIndex(L, body_b_index);
        *body_a = CheckBody(L, body_a_index);
        *body_b = CheckBody(L, body_b_index);
        b2World* world_a = (*body_a)->GetWorld();
        b2World* world_b = (*body_b)->GetWorld();
        if (world_a != world_b)
        {
            luaL_error(L, "joints can only be connected to bodies in the same Box2D world.");
            return;
        }
        if (world_a->IsLocked())
        {
            luaL_error(L, "Could not create joint. The world is locked.");
            return;
        }
        *world = world_a;
    }

    static void ReadCommonDef(lua_State* L, int table_index, b2JointDef* def)
    {
        def->collideConnected = GetBoolField(L, table_index, "collide_connected", def->collideConnected);
    }

    static void ReadCommonAnchoredDef(lua_State* L, int table_index, b2Vec2* local_anchor_a, b2Vec2* local_anchor_b)
    {
        *local_anchor_a = GetVec2Field(L, table_index, "local_anchor_a", *local_anchor_a, GetPhysicsScale());
        *local_anchor_b = GetVec2Field(L, table_index, "local_anchor_b", *local_anchor_b, GetPhysicsScale());
    }

    static int Joint_Destroy(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        HOpaqueHandle joint_handle = INVALID_OPAQUE_HANDLE;
        b2Joint* joint = CheckJoint(L, 1, &joint_handle);
        b2World* world = joint->GetBodyA()->GetWorld();
        if (world->IsLocked())
        {
            return luaL_error(L, "Could not destroy joint. The world is locked.");
        }
        world->DestroyJoint(joint);
        InvalidateJointHandle(joint_handle);
        return 0;
    }

    static int Joint_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckJoint(L, 1)->GetType());
        return 1;
    }

    static int Joint_GetBodyA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DJointMeta* joint_meta = 0;
        b2Body* body = CheckJoint(L, 1, &joint_meta)->GetBodyA();
        PushBody(L, body, joint_meta ? joint_meta->m_Collection : 0, GetBodyInstanceId(body));
        return 1;
    }

    static int Joint_GetBodyB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DJointMeta* joint_meta = 0;
        b2Body* body = CheckJoint(L, 1, &joint_meta)->GetBodyB();
        PushBody(L, body, joint_meta ? joint_meta->m_Collection : 0, GetBodyInstanceId(body));
        return 1;
    }

    static int Joint_GetLocalAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(joint->GetBodyA()->GetLocalPoint(joint->GetAnchorA()), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetLocalAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        dmScript::PushVector3(L, FromB2(joint->GetBodyB()->GetLocalPoint(joint->GetAnchorB()), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(CheckJoint(L, 1)->GetAnchorA(), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(CheckJoint(L, 1)->GetAnchorB(), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_IsActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckJoint(L, 1)->IsActive());
        return 1;
    }

    static int Joint_GetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckJoint(L, 1)->GetCollideConnected());
        return 1;
    }

    static int Joint_SetMouseTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        if (joint->GetType() != e_mouseJoint)
        {
            return luaL_error(L, "b2d.joint.set_mouse_target can only be used with mouse joints.");
        }
        b2World* world = joint->GetBodyA()->GetWorld();
        if (world->IsLocked())
        {
            return luaL_error(L, "Could not set mouse joint target. The world is locked.");
        }
        ((b2MouseJoint*)joint)->SetTarget(CheckVec2(L, 2, GetPhysicsScale()));
        return 0;
    }

    static int Joint_GetMouseTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2MouseJoint* joint = (b2MouseJoint*)CheckJointType(L, 1, e_mouseJoint, "get_mouse_target", "mouse");
        dmScript::PushVector3(L, FromB2(joint->GetTarget(), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_SetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2DistanceJoint* joint = (b2DistanceJoint*)CheckJointType(L, 1, e_distanceJoint, "set_length", "distance");
        CheckJointWorldUnlocked(L, joint, "set_length");
        joint->SetLength((float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2DistanceJoint* joint = (b2DistanceJoint*)CheckJointType(L, 1, e_distanceJoint, "get_length", "distance");
        lua_pushnumber(L, joint->GetLength() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetFrequency(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_frequency");
        float frequency = (float)luaL_checknumber(L, 2);
        switch (joint->GetType())
        {
            case e_distanceJoint: ((b2DistanceJoint*)joint)->SetFrequency(frequency); break;
            case e_mouseJoint: ((b2MouseJoint*)joint)->SetFrequency(frequency); break;
            case e_weldJoint: ((b2WeldJoint*)joint)->SetFrequency(frequency); break;
            case e_wheelJoint: ((b2WheelJoint*)joint)->SetSpringFrequencyHz(frequency); break;
            default: return luaL_error(L, "b2d.joint.set_frequency can only be used with distance, mouse, weld, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetFrequency(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_distanceJoint: lua_pushnumber(L, ((b2DistanceJoint*)joint)->GetFrequency()); break;
            case e_mouseJoint: lua_pushnumber(L, ((b2MouseJoint*)joint)->GetFrequency()); break;
            case e_weldJoint: lua_pushnumber(L, ((b2WeldJoint*)joint)->GetFrequency()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetSpringFrequencyHz()); break;
            default: return luaL_error(L, "b2d.joint.get_frequency can only be used with distance, mouse, weld, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_damping_ratio");
        float damping_ratio = (float)luaL_checknumber(L, 2);
        switch (joint->GetType())
        {
            case e_distanceJoint: ((b2DistanceJoint*)joint)->SetDampingRatio(damping_ratio); break;
            case e_mouseJoint: ((b2MouseJoint*)joint)->SetDampingRatio(damping_ratio); break;
            case e_weldJoint: ((b2WeldJoint*)joint)->SetDampingRatio(damping_ratio); break;
            case e_wheelJoint: ((b2WheelJoint*)joint)->SetSpringDampingRatio(damping_ratio); break;
            default: return luaL_error(L, "b2d.joint.set_damping_ratio can only be used with distance, mouse, weld, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetDampingRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_distanceJoint: lua_pushnumber(L, ((b2DistanceJoint*)joint)->GetDampingRatio()); break;
            case e_mouseJoint: lua_pushnumber(L, ((b2MouseJoint*)joint)->GetDampingRatio()); break;
            case e_weldJoint: lua_pushnumber(L, ((b2WeldJoint*)joint)->GetDampingRatio()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetSpringDampingRatio()); break;
            default: return luaL_error(L, "b2d.joint.get_damping_ratio can only be used with distance, mouse, weld, or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetLocalAxisA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: dmScript::PushVector3(L, FromB2(((b2PrismaticJoint*)joint)->GetLocalAxisA(), 1.0f)); break;
            case e_wheelJoint: dmScript::PushVector3(L, FromB2(((b2WheelJoint*)joint)->GetLocalAxisA(), 1.0f)); break;
            default: return luaL_error(L, "b2d.joint.get_local_axis_a can only be used with prismatic or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetReferenceAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetReferenceAngle()); break;
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetReferenceAngle()); break;
            case e_weldJoint: lua_pushnumber(L, ((b2WeldJoint*)joint)->GetReferenceAngle()); break;
            default: return luaL_error(L, "b2d.joint.get_reference_angle can only be used with prismatic, revolute, or weld joints.");
        }
        return 1;
    }

    static int Joint_GetJointTranslation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetJointTranslation() * GetInvPhysicsScale()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetJointTranslation() * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_joint_translation can only be used with prismatic or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetJointSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetJointSpeed() * GetInvPhysicsScale()); break;
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetJointSpeed()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetJointSpeed() * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_joint_speed can only be used with prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetJointAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2RevoluteJoint* joint = (b2RevoluteJoint*)CheckJointType(L, 1, e_revoluteJoint, "get_joint_angle", "revolute");
        lua_pushnumber(L, joint->GetJointAngle());
        return 1;
    }

    static int Joint_EnableLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "enable_limit");
        bool enable_limit = lua_toboolean(L, 2);
        switch (joint->GetType())
        {
            case e_prismaticJoint: ((b2PrismaticJoint*)joint)->EnableLimit(enable_limit); break;
            case e_revoluteJoint: ((b2RevoluteJoint*)joint)->EnableLimit(enable_limit); break;
            default: return luaL_error(L, "b2d.joint.enable_limit can only be used with prismatic or revolute joints.");
        }
        return 0;
    }

    static int Joint_IsLimitEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushboolean(L, ((b2PrismaticJoint*)joint)->IsLimitEnabled()); break;
            case e_revoluteJoint: lua_pushboolean(L, ((b2RevoluteJoint*)joint)->IsLimitEnabled()); break;
            default: return luaL_error(L, "b2d.joint.is_limit_enabled can only be used with prismatic or revolute joints.");
        }
        return 1;
    }

    static int Joint_SetLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_limits");
        switch (joint->GetType())
        {
            case e_prismaticJoint:
                ((b2PrismaticJoint*)joint)->SetLimits((float)luaL_checknumber(L, 2) * GetPhysicsScale(), (float)luaL_checknumber(L, 3) * GetPhysicsScale());
                break;
            case e_revoluteJoint:
                ((b2RevoluteJoint*)joint)->SetLimits((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
                break;
            default: return luaL_error(L, "b2d.joint.set_limits can only be used with prismatic or revolute joints.");
        }
        return 0;
    }

    static int Joint_GetLowerLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetLowerLimit() * GetInvPhysicsScale()); break;
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetLowerLimit()); break;
            default: return luaL_error(L, "b2d.joint.get_lower_limit can only be used with prismatic or revolute joints.");
        }
        return 1;
    }

    static int Joint_GetUpperLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetUpperLimit() * GetInvPhysicsScale()); break;
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetUpperLimit()); break;
            default: return luaL_error(L, "b2d.joint.get_upper_limit can only be used with prismatic or revolute joints.");
        }
        return 1;
    }

    static int Joint_EnableMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "enable_motor");
        bool enable_motor = lua_toboolean(L, 2);
        switch (joint->GetType())
        {
            case e_prismaticJoint: ((b2PrismaticJoint*)joint)->EnableMotor(enable_motor); break;
            case e_revoluteJoint: ((b2RevoluteJoint*)joint)->EnableMotor(enable_motor); break;
            case e_wheelJoint: ((b2WheelJoint*)joint)->EnableMotor(enable_motor); break;
            default: return luaL_error(L, "b2d.joint.enable_motor can only be used with prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_IsMotorEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushboolean(L, ((b2PrismaticJoint*)joint)->IsMotorEnabled()); break;
            case e_revoluteJoint: lua_pushboolean(L, ((b2RevoluteJoint*)joint)->IsMotorEnabled()); break;
            case e_wheelJoint: lua_pushboolean(L, ((b2WheelJoint*)joint)->IsMotorEnabled()); break;
            default: return luaL_error(L, "b2d.joint.is_motor_enabled can only be used with prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMotorSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_motor_speed");
        float motor_speed = (float)luaL_checknumber(L, 2);
        switch (joint->GetType())
        {
            case e_prismaticJoint: ((b2PrismaticJoint*)joint)->SetMotorSpeed(motor_speed * GetPhysicsScale()); break;
            case e_revoluteJoint: ((b2RevoluteJoint*)joint)->SetMotorSpeed(motor_speed); break;
            case e_wheelJoint: ((b2WheelJoint*)joint)->SetMotorSpeed(motor_speed); break;
            default: return luaL_error(L, "b2d.joint.set_motor_speed can only be used with prismatic, revolute, or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetMotorSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_prismaticJoint: lua_pushnumber(L, ((b2PrismaticJoint*)joint)->GetMotorSpeed() * GetInvPhysicsScale()); break;
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetMotorSpeed()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetMotorSpeed()); break;
            default: return luaL_error(L, "b2d.joint.get_motor_speed can only be used with prismatic, revolute, or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMaxMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2PrismaticJoint* joint = (b2PrismaticJoint*)CheckJointType(L, 1, e_prismaticJoint, "set_max_motor_force", "prismatic");
        CheckJointWorldUnlocked(L, joint, "set_max_motor_force");
        joint->SetMaxMotorForce((float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PrismaticJoint* joint = (b2PrismaticJoint*)CheckJointType(L, 1, e_prismaticJoint, "get_max_motor_force", "prismatic");
        lua_pushnumber(L, joint->GetMaxMotorForce() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_GetMotorForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PrismaticJoint* joint = (b2PrismaticJoint*)CheckJointType(L, 1, e_prismaticJoint, "get_motor_force", "prismatic");
        float inv_dt = (float)luaL_optnumber(L, 2, 1.0);
        lua_pushnumber(L, joint->GetMotorForce(inv_dt) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_max_motor_torque");
        float max_motor_torque = (float)luaL_checknumber(L, 2) * GetPhysicsScale();
        switch (joint->GetType())
        {
            case e_revoluteJoint: ((b2RevoluteJoint*)joint)->SetMaxMotorTorque(max_motor_torque); break;
            case e_wheelJoint: ((b2WheelJoint*)joint)->SetMaxMotorTorque(max_motor_torque); break;
            default: return luaL_error(L, "b2d.joint.set_max_motor_torque can only be used with revolute or wheel joints.");
        }
        return 0;
    }

    static int Joint_GetMaxMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetMaxMotorTorque() * GetInvPhysicsScale()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetMaxMotorTorque() * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_max_motor_torque can only be used with revolute or wheel joints.");
        }
        return 1;
    }

    static int Joint_GetMotorTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        float inv_dt = (float)luaL_optnumber(L, 2, 1.0);
        switch (joint->GetType())
        {
            case e_revoluteJoint: lua_pushnumber(L, ((b2RevoluteJoint*)joint)->GetMotorTorque(inv_dt) * GetInvPhysicsScale()); break;
            case e_wheelJoint: lua_pushnumber(L, ((b2WheelJoint*)joint)->GetMotorTorque(inv_dt) * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_motor_torque can only be used with revolute or wheel joints.");
        }
        return 1;
    }

    static int Joint_SetMaxForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Joint* joint = CheckJoint(L, 1);
        CheckJointWorldUnlocked(L, joint, "set_max_force");
        float max_force = (float)luaL_checknumber(L, 2) * GetPhysicsScale();
        switch (joint->GetType())
        {
            case e_mouseJoint: ((b2MouseJoint*)joint)->SetMaxForce(max_force); break;
            case e_frictionJoint: ((b2FrictionJoint*)joint)->SetMaxForce(max_force); break;
            default: return luaL_error(L, "b2d.joint.set_max_force can only be used with mouse or friction joints.");
        }
        return 0;
    }

    static int Joint_GetMaxForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_mouseJoint: lua_pushnumber(L, ((b2MouseJoint*)joint)->GetMaxForce() * GetInvPhysicsScale()); break;
            case e_frictionJoint: lua_pushnumber(L, ((b2FrictionJoint*)joint)->GetMaxForce() * GetInvPhysicsScale()); break;
            default: return luaL_error(L, "b2d.joint.get_max_force can only be used with mouse or friction joints.");
        }
        return 1;
    }

    static int Joint_SetMaxTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2FrictionJoint* joint = (b2FrictionJoint*)CheckJointType(L, 1, e_frictionJoint, "set_max_torque", "friction");
        CheckJointWorldUnlocked(L, joint, "set_max_torque");
        joint->SetMaxTorque((float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2FrictionJoint* joint = (b2FrictionJoint*)CheckJointType(L, 1, e_frictionJoint, "get_max_torque", "friction");
        lua_pushnumber(L, joint->GetMaxTorque() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2RopeJoint* joint = (b2RopeJoint*)CheckJointType(L, 1, e_ropeJoint, "set_max_length", "rope");
        CheckJointWorldUnlocked(L, joint, "set_max_length");
        joint->SetMaxLength((float)luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int Joint_GetMaxLength(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2RopeJoint* joint = (b2RopeJoint*)CheckJointType(L, 1, e_ropeJoint, "get_max_length", "rope");
        lua_pushnumber(L, joint->GetMaxLength() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_GetLimitState(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2RopeJoint* joint = (b2RopeJoint*)CheckJointType(L, 1, e_ropeJoint, "get_limit_state", "rope");
        lua_pushinteger(L, joint->GetLimitState());
        return 1;
    }

    static int Joint_GetGroundAnchorA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PulleyJoint* joint = (b2PulleyJoint*)CheckJointType(L, 1, e_pulleyJoint, "get_ground_anchor_a", "pulley");
        dmScript::PushVector3(L, FromB2(joint->GetGroundAnchorA(), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetGroundAnchorB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PulleyJoint* joint = (b2PulleyJoint*)CheckJointType(L, 1, e_pulleyJoint, "get_ground_anchor_b", "pulley");
        dmScript::PushVector3(L, FromB2(joint->GetGroundAnchorB(), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetLengthA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PulleyJoint* joint = (b2PulleyJoint*)CheckJointType(L, 1, e_pulleyJoint, "get_length_a", "pulley");
        lua_pushnumber(L, joint->GetLengthA() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_GetLengthB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2PulleyJoint* joint = (b2PulleyJoint*)CheckJointType(L, 1, e_pulleyJoint, "get_length_b", "pulley");
        lua_pushnumber(L, joint->GetLengthB() * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_SetRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2GearJoint* joint = (b2GearJoint*)CheckJointType(L, 1, e_gearJoint, "set_ratio", "gear");
        CheckJointWorldUnlocked(L, joint, "set_ratio");
        joint->SetRatio((float)luaL_checknumber(L, 2));
        return 0;
    }

    static int Joint_GetRatio(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        switch (joint->GetType())
        {
            case e_gearJoint: lua_pushnumber(L, ((b2GearJoint*)joint)->GetRatio()); break;
            case e_pulleyJoint: lua_pushnumber(L, ((b2PulleyJoint*)joint)->GetRatio()); break;
            default: return luaL_error(L, "b2d.joint.get_ratio can only be used with gear or pulley joints.");
        }
        return 1;
    }

    static int Joint_GetJoint1(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DJointMeta* joint_meta = 0;
        b2GearJoint* joint = (b2GearJoint*)CheckJointType(L, 1, &joint_meta, e_gearJoint, "get_joint1", "gear");
        PushJoint(L, joint->GetJoint1(), joint_meta ? joint_meta->m_Collection : 0);
        return 1;
    }

    static int Joint_GetJoint2(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DJointMeta* joint_meta = 0;
        b2GearJoint* joint = (b2GearJoint*)CheckJointType(L, 1, &joint_meta, e_gearJoint, "get_joint2", "gear");
        PushJoint(L, joint->GetJoint2(), joint_meta ? joint_meta->m_Collection : 0);
        return 1;
    }

    static int Joint_GetReactionForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        float inv_dt = (float)luaL_optnumber(L, 2, 1.0);
        dmScript::PushVector3(L, FromB2(joint->GetReactionForce(inv_dt), GetInvPhysicsScale()));
        return 1;
    }

    static int Joint_GetReactionTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        float inv_dt = (float)luaL_optnumber(L, 2, 1.0);
        lua_pushnumber(L, joint->GetReactionTorque(inv_dt) * GetInvPhysicsScale());
        return 1;
    }

    static int Joint_CreateDistance(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2DistanceJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.length = GetNumberField(L, def_index, "length", def.length) * GetPhysicsScale();
        def.frequencyHz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.frequencyHz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateMouse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2MouseJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        def.target = GetVec2Field(L, def_index, "target", def.target, GetPhysicsScale());
        def.maxForce = GetNumberField(L, def_index, "max_force", def.maxForce) * GetPhysicsScale();
        def.frequencyHz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.frequencyHz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreatePrismatic(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2PrismaticJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.localAxisA = GetVec2Field(L, def_index, "local_axis_a", def.localAxisA, 1.0f);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerTranslation = GetNumberField(L, def_index, "lower_translation", def.lowerTranslation) * GetPhysicsScale();
        def.upperTranslation = GetNumberField(L, def_index, "upper_translation", def.upperTranslation) * GetPhysicsScale();
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorForce = GetNumberField(L, def_index, "max_motor_force", def.maxMotorForce) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed) * GetPhysicsScale();

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateRevolute(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2RevoluteJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        def.enableLimit = GetBoolField(L, def_index, "enable_limit", def.enableLimit);
        def.lowerAngle = GetNumberField(L, def_index, "lower_angle", def.lowerAngle);
        def.upperAngle = GetNumberField(L, def_index, "upper_angle", def.upperAngle);
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateWeld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WeldJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.referenceAngle = GetNumberField(L, def_index, "reference_angle", def.referenceAngle);
        def.frequencyHz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.frequencyHz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateWheel(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2WheelJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.localAxisA = GetVec2Field(L, def_index, "local_axis_a", def.localAxisA, 1.0f);
        def.enableMotor = GetBoolField(L, def_index, "enable_motor", def.enableMotor);
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque) * GetPhysicsScale();
        def.motorSpeed = GetNumberField(L, def_index, "motor_speed", def.motorSpeed);
        def.frequencyHz = GetNumberField(L, def_index, HasField(L, def_index, "frequency") ? "frequency" : "hertz", def.frequencyHz);
        def.dampingRatio = GetNumberField(L, def_index, HasField(L, def_index, "damping") ? "damping" : "damping_ratio", def.dampingRatio);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2FrictionJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.maxForce = GetNumberField(L, def_index, "max_force", def.maxForce) * GetPhysicsScale();
        def.maxTorque = GetNumberField(L, def_index, "max_torque", def.maxTorque) * GetPhysicsScale();

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateRope(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2RopeJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.maxLength = GetNumberField(L, def_index, "max_length", def.maxLength) * GetPhysicsScale();

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreatePulley(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = GetBodyCollection(L, 1);
        b2Body* body_a = 0;
        b2Body* body_b = 0;
        b2World* world = 0;
        CheckJointDefBodies(L, 1, 2, &body_a, &body_b, &world);
        int def_index = CheckDefinitionTable(L, 3);

        b2PulleyJointDef def;
        def.bodyA = body_a;
        def.bodyB = body_b;
        ReadCommonDef(L, def_index, &def);
        ReadCommonAnchoredDef(L, def_index, &def.localAnchorA, &def.localAnchorB);
        def.groundAnchorA = GetVec2Field(L, def_index, "ground_anchor_a", def.groundAnchorA, GetPhysicsScale());
        def.groundAnchorB = GetVec2Field(L, def_index, "ground_anchor_b", def.groundAnchorB, GetPhysicsScale());
        def.lengthA = GetNumberField(L, def_index, "length_a", def.lengthA) * GetPhysicsScale();
        def.lengthB = GetNumberField(L, def_index, "length_b", def.lengthB) * GetPhysicsScale();
        def.ratio = GetNumberField(L, def_index, "ratio", def.ratio);

        PushJoint(L, world->CreateJoint(&def), collection);
        return 1;
    }

    static int Joint_CreateGear(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DJointMeta* joint_meta = 0;
        b2Joint* joint1 = CheckJoint(L, 1, &joint_meta);
        b2Joint* joint2 = CheckJoint(L, 2);
        b2World* world = joint1->GetBodyA()->GetWorld();
        if (world != joint2->GetBodyA()->GetWorld())
        {
            return luaL_error(L, "gear joints can only connect joints in the same Box2D world.");
        }
        if (world->IsLocked())
        {
            return luaL_error(L, "Could not create joint. The world is locked.");
        }
        int def_index = CheckDefinitionTable(L, 3);

        b2GearJointDef def;
        ReadCommonDef(L, def_index, &def);
        def.bodyA = joint1->GetBodyA();
        def.bodyB = joint2->GetBodyB();
        def.joint1 = joint1;
        def.joint2 = joint2;
        def.ratio = GetNumberField(L, def_index, "ratio", def.ratio);

        PushJoint(L, world->CreateJoint(&def), joint_meta ? joint_meta->m_Collection : 0);
        return 1;
    }

    static int Joint_tostring(lua_State* L)
    {
        b2Joint* joint = CheckJoint(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_JOINT, joint);
        return 1;
    }

    static int Joint_eq(lua_State* L)
    {
        B2DLuaJoint* a = ToJoint(L, 1);
        B2DLuaJoint* b = ToJoint(L, 2);
        lua_pushboolean(L, a && b && a->m_Handle == b->m_Handle);
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
        {"is_active", Joint_IsActive},
        {"get_collide_connected", Joint_GetCollideConnected},
        {"set_mouse_target", Joint_SetMouseTarget},
        {"get_mouse_target", Joint_GetMouseTarget},
        {"set_length", Joint_SetLength},
        {"get_length", Joint_GetLength},
        {"set_frequency", Joint_SetFrequency},
        {"get_frequency", Joint_GetFrequency},
        {"set_hertz", Joint_SetFrequency},
        {"get_hertz", Joint_GetFrequency},
        {"set_damping_ratio", Joint_SetDampingRatio},
        {"get_damping_ratio", Joint_GetDampingRatio},
        {"set_spring_damping_ratio", Joint_SetDampingRatio},
        {"get_spring_damping_ratio", Joint_GetDampingRatio},
        {"get_local_axis_a", Joint_GetLocalAxisA},
        {"get_reference_angle", Joint_GetReferenceAngle},
        {"get_joint_translation", Joint_GetJointTranslation},
        {"get_joint_speed", Joint_GetJointSpeed},
        {"get_joint_angle", Joint_GetJointAngle},
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
        {"set_max_length", Joint_SetMaxLength},
        {"get_max_length", Joint_GetMaxLength},
        {"get_limit_state", Joint_GetLimitState},
        {"get_ground_anchor_a", Joint_GetGroundAnchorA},
        {"get_ground_anchor_b", Joint_GetGroundAnchorB},
        {"get_length_a", Joint_GetLengthA},
        {"get_length_b", Joint_GetLengthB},
        {"set_ratio", Joint_SetRatio},
        {"get_ratio", Joint_GetRatio},
        {"get_joint1", Joint_GetJoint1},
        {"get_joint2", Joint_GetJoint2},
        {"get_reaction_force", Joint_GetReactionForce},
        {"get_reaction_torque", Joint_GetReactionTorque},
        {"create_distance", Joint_CreateDistance},
        {"create_mouse", Joint_CreateMouse},
        {"create_prismatic", Joint_CreatePrismatic},
        {"create_revolute", Joint_CreateRevolute},
        {"create_weld", Joint_CreateWeld},
        {"create_wheel", Joint_CreateWheel},
        {"create_friction", Joint_CreateFriction},
        {"create_rope", Joint_CreateRope},
        {"create_pulley", Joint_CreatePulley},
        {"create_gear", Joint_CreateGear},
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

        SET_CONSTANT(e_unknownJoint, "JOINT_TYPE_UNKNOWN");
        SET_CONSTANT(e_revoluteJoint, "JOINT_TYPE_REVOLUTE");
        SET_CONSTANT(e_prismaticJoint, "JOINT_TYPE_PRISMATIC");
        SET_CONSTANT(e_distanceJoint, "JOINT_TYPE_DISTANCE");
        SET_CONSTANT(e_pulleyJoint, "JOINT_TYPE_PULLEY");
        SET_CONSTANT(e_mouseJoint, "JOINT_TYPE_MOUSE");
        SET_CONSTANT(e_gearJoint, "JOINT_TYPE_GEAR");
        SET_CONSTANT(e_wheelJoint, "JOINT_TYPE_WHEEL");
        SET_CONSTANT(e_weldJoint, "JOINT_TYPE_WELD");
        SET_CONSTANT(e_frictionJoint, "JOINT_TYPE_FRICTION");
        SET_CONSTANT(e_ropeJoint, "JOINT_TYPE_ROPE");
        SET_CONSTANT(e_inactiveLimit, "LIMIT_STATE_INACTIVE");
        SET_CONSTANT(e_atLowerLimit, "LIMIT_STATE_AT_LOWER");
        SET_CONSTANT(e_atUpperLimit, "LIMIT_STATE_AT_UPPER");
        SET_CONSTANT(e_equalLimits, "LIMIT_STATE_EQUAL");

#undef SET_CONSTANT

        lua_setfield(L, -2, "joint");
    }

    void ScriptBox2DInvalidateJoint(void* joint)
    {
        if (!joint)
        {
            return;
        }

        HOpaqueHandle* handle = g_JointToHandle.Get(JointPtrToKey((b2Joint*)joint));
        if (handle)
        {
            InvalidateJointHandle(*handle);
        }
    }

    void ScriptBox2DFinalizeJoint()
    {
        TYPE_HASH_JOINT = 0;

        for (uint32_t i = 0; i < g_JointHandles.Capacity(); ++i)
        {
            if (g_JointHandles.GetByIndex(i))
            {
                g_JointHandles.Release(g_JointHandles.IndexToHandle(i));
            }
        }
        g_JointToHandle.Clear();
        g_JointMeta.Clear();
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
 * @version 2
 */

/*# Box2D joint
 * @typedef
 * @name b2Joint
 * @param value [type:userdata]
 */

/*# Unknown joint type.
 * @name b2d.joint.JOINT_TYPE_UNKNOWN
 * @constant
 */

/*# Revolute joint type.
 * @name b2d.joint.JOINT_TYPE_REVOLUTE
 * @constant
 */

/*# Prismatic joint type.
 * @name b2d.joint.JOINT_TYPE_PRISMATIC
 * @constant
 */

/*# Distance joint type.
 * @name b2d.joint.JOINT_TYPE_DISTANCE
 * @constant
 */

/*# Pulley joint type.
 * @name b2d.joint.JOINT_TYPE_PULLEY
 * @constant
 */

/*# Mouse joint type.
 * @name b2d.joint.JOINT_TYPE_MOUSE
 * @constant
 */

/*# Gear joint type.
 * @name b2d.joint.JOINT_TYPE_GEAR
 * @constant
 */

/*# Wheel joint type.
 * @name b2d.joint.JOINT_TYPE_WHEEL
 * @constant
 */

/*# Weld joint type.
 * @name b2d.joint.JOINT_TYPE_WELD
 * @constant
 */

/*# Friction joint type.
 * @name b2d.joint.JOINT_TYPE_FRICTION
 * @constant
 */

/*# Rope joint type.
 * @name b2d.joint.JOINT_TYPE_ROPE
 * @constant
 */

/*# Inactive limit state.
 * @name b2d.joint.LIMIT_STATE_INACTIVE
 * @constant
 */

/*# At lower limit state.
 * @name b2d.joint.LIMIT_STATE_AT_LOWER
 * @constant
 */

/*# At upper limit state.
 * @name b2d.joint.LIMIT_STATE_AT_UPPER
 * @constant
 */

/*# Equal limits state.
 * @name b2d.joint.LIMIT_STATE_EQUAL
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

/*# Get whether the joint is active.
 * @name b2d.joint.is_active
 * @param joint [type: b2Joint] joint
 * @return active [type: boolean] true if the joint is active
 */

/*# Get whether connected bodies can collide.
 * @name b2d.joint.get_collide_connected
 * @param joint [type: b2Joint] joint
 * @return collide [type: boolean] true if connected bodies can collide
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

/*# Set spring frequency.
 * @name b2d.joint.set_frequency
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @param frequency [type: number] frequency in hertz
 */

/*# Get spring frequency.
 * @name b2d.joint.get_frequency
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @return frequency [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.set_frequency`.
 * @name b2d.joint.set_hertz
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @param hertz [type: number] frequency in hertz
 */

/*# Alias for `b2d.joint.get_frequency`.
 * @name b2d.joint.get_hertz
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @return hertz [type: number] frequency in hertz
 */

/*# Set spring damping ratio.
 * @name b2d.joint.set_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Get spring damping ratio.
 * @name b2d.joint.get_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @return ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.set_damping_ratio`.
 * @name b2d.joint.set_spring_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @param ratio [type: number] damping ratio
 */

/*# Alias for `b2d.joint.get_damping_ratio`.
 * @name b2d.joint.get_spring_damping_ratio
 * @param joint [type: b2Joint] distance, mouse, weld, or wheel joint
 * @return ratio [type: number] damping ratio
 */

/*# Get the local axis on body A.
 * @name b2d.joint.get_local_axis_a
 * @param joint [type: b2Joint] prismatic or wheel joint
 * @return axis [type: vector3] local axis
 */

/*# Get the reference angle.
 * @name b2d.joint.get_reference_angle
 * @param joint [type: b2Joint] prismatic, revolute, or weld joint
 * @return angle [type: number] reference angle in radians
 */

/*# Get joint translation.
 * @name b2d.joint.get_joint_translation
 * @param joint [type: b2Joint] prismatic or wheel joint
 * @return translation [type: number] translation in project units
 */

/*# Get joint speed.
 * @name b2d.joint.get_joint_speed
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @return speed [type: number] joint speed
 */

/*# Get revolute joint angle.
 * @name b2d.joint.get_joint_angle
 * @param joint [type: b2Joint] revolute joint
 * @return angle [type: number] angle in radians
 */

/*# Enable or disable joint limits.
 * @name b2d.joint.enable_limit
 * @param joint [type: b2Joint] prismatic or revolute joint
 * @param enable [type: boolean] true to enable limits
 */

/*# Get whether joint limits are enabled.
 * @name b2d.joint.is_limit_enabled
 * @param joint [type: b2Joint] prismatic or revolute joint
 * @return enabled [type: boolean] true if limits are enabled
 */

/*# Set joint limits.
 * @name b2d.joint.set_limits
 * @param joint [type: b2Joint] prismatic or revolute joint
 * @param lower [type: number] lower limit
 * @param upper [type: number] upper limit
 */

/*# Get the lower joint limit.
 * @name b2d.joint.get_lower_limit
 * @param joint [type: b2Joint] prismatic or revolute joint
 * @return lower [type: number] lower limit
 */

/*# Get the upper joint limit.
 * @name b2d.joint.get_upper_limit
 * @param joint [type: b2Joint] prismatic or revolute joint
 * @return upper [type: number] upper limit
 */

/*# Enable or disable the joint motor.
 * @name b2d.joint.enable_motor
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @param enable [type: boolean] true to enable the motor
 */

/*# Get whether the joint motor is enabled.
 * @name b2d.joint.is_motor_enabled
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @return enabled [type: boolean] true if the motor is enabled
 */

/*# Set motor speed.
 * @name b2d.joint.set_motor_speed
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @param speed [type: number] motor speed
 */

/*# Get motor speed.
 * @name b2d.joint.get_motor_speed
 * @param joint [type: b2Joint] prismatic, revolute, or wheel joint
 * @return speed [type: number] motor speed
 */

/*# Set maximum motor force.
 * @name b2d.joint.set_max_motor_force
 * @param joint [type: b2Joint] prismatic joint
 * @param force [type: number] maximum motor force
 */

/*# Get maximum motor force.
 * @name b2d.joint.get_max_motor_force
 * @param joint [type: b2Joint] prismatic joint
 * @return force [type: number] maximum motor force
 */

/*# Get current motor force.
 * @name b2d.joint.get_motor_force
 * @param joint [type: b2Joint] prismatic joint
 * @param inv_dt [type: number] inverse time step
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
 * @param inv_dt [type: number] inverse time step
 * @return torque [type: number] motor torque
 */

/*# Set maximum force.
 * @name b2d.joint.set_max_force
 * @param joint [type: b2Joint] mouse or friction joint
 * @param force [type: number] maximum force
 */

/*# Get maximum force.
 * @name b2d.joint.get_max_force
 * @param joint [type: b2Joint] mouse or friction joint
 * @return force [type: number] maximum force
 */

/*# Set maximum torque.
 * @name b2d.joint.set_max_torque
 * @param joint [type: b2Joint] friction joint
 * @param torque [type: number] maximum torque
 */

/*# Get maximum torque.
 * @name b2d.joint.get_max_torque
 * @param joint [type: b2Joint] friction joint
 * @return torque [type: number] maximum torque
 */

/*# Set rope maximum length.
 * @name b2d.joint.set_max_length
 * @param joint [type: b2Joint] rope joint
 * @param length [type: number] maximum length in project units
 */

/*# Get rope maximum length.
 * @name b2d.joint.get_max_length
 * @param joint [type: b2Joint] rope joint
 * @return length [type: number] maximum length in project units
 */

/*# Get rope limit state.
 * @name b2d.joint.get_limit_state
 * @param joint [type: b2Joint] rope joint
 * @return state [type: number] one of the `LIMIT_STATE_*` constants
 */

/*# Get pulley ground anchor A.
 * @name b2d.joint.get_ground_anchor_a
 * @param joint [type: b2Joint] pulley joint
 * @return anchor [type: vector3] world anchor
 */

/*# Get pulley ground anchor B.
 * @name b2d.joint.get_ground_anchor_b
 * @param joint [type: b2Joint] pulley joint
 * @return anchor [type: vector3] world anchor
 */

/*# Get pulley segment length A.
 * @name b2d.joint.get_length_a
 * @param joint [type: b2Joint] pulley joint
 * @return length [type: number] length in project units
 */

/*# Get pulley segment length B.
 * @name b2d.joint.get_length_b
 * @param joint [type: b2Joint] pulley joint
 * @return length [type: number] length in project units
 */

/*# Set gear joint ratio.
 * @name b2d.joint.set_ratio
 * @param joint [type: b2Joint] gear joint
 * @param ratio [type: number] gear ratio
 */

/*# Get joint ratio.
 * @name b2d.joint.get_ratio
 * @param joint [type: b2Joint] pulley or gear joint
 * @return ratio [type: number] joint ratio
 */

/*# Get the first joint connected to a gear joint.
 * @name b2d.joint.get_joint1
 * @param joint [type: b2Joint] gear joint
 * @return joint1 [type: b2Joint] first connected joint
 */

/*# Get the second joint connected to a gear joint.
 * @name b2d.joint.get_joint2
 * @param joint [type: b2Joint] gear joint
 * @return joint2 [type: b2Joint] second connected joint
 */

/*# Get reaction force.
 * @name b2d.joint.get_reaction_force
 * @param joint [type: b2Joint] joint
 * @param inv_dt [type: number] inverse time step
 * @return force [type: vector3] reaction force
 */

/*# Get reaction torque.
 * @name b2d.joint.get_reaction_torque
 * @param joint [type: b2Joint] joint
 * @param inv_dt [type: number] inverse time step
 * @return torque [type: number] reaction torque
 */

/*# Create a distance joint.
 * @name b2d.joint.create_distance
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `length`, `frequency`, `damping_ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a mouse joint.
 * @name b2d.joint.create_mouse
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `target`, `max_force`, `frequency`, `damping_ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a prismatic joint.
 * @name b2d.joint.create_prismatic
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `local_axis_a`, `reference_angle`, `enable_limit`, `lower_translation`, `upper_translation`, `enable_motor`, `max_motor_force`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a revolute joint.
 * @name b2d.joint.create_revolute
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `reference_angle`, `enable_limit`, `lower_angle`, `upper_angle`, `enable_motor`, `max_motor_torque`, `motor_speed`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a weld joint.
 * @name b2d.joint.create_weld
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `reference_angle`, `frequency`, `damping_ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a wheel joint.
 * @name b2d.joint.create_wheel
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `local_axis_a`, `enable_motor`, `max_motor_torque`, `motor_speed`, `frequency`, `damping_ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a friction joint.
 * @name b2d.joint.create_friction
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `max_force`, `max_torque`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a rope joint.
 * @name b2d.joint.create_rope
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `local_anchor_a`, `local_anchor_b`, `max_length`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a pulley joint.
 * @name b2d.joint.create_pulley
 * @param body_a [type: b2Body] first body
 * @param body_b [type: b2Body] second body
 * @param definition [type: table] optional definition with `ground_anchor_a`, `ground_anchor_b`, `local_anchor_a`, `local_anchor_b`, `length_a`, `length_b`, `ratio`, and `collide_connected`
 * @return joint [type: b2Joint] created joint
 */

/*# Create a gear joint.
 * @name b2d.joint.create_gear
 * @param joint1 [type: b2Joint] first revolute or prismatic joint
 * @param joint2 [type: b2Joint] second revolute or prismatic joint
 * @param definition [type: table] optional definition with `ratio`
 * @return joint [type: b2Joint] created joint
 */
