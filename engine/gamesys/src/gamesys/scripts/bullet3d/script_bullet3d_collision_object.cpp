// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <stdint.h>

#include <dmsdk/dlib/hashtable.h>
#include <gameobject/script.h>
#include <script/script.h>

#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

//////////////////////////////////////////////////////////////////////////////
// btCollisionObject
namespace dmGameSystem
{
#define BULLET3D_TYPE_NAME_COLLISION_OBJECT "bullet3d_collision_object"

    static uint32_t TYPE_HASH_COLLISION_OBJECT = 0;

    struct Bullet3DLuaCollisionObject
    {
        uint64_t m_Id;
    };

    struct Bullet3DCollisionObjectMeta
    {
        dmGameObject::HCollection m_Collection;
        dmhash_t                  m_InstanceId;
        uint32_t                  m_InstanceGeneration;
    };

    // Bullet collision objects are raw pointers. Assign each live pointer a
    // monotonically increasing identity so stale Lua userdata cannot revive
    // when a native address is reused.
    static uint64_t                                   g_NextBullet3DCollisionObjectId = 0;
    static dmHashTable64<btCollisionObject*>          g_Bullet3DCollisionObjects;
    static dmHashTable64<uint64_t>                    g_Bullet3DCollisionObjectToId;
    static dmHashTable64<Bullet3DCollisionObjectMeta> g_Bullet3DCollisionObjectMeta;

    static uint64_t                                   CollisionObjectPtrToKey(const btCollisionObject* collision_object)
    {
        return (uint64_t)(uintptr_t)collision_object;
    }

    static void EnsureCollisionObjectCapacity()
    {
        if (g_Bullet3DCollisionObjects.Full())
        {
            g_Bullet3DCollisionObjects.OffsetCapacity(32);
            g_Bullet3DCollisionObjectToId.OffsetCapacity(32);
            g_Bullet3DCollisionObjectMeta.OffsetCapacity(32);
        }
    }

    static uint64_t AllocateCollisionObjectId(lua_State* L)
    {
        if (g_NextBullet3DCollisionObjectId == UINT64_MAX)
        {
            luaL_error(L, "The bullet3d collision object identity space is exhausted.");
            return 0;
        }
        return ++g_NextBullet3DCollisionObjectId;
    }

    static void InvalidateCollisionObjectId(uint64_t id)
    {
        btCollisionObject** collision_object = g_Bullet3DCollisionObjects.Get(id);
        if (!collision_object)
        {
            return;
        }

        uint64_t  key = CollisionObjectPtrToKey(*collision_object);
        uint64_t* mapped_id = g_Bullet3DCollisionObjectToId.Get(key);
        if (mapped_id && *mapped_id == id)
        {
            g_Bullet3DCollisionObjectToId.Erase(key);
        }
        if (g_Bullet3DCollisionObjectMeta.Get(id))
        {
            g_Bullet3DCollisionObjectMeta.Erase(id);
        }
        g_Bullet3DCollisionObjects.Erase(id);
    }

