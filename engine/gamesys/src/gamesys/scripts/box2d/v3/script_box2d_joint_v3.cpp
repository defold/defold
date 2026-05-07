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
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque);
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
        def.maxMotorTorque = GetNumberField(L, def_index, "max_motor_torque", def.maxMotorTorque);
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
        def.maxTorque = GetNumberField(L, def_index, "max_torque", def.maxTorque);
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
