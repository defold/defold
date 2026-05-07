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

    static B2DJointMeta* GetJointMeta(lua_State* L, B2DLuaJoint* luajoint)
    {
        B2DJointMeta* joint_meta = g_JointMeta.Get(luajoint->m_Handle);
        if (!joint_meta)
        {
            luaL_error(L, "Invalid b2joint handle.");
            return 0;
        }
        return joint_meta;
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
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        b2Joint* joint = CheckJoint(L, 1);
        b2World* world = joint->GetBodyA()->GetWorld();
        if (world->IsLocked())
        {
            return luaL_error(L, "Could not destroy joint. The world is locked.");
        }
        world->DestroyJoint(joint);
        InvalidateJointHandle(luajoint->m_Handle);
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
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        B2DJointMeta* joint_meta = GetJointMeta(L, luajoint);
        b2Body* body = CheckJoint(L, 1)->GetBodyA();
        PushBody(L, body, joint_meta ? joint_meta->m_Collection : 0, GetBodyInstanceId(body));
        return 1;
    }

    static int Joint_GetBodyB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        B2DJointMeta* joint_meta = GetJointMeta(L, luajoint);
        b2Body* body = CheckJoint(L, 1)->GetBodyB();
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

    static int Joint_GetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckJoint(L, 1)->GetCollideConnected());
        return 1;
    }

    static int Joint_SetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckJoint(L, 1);
        return luaL_error(L, "b2d.joint.set_collide_connected is only supported by Box2D v3.");
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
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque);
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
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque);
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
        def.maxTorque = GetNumberField(L, def_index, "max_torque", def.maxTorque);

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
        B2DLuaJoint* luajoint = CheckJointInternal(L, 1);
        B2DJointMeta* joint_meta = GetJointMeta(L, luajoint);
        b2Joint* joint1 = CheckJoint(L, 1);
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
        {"get_collide_connected", Joint_GetCollideConnected},
        {"set_collide_connected", Joint_SetCollideConnected},
        {"set_mouse_target", Joint_SetMouseTarget},
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