    static Bullet3DLuaCollisionObject* CheckCollisionObjectInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaCollisionObject*)dmScript::CheckUserType(L, index, TYPE_HASH_COLLISION_OBJECT, "Expected user type " BULLET3D_TYPE_NAME_COLLISION_OBJECT);
    }

    static Bullet3DLuaCollisionObject* ToCollisionObjectInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaCollisionObject*)dmScript::ToUserType(L, index, TYPE_HASH_COLLISION_OBJECT);
    }

    static btCollisionObject* VerifyCollisionObjectInternal(lua_State* L, Bullet3DLuaCollisionObject* lua_collision_object, bool report_error, Bullet3DCollisionObjectMeta** out_meta)
    {
        btCollisionObject**          collision_object = g_Bullet3DCollisionObjects.Get(lua_collision_object->m_Id);
        Bullet3DCollisionObjectMeta* meta = g_Bullet3DCollisionObjectMeta.Get(lua_collision_object->m_Id);
        if (!collision_object || !meta)
        {
            if (report_error)
            {
                luaL_error(L, "Invalid bullet3d collision object handle.");
            }
            return 0;
        }

        if (meta->m_InstanceId)
        {
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(meta->m_Collection, meta->m_InstanceId);
            if (!instance || dmGameObject::GetGeneration(instance) != meta->m_InstanceGeneration)
            {
                dmhash_t instance_id = meta->m_InstanceId;
                InvalidateCollisionObjectId(lua_collision_object->m_Id);
                if (report_error)
                {
                    luaL_error(L, "Cannot get bullet3d collision object for game object instance '%s'. Has the game object been deleted?", dmHashReverseSafe64(instance_id));
                }
                return 0;
            }
        }

        if (out_meta)
        {
            *out_meta = meta;
        }
        return *collision_object;
    }

    btCollisionObject* ToBullet3DCollisionObject(lua_State* L, int index)
    {
        Bullet3DLuaCollisionObject* lua_collision_object = ToCollisionObjectInternal(L, index);
        return lua_collision_object ? VerifyCollisionObjectInternal(L, lua_collision_object, false, 0) : 0;
    }

    bool IsBullet3DCollisionObjectValid(lua_State* L, int index)
    {
        return ToBullet3DCollisionObject(L, index) != 0;
    }

    btCollisionObject* CheckBullet3DCollisionObject(lua_State* L, int index)
    {
        return VerifyCollisionObjectInternal(L, CheckCollisionObjectInternal(L, index), true, 0);
    }

    uint64_t CheckBullet3DCollisionObjectId(lua_State* L, int index)
    {
        Bullet3DLuaCollisionObject* collision_object = CheckCollisionObjectInternal(L, index);
        VerifyCollisionObjectInternal(L, collision_object, true, 0);
        return collision_object->m_Id;
    }

    btCollisionObject* ToBullet3DCollisionObjectById(lua_State* L, uint64_t id)
    {
        Bullet3DLuaCollisionObject collision_object = { id };
        return VerifyCollisionObjectInternal(L, &collision_object, false, 0);
    }

    dmGameObject::HCollection GetBullet3DCollisionObjectCollectionById(lua_State* L, uint64_t id)
    {
        Bullet3DLuaCollisionObject collision_object = { id };
        Bullet3DCollisionObjectMeta* meta = 0;
        VerifyCollisionObjectInternal(L, &collision_object, true, &meta);
        return meta ? meta->m_Collection : 0;
    }

    void PushBullet3DCollisionObjectById(lua_State* L, uint64_t id)
    {
        Bullet3DLuaCollisionObject collision_object = { id };
        if (!VerifyCollisionObjectInternal(L, &collision_object, false, 0))
        {
            lua_pushnil(L);
            return;
        }

        Bullet3DLuaCollisionObject* lua_collision_object = (Bullet3DLuaCollisionObject*)lua_newuserdata(L, sizeof(Bullet3DLuaCollisionObject));
        lua_collision_object->m_Id = id;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_COLLISION_OBJECT);
        lua_setmetatable(L, -2);
    }

    static btCollisionObject* CheckCollisionObjectWithMeta(lua_State* L, int index, Bullet3DCollisionObjectMeta** out_meta)
    {
        return VerifyCollisionObjectInternal(L, CheckCollisionObjectInternal(L, index), true, out_meta);
    }

    dmGameObject::HCollection GetBullet3DCollisionObjectCollection(lua_State* L, int index)
    {
        Bullet3DCollisionObjectMeta* meta = 0;
        VerifyCollisionObjectInternal(L, CheckCollisionObjectInternal(L, index), true, &meta);
        return meta ? meta->m_Collection : 0;
    }

    void PushBullet3DCollisionObject(lua_State* L, void* collision_object_ptr, dmGameObject::HCollection collection, dmhash_t instance_id)
    {
        if (!collision_object_ptr)
        {
            lua_pushnil(L);
            return;
        }

        btCollisionObject*      collision_object = (btCollisionObject*)collision_object_ptr;
        dmGameObject::HInstance instance = instance_id ? dmGameObject::GetInstanceFromIdentifier(collection, instance_id) : 0;
        uint32_t                instance_generation = instance ? dmGameObject::GetGeneration(instance) : 0;

        EnsureCollisionObjectCapacity();

        uint64_t            id = 0;
        uint64_t            key = CollisionObjectPtrToKey(collision_object);
        uint64_t*           existing_id = g_Bullet3DCollisionObjectToId.Get(key);
        btCollisionObject** existing_collision_object = existing_id ? g_Bullet3DCollisionObjects.Get(*existing_id) : 0;
        if (existing_collision_object)
        {
            Bullet3DCollisionObjectMeta* existing_meta = g_Bullet3DCollisionObjectMeta.Get(*existing_id);
            dmGameObject::HInstance      existing_instance = existing_meta && existing_meta->m_InstanceId ? dmGameObject::GetInstanceFromIdentifier(existing_meta->m_Collection, existing_meta->m_InstanceId) : 0;
            if (!existing_meta || (existing_meta->m_InstanceId && (!existing_instance || dmGameObject::GetGeneration(existing_instance) != existing_meta->m_InstanceGeneration)))
            {
                InvalidateCollisionObjectId(*existing_id);
                existing_id = 0;
                existing_collision_object = 0;
            }
        }

        if (existing_collision_object)
        {
            id = *existing_id;
        }
        else
        {
            if (existing_id)
            {
                g_Bullet3DCollisionObjectToId.Erase(key);
            }
            id = AllocateCollisionObjectId(L);
            g_Bullet3DCollisionObjects.Put(id, collision_object);
            g_Bullet3DCollisionObjectToId.Put(key, id);
        }

        Bullet3DCollisionObjectMeta meta = {};
        meta.m_Collection = collection;
        meta.m_InstanceId = instance_id;
        meta.m_InstanceGeneration = instance_generation;
        Bullet3DCollisionObjectMeta* existing_meta = g_Bullet3DCollisionObjectMeta.Get(id);
        if (existing_meta)
        {
            *existing_meta = meta;
        }
        else
        {
            g_Bullet3DCollisionObjectMeta.Put(id, meta);
        }

        Bullet3DLuaCollisionObject* lua_collision_object = (Bullet3DLuaCollisionObject*)lua_newuserdata(L, sizeof(Bullet3DLuaCollisionObject));
        lua_collision_object->m_Id = id;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_COLLISION_OBJECT);
        lua_setmetatable(L, -2);
    }

    void ScriptBullet3DInvalidateCollisionObject(void* collision_object_ptr)
    {
        if (!collision_object_ptr)
        {
            return;
        }

        uint64_t* id = g_Bullet3DCollisionObjectToId.Get(CollisionObjectPtrToKey((btCollisionObject*)collision_object_ptr));
        if (id)
        {
            InvalidateCollisionObjectId(*id);
        }
    }

    static void SetWorldTransform(btCollisionObject* collision_object, Bullet3DCollisionObjectMeta* meta, const btVector3& position, const btQuaternion& rotation)
    {
        btTransform  transform(rotation, position);
        btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
        if (rigid_body)
        {
            rigid_body->setCenterOfMassTransform(transform);
        }
        else
        {
            collision_object->setWorldTransform(transform);
        }
        collision_object->setInterpolationWorldTransform(transform);
        collision_object->activate(true);

        if (meta && meta->m_InstanceId)
        {
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(meta->m_Collection, meta->m_InstanceId);
            if (instance)
            {
                float                   inv_scale = GetBullet3DInvPhysicsScale();
                dmVMath::Point3         world_position(position.getX() * inv_scale, position.getY() * inv_scale, position.getZ() * inv_scale);
                dmVMath::Quat           world_rotation(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
                dmGameObject::HInstance parent = dmGameObject::GetParent(instance);
                if (parent)
                {
                    dmVMath::Matrix4 inverse_parent = dmVMath::Inverse(dmGameObject::GetWorldMatrix(parent));
                    dmVMath::Vector4 local_position = inverse_parent * dmVMath::Vector4(world_position);
                    dmGameObject::SetPosition(instance, dmVMath::Point3(local_position.getXYZ()));
                    dmGameObject::SetRotation(instance, dmVMath::Conjugate(dmGameObject::GetWorldRotation(parent)) * world_rotation);
                }
                else
                {
                    dmGameObject::SetPosition(instance, world_position);
                    dmGameObject::SetRotation(instance, world_rotation);
                }
            }
        }
    }

    static int CollisionObject_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, IsBullet3DCollisionObjectValid(L, 1));
        return 1;
    }

    static int CollisionObject_GetWorldTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        const btTransform& transform = CheckBullet3DCollisionObject(L, 1)->getWorldTransform();
        PushBullet3DVector3(L, transform.getOrigin(), GetBullet3DInvPhysicsScale());
        PushBullet3DQuat(L, transform.getRotation());
        return 2;
    }

    static int CollisionObject_SetWorldTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DCollisionObjectMeta* meta = 0;
        btCollisionObject*           collision_object = CheckCollisionObjectWithMeta(L, 1, &meta);
        SetWorldTransform(collision_object, meta, CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()), CheckBullet3DFiniteQuat(L, 3, "rotation"));
        return 0;
    }

    static int CollisionObject_GetPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DCollisionObject(L, 1)->getWorldTransform().getOrigin(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int CollisionObject_SetPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DCollisionObjectMeta* meta = 0;
        btCollisionObject*           collision_object = CheckCollisionObjectWithMeta(L, 1, &meta);
        const btTransform&           transform = collision_object->getWorldTransform();
        SetWorldTransform(collision_object, meta, CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()), transform.getRotation());
        return 0;
    }

    static int CollisionObject_GetRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DQuat(L, CheckBullet3DCollisionObject(L, 1)->getWorldTransform().getRotation());
        return 1;
    }

    static int CollisionObject_SetRotation(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DCollisionObjectMeta* meta = 0;
        btCollisionObject*           collision_object = CheckCollisionObjectWithMeta(L, 1, &meta);
        SetWorldTransform(collision_object, meta, collision_object->getWorldTransform().getOrigin(), CheckBullet3DFiniteQuat(L, 2, "rotation"));
        return 0;
    }

    static int CollisionObject_GetActivationState(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DCollisionObject(L, 1)->getActivationState());
        return 1;
    }

    static int CollisionObject_SetActivationState(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DCollisionObject(L, 1)->setActivationState(luaL_checkinteger(L, 2));
        return 0;
    }

    static int CollisionObject_ForceActivationState(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DCollisionObject(L, 1)->forceActivationState(luaL_checkinteger(L, 2));
        return 0;
    }

    static int CollisionObject_Activate(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DCollisionObject(L, 1)->activate(lua_toboolean(L, 2) != 0);
        return 0;
    }

    static int CollisionObject_IsActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckBullet3DCollisionObject(L, 1)->isActive());
        return 1;
    }

    static int CollisionObject_GetDeactivationTime(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DCollisionObject(L, 1)->getDeactivationTime());
        return 1;
    }

    static int CollisionObject_SetDeactivationTime(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DCollisionObject(L, 1)->setDeactivationTime(luaL_checknumber(L, 2));
        return 0;
    }

#define BULLET3D_NUMBER_PROPERTY(NAME, GETTER, SETTER, INPUT_SCALE, OUTPUT_SCALE) \
    static int CollisionObject_Get##NAME(lua_State* L) \
    { \
        DM_LUA_STACK_CHECK(L, 1); \
        lua_pushnumber(L, CheckBullet3DCollisionObject(L, 1)->GETTER() * (OUTPUT_SCALE)); \
        return 1; \
    } \
    static int CollisionObject_Set##NAME(lua_State* L) \
    { \
        DM_LUA_STACK_CHECK(L, 0); \
        CheckBullet3DCollisionObject(L, 1)->SETTER(luaL_checknumber(L, 2) * (INPUT_SCALE)); \
        return 0; \
    }

    BULLET3D_NUMBER_PROPERTY(Friction, getFriction, setFriction, 1.0f, 1.0f)
    BULLET3D_NUMBER_PROPERTY(Restitution, getRestitution, setRestitution, 1.0f, 1.0f)
    BULLET3D_NUMBER_PROPERTY(ContactProcessingThreshold, getContactProcessingThreshold, setContactProcessingThreshold, GetBullet3DPhysicsScale(), GetBullet3DInvPhysicsScale())
    BULLET3D_NUMBER_PROPERTY(CCDSweptSphereRadius, getCcdSweptSphereRadius, setCcdSweptSphereRadius, GetBullet3DPhysicsScale(), GetBullet3DInvPhysicsScale())
    BULLET3D_NUMBER_PROPERTY(CCDMotionThreshold, getCcdMotionThreshold, setCcdMotionThreshold, GetBullet3DPhysicsScale(), GetBullet3DInvPhysicsScale())

#undef BULLET3D_NUMBER_PROPERTY

    static int CollisionObject_GetCollisionFlags(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DCollisionObject(L, 1)->getCollisionFlags());
        return 1;
    }

    static const btBroadphaseProxy* CheckCollisionObjectBroadphaseHandle(lua_State* L, int index)
    {
        const btBroadphaseProxy* broadphase_handle = CheckBullet3DCollisionObject(L, index)->getBroadphaseHandle();
        if (!broadphase_handle)
        {
            luaL_error(L, "Cannot get collision filter: bullet3d collision object is not in a world and has no broadphase handle.");
        }
        return broadphase_handle;
    }

    static int CollisionObject_GetCollisionFilterGroup(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, (uint16_t)CheckCollisionObjectBroadphaseHandle(L, 1)->m_collisionFilterGroup);
        return 1;
    }

    static int CollisionObject_GetCollisionFilterMask(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, (uint16_t)CheckCollisionObjectBroadphaseHandle(L, 1)->m_collisionFilterMask);
        return 1;
    }

    static int CollisionObject_HasCollisionFlag(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        int flag = luaL_checkinteger(L, 2);
        lua_pushboolean(L, (CheckBullet3DCollisionObject(L, 1)->getCollisionFlags() & flag) == flag);
        return 1;
    }

    static int CollisionObject_GetInternalType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DCollisionObject(L, 1)->getInternalType());
        return 1;
    }

#define BULLET3D_BOOL_QUERY(NAME, EXPRESSION) \
    static int CollisionObject_##NAME(lua_State* L) \
    { \
        DM_LUA_STACK_CHECK(L, 1); \
        btCollisionObject* collision_object = CheckBullet3DCollisionObject(L, 1); \
        lua_pushboolean(L, (EXPRESSION)); \
        return 1; \
    }

    BULLET3D_BOOL_QUERY(IsStatic, collision_object->isStaticObject())
    BULLET3D_BOOL_QUERY(IsKinematic, collision_object->isKinematicObject())
    BULLET3D_BOOL_QUERY(IsStaticOrKinematic, collision_object->isStaticOrKinematicObject())
    BULLET3D_BOOL_QUERY(HasContactResponse, collision_object->hasContactResponse())
    BULLET3D_BOOL_QUERY(IsRigidBody, btRigidBody::upcast(collision_object) != 0)
    BULLET3D_BOOL_QUERY(IsGhostObject, collision_object->getInternalType() == btCollisionObject::CO_GHOST_OBJECT)

#undef BULLET3D_BOOL_QUERY

    static int CollisionObject_tostring(lua_State* L)
    {
        btCollisionObject* collision_object = CheckBullet3DCollisionObject(L, 1);
        lua_pushfstring(L, "Bullet3D.%s = %p", BULLET3D_TYPE_NAME_COLLISION_OBJECT, collision_object);
        return 1;
    }

    static int CollisionObject_eq(lua_State* L)
    {
        Bullet3DLuaCollisionObject* a = ToCollisionObjectInternal(L, 1);
        Bullet3DLuaCollisionObject* b = ToCollisionObjectInternal(L, 2);
        lua_pushboolean(L, a && b && a->m_Id == b->m_Id);
        return 1;
    }

    static const luaL_reg CollisionObject_methods[] = {
        { 0, 0 }
    };

    static const luaL_reg CollisionObject_meta[] = {
        { "__tostring", CollisionObject_tostring },
        { "__eq", CollisionObject_eq },
        { 0, 0 }
    };

    static const luaL_reg CollisionObject_functions[] = {
        { "is_valid", CollisionObject_IsValid },

        { "get_world_transform", CollisionObject_GetWorldTransform },
        { "set_world_transform", CollisionObject_SetWorldTransform },

        { "get_position", CollisionObject_GetPosition },
        { "set_position", CollisionObject_SetPosition },

        { "get_rotation", CollisionObject_GetRotation },
        { "set_rotation", CollisionObject_SetRotation },

        { "get_activation_state", CollisionObject_GetActivationState },
        { "set_activation_state", CollisionObject_SetActivationState },
        { "force_activation_state", CollisionObject_ForceActivationState },
        { "activate", CollisionObject_Activate },
        { "is_active", CollisionObject_IsActive },
        { "get_deactivation_time", CollisionObject_GetDeactivationTime },
        { "set_deactivation_time", CollisionObject_SetDeactivationTime },

        { "get_friction", CollisionObject_GetFriction },
        { "set_friction", CollisionObject_SetFriction },

        { "get_restitution", CollisionObject_GetRestitution },
        { "set_restitution", CollisionObject_SetRestitution },

        { "get_contact_processing_threshold", CollisionObject_GetContactProcessingThreshold },
        { "set_contact_processing_threshold", CollisionObject_SetContactProcessingThreshold },

        { "get_ccd_swept_sphere_radius", CollisionObject_GetCCDSweptSphereRadius },
        { "set_ccd_swept_sphere_radius", CollisionObject_SetCCDSweptSphereRadius },

        { "get_ccd_motion_threshold", CollisionObject_GetCCDMotionThreshold },
        { "set_ccd_motion_threshold", CollisionObject_SetCCDMotionThreshold },

        { "get_collision_flags", CollisionObject_GetCollisionFlags },
        { "has_collision_flag", CollisionObject_HasCollisionFlag },
        { "get_collision_filter_group", CollisionObject_GetCollisionFilterGroup },
        { "get_collision_filter_mask", CollisionObject_GetCollisionFilterMask },

        { "get_internal_type", CollisionObject_GetInternalType },
        { "is_static", CollisionObject_IsStatic },
        { "is_kinematic", CollisionObject_IsKinematic },
        { "is_static_or_kinematic", CollisionObject_IsStaticOrKinematic },
        { "has_contact_response", CollisionObject_HasContactResponse },
        { "is_rigid_body", CollisionObject_IsRigidBody },
        { "is_ghost_object", CollisionObject_IsGhostObject },
        { 0, 0 }
    };

    static void SetIntegerConstant(lua_State* L, const char* name, int value)
    {
        lua_pushinteger(L, value);
        lua_setfield(L, -2, name);
    }

    void ScriptBullet3DInitializeCollisionObject(lua_State* L)
    {
        TYPE_HASH_COLLISION_OBJECT = dmScript::RegisterUserType(L, BULLET3D_TYPE_NAME_COLLISION_OBJECT, CollisionObject_methods, CollisionObject_meta);

        lua_newtable(L);
        luaL_register(L, 0, CollisionObject_functions);

        SetIntegerConstant(L, "ACTIVE_TAG", ACTIVE_TAG);
        SetIntegerConstant(L, "ISLAND_SLEEPING", ISLAND_SLEEPING);
        SetIntegerConstant(L, "WANTS_DEACTIVATION", WANTS_DEACTIVATION);
        SetIntegerConstant(L, "DISABLE_DEACTIVATION", DISABLE_DEACTIVATION);
        SetIntegerConstant(L, "DISABLE_SIMULATION", DISABLE_SIMULATION);

        SetIntegerConstant(L, "CF_STATIC_OBJECT", btCollisionObject::CF_STATIC_OBJECT);
        SetIntegerConstant(L, "CF_KINEMATIC_OBJECT", btCollisionObject::CF_KINEMATIC_OBJECT);
        SetIntegerConstant(L, "CF_NO_CONTACT_RESPONSE", btCollisionObject::CF_NO_CONTACT_RESPONSE);
        SetIntegerConstant(L, "CF_CUSTOM_MATERIAL_CALLBACK", btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
        SetIntegerConstant(L, "CF_CHARACTER_OBJECT", btCollisionObject::CF_CHARACTER_OBJECT);
        SetIntegerConstant(L, "CF_DISABLE_VISUALIZE_OBJECT", btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
        SetIntegerConstant(L, "CF_DISABLE_SPU_COLLISION_PROCESSING", btCollisionObject::CF_DISABLE_SPU_COLLISION_PROCESSING);

        SetIntegerConstant(L, "CO_COLLISION_OBJECT", btCollisionObject::CO_COLLISION_OBJECT);
        SetIntegerConstant(L, "CO_RIGID_BODY", btCollisionObject::CO_RIGID_BODY);
        SetIntegerConstant(L, "CO_GHOST_OBJECT", btCollisionObject::CO_GHOST_OBJECT);
        SetIntegerConstant(L, "CO_SOFT_BODY", btCollisionObject::CO_SOFT_BODY);
        SetIntegerConstant(L, "CO_HF_FLUID", btCollisionObject::CO_HF_FLUID);

        lua_setfield(L, -2, "collision_object");
    }

    void ScriptBullet3DFinalizeCollisionObject()
    {
        TYPE_HASH_COLLISION_OBJECT = 0;
        g_Bullet3DCollisionObjects.Clear();
        g_Bullet3DCollisionObjectToId.Clear();
        g_Bullet3DCollisionObjectMeta.Clear();
    }
} // namespace dmGameSystem

/*# Bullet collision object API
 *
 * Functions shared by rigid bodies and trigger ghost objects. Defold keeps
 * ownership of each object's user pointer, motion state, collision shape, and
 * world membership. Logical child shapes are inspected and mutated through
 * `bullet3d.shape`; ownership is not transferred to Lua.
 *
 * Positions, distances, and CCD thresholds use Defold units. Rotations,
 * coefficients, flags, activation state, and time values use Bullet values.
 *
 * @document
 * @name bullet3d.collision_object
 * @namespace bullet3d.collision_object
 * @language Lua
 */

/*# Active simulation state
 * @name bullet3d.collision_object.ACTIVE_TAG
 * @constant
 */
/*# Sleeping simulation state
 * @name bullet3d.collision_object.ISLAND_SLEEPING
 * @constant
 */
/*# Wants-deactivation simulation state
 * @name bullet3d.collision_object.WANTS_DEACTIVATION
 * @constant
 */
/*# Disable automatic deactivation
 * @name bullet3d.collision_object.DISABLE_DEACTIVATION
 * @constant
 */
/*# Disable simulation
 * @name bullet3d.collision_object.DISABLE_SIMULATION
 * @constant
 */

/*# Static collision object flag
 * @name bullet3d.collision_object.CF_STATIC_OBJECT
 * @constant
 */
/*# Kinematic collision object flag
 * @name bullet3d.collision_object.CF_KINEMATIC_OBJECT
 * @constant
 */
/*# Disable contact response flag
 * @name bullet3d.collision_object.CF_NO_CONTACT_RESPONSE
 * @constant
 */
/*# Custom material callback flag
 * @name bullet3d.collision_object.CF_CUSTOM_MATERIAL_CALLBACK
 * @constant
 */
/*# Character collision object flag
 * @name bullet3d.collision_object.CF_CHARACTER_OBJECT
 * @constant
 */
/*# Disable debug visualization flag
 * @name bullet3d.collision_object.CF_DISABLE_VISUALIZE_OBJECT
 * @constant
 */
/*# Disable SPU collision processing flag
 * @name bullet3d.collision_object.CF_DISABLE_SPU_COLLISION_PROCESSING
 * @constant
 */

/*# Generic collision object type
 * @name bullet3d.collision_object.CO_COLLISION_OBJECT
 * @constant
 */
/*# Rigid body collision object type
 * @name bullet3d.collision_object.CO_RIGID_BODY
 * @constant
 */
/*# Ghost collision object type
 * @name bullet3d.collision_object.CO_GHOST_OBJECT
 * @constant
 */
/*# Soft body collision object type
 * @name bullet3d.collision_object.CO_SOFT_BODY
 * @constant
 */
/*# Height-field fluid collision object type
 * @name bullet3d.collision_object.CO_HF_FLUID
 * @constant
 */

/*# Test whether a collision object handle is valid
 * @name bullet3d.collision_object.is_valid
 * @param object [type:btCollisionObject] collision object
 * @return valid [type:boolean] `true` if the native object still exists
 */

/*# Get the world transform
 * @name bullet3d.collision_object.get_world_transform
 * @param object [type:btCollisionObject] collision object
 * @return position [type:vector3] world position in Defold units
 * @return rotation [type:quaternion] world rotation
 */

/*# Set the world transform
 *
 * The owning game object's position and rotation are updated as well, so the
 * transform persists when Defold synchronizes game objects into Bullet.
 *
 * @name bullet3d.collision_object.set_world_transform
 * @param object [type:btCollisionObject] collision object
 * @param position [type:vector3] world position in Defold units
 * @param rotation [type:quaternion] world rotation
 */

/*# Get the world position
 * @name bullet3d.collision_object.get_position
 * @param object [type:btCollisionObject] collision object
 * @return position [type:vector3] world position in Defold units
 */

/*# Set the world position
 *
 * The owning game object's position is updated as well.
 *
 * @name bullet3d.collision_object.set_position
 * @param object [type:btCollisionObject] collision object
 * @param position [type:vector3] world position in Defold units
 */

/*# Get the world rotation
 * @name bullet3d.collision_object.get_rotation
 * @param object [type:btCollisionObject] collision object
 * @return rotation [type:quaternion] world rotation
 */

/*# Set the world rotation
 *
 * The owning game object's rotation is updated as well.
 *
 * @name bullet3d.collision_object.set_rotation
 * @param object [type:btCollisionObject] collision object
 * @param rotation [type:quaternion] world rotation
 */

/*# Get the activation state
 * @name bullet3d.collision_object.get_activation_state
 * @param object [type:btCollisionObject] collision object
 * @return state [type:number] one of the activation constants
 */

/*# Set the activation state
 * @name bullet3d.collision_object.set_activation_state
 * @param object [type:btCollisionObject] collision object
 * @param state [type:number] activation state
 */

/*# Force the activation state
 * @name bullet3d.collision_object.force_activation_state
 * @param object [type:btCollisionObject] collision object
 * @param state [type:number] activation state
 */

/*# Activate a collision object
 * @name bullet3d.collision_object.activate
 * @param object [type:btCollisionObject] collision object
 * @param [force] [type:boolean] force activation of a static or kinematic object; defaults to `false`
 */

/*# Test whether a collision object is active
 * @name bullet3d.collision_object.is_active
 * @param object [type:btCollisionObject] collision object
 * @return active [type:boolean] active state
 */

/*# Get deactivation time
 * @name bullet3d.collision_object.get_deactivation_time
 * @param object [type:btCollisionObject] collision object
 * @return seconds [type:number] deactivation time
 */

/*# Set deactivation time
 * @name bullet3d.collision_object.set_deactivation_time
 * @param object [type:btCollisionObject] collision object
 * @param seconds [type:number] deactivation time
 */

/*# Get friction
 * @name bullet3d.collision_object.get_friction
 * @param object [type:btCollisionObject] collision object
 * @return friction [type:number] friction coefficient
 */

/*# Set friction
 * @name bullet3d.collision_object.set_friction
 * @param object [type:btCollisionObject] collision object
 * @param friction [type:number] friction coefficient
 */

/*# Get restitution
 * @name bullet3d.collision_object.get_restitution
 * @param object [type:btCollisionObject] collision object
 * @return restitution [type:number] restitution coefficient
 */

/*# Set restitution
 * @name bullet3d.collision_object.set_restitution
 * @param object [type:btCollisionObject] collision object
 * @param restitution [type:number] restitution coefficient
 */

/*# Get the contact processing threshold
 * @name bullet3d.collision_object.get_contact_processing_threshold
 * @param object [type:btCollisionObject] collision object
 * @return threshold [type:number] threshold in Defold units
 */

/*# Set the contact processing threshold
 * @name bullet3d.collision_object.set_contact_processing_threshold
 * @param object [type:btCollisionObject] collision object
 * @param threshold [type:number] threshold in Defold units
 */

/*# Get the CCD swept sphere radius
 * @name bullet3d.collision_object.get_ccd_swept_sphere_radius
 * @param object [type:btCollisionObject] collision object
 * @return radius [type:number] radius in Defold units
 */

/*# Set the CCD swept sphere radius
 * @name bullet3d.collision_object.set_ccd_swept_sphere_radius
 * @param object [type:btCollisionObject] collision object
 * @param radius [type:number] radius in Defold units
 */

/*# Get the CCD motion threshold
 * @name bullet3d.collision_object.get_ccd_motion_threshold
 * @param object [type:btCollisionObject] collision object
 * @return threshold [type:number] threshold in Defold units
 */

/*# Set the CCD motion threshold
 * @name bullet3d.collision_object.set_ccd_motion_threshold
 * @param object [type:btCollisionObject] collision object
 * @param threshold [type:number] threshold in Defold units
 */

/*# Get collision flags
 * @name bullet3d.collision_object.get_collision_flags
 * @param object [type:btCollisionObject] collision object
 * @return flags [type:number] bit field of `CF_*` constants
 */

/*# Get the collision filter group
 *
 * Returns the raw unsigned 16-bit filter group that Defold assigned to the
 * object's Bullet broadphase proxy. Use this value as `category_bits` in a
 * `bullet3d.world` query filter. Bullet applies reciprocal filtering: the
 * query's `mask_bits` must include this group, and the query's `category_bits`
 * must be included in the object's filter mask.
 *
 * @name bullet3d.collision_object.get_collision_filter_group
 * @param object [type:btCollisionObject] collision object in a Bullet world
 * @return group [type:number] raw unsigned 16-bit collision filter group
 */

/*# Get the collision filter mask
 *
 * Returns the raw unsigned 16-bit filter mask that Defold assigned to the
 * object's Bullet broadphase proxy. Use this value as `mask_bits` in a
 * `bullet3d.world` query filter. Bullet applies reciprocal filtering: the
 * query's `category_bits` must be included in this mask, and the query's
 * `mask_bits` must include the object's filter group.
 *
 * @name bullet3d.collision_object.get_collision_filter_mask
 * @param object [type:btCollisionObject] collision object in a Bullet world
 * @return mask [type:number] raw unsigned 16-bit collision filter mask
 */

/*# Test a collision flag
 * @name bullet3d.collision_object.has_collision_flag
 * @param object [type:btCollisionObject] collision object
 * @param flag [type:number] collision flag or mask
 * @return set [type:boolean] `true` when all requested flag bits are set
 */

/*# Get the Bullet collision object type
 * @name bullet3d.collision_object.get_internal_type
 * @param object [type:btCollisionObject] collision object
 * @return type [type:number] one of the `CO_*` constants
 */

/*# Test whether the object is static
 * @name bullet3d.collision_object.is_static
 * @param object [type:btCollisionObject] collision object
 * @return static [type:boolean] static state
 */

/*# Test whether the object is kinematic
 * @name bullet3d.collision_object.is_kinematic
 * @param object [type:btCollisionObject] collision object
 * @return kinematic [type:boolean] kinematic state
 */

/*# Test whether the object is static or kinematic
 * @name bullet3d.collision_object.is_static_or_kinematic
 * @param object [type:btCollisionObject] collision object
 * @return result [type:boolean] static or kinematic state
 */

/*# Test whether the object responds to contacts
 * @name bullet3d.collision_object.has_contact_response
 * @param object [type:btCollisionObject] collision object
 * @return result [type:boolean] contact response state
 */

/*# Test whether the object is a rigid body
 * @name bullet3d.collision_object.is_rigid_body
 * @param object [type:btCollisionObject] collision object
 * @return result [type:boolean] rigid body state
 */

/*# Test whether the object is a ghost trigger
 * @name bullet3d.collision_object.is_ghost_object
 * @param object [type:btCollisionObject] collision object
 * @return result [type:boolean] ghost object state
 */
