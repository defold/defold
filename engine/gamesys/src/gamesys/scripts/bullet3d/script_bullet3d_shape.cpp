// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <assert.h>
#include <math.h>
#include <stdint.h>

#include <dlib/array.h>
#include <gameobject/gameobject.h>
#include <gameobject/script.h>
#include <script/script.h>

#include "components/comp_collision_object.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "gamesys_private.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
#define BULLET3D_TYPE_NAME_SHAPE "bullet3d_shape"

    static uint32_t TYPE_HASH_SHAPE = 0;

    struct Bullet3DLuaShape
    {
        uint64_t m_OwnerId;
        uint32_t m_ShapeIndex;
    };

    struct Bullet3DMutableShape
    {
        btCollisionObject*            m_Owner;
        btCollisionShape*             m_Shape;
        dmGameObject::HComponent      m_Component;
        dmGameObject::HComponentWorld m_ComponentWorld;
        uint32_t                      m_ShapeIndex;
    };

    static Bullet3DLuaShape* CheckShapeInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaShape*)dmScript::CheckUserType(L, index, TYPE_HASH_SHAPE, "Expected user type " BULLET3D_TYPE_NAME_SHAPE);
    }

    static Bullet3DLuaShape* ToShapeInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaShape*)dmScript::ToUserType(L, index, TYPE_HASH_SHAPE);
    }

    static uint32_t GetShapeCount(const btCollisionObject* owner)
    {
        const btCollisionShape* root_shape = owner->getCollisionShape();
        return root_shape->isCompound() ? (uint32_t)((const btCompoundShape*)root_shape)->getNumChildShapes() : 1;
    }

    static btCollisionShape* ResolveShape(lua_State* L, const Bullet3DLuaShape* lua_shape, bool report_error, btCollisionObject** out_owner)
    {
        btCollisionObject* owner = ToBullet3DCollisionObjectById(L, lua_shape->m_OwnerId);
        if (!owner)
        {
            if (report_error)
            {
                luaL_error(L, "Invalid bullet3d shape handle: its collision object no longer exists.");
            }
            return 0;
        }

        btCollisionShape* root_shape = owner->getCollisionShape();
        btCollisionShape* shape = 0;
        if (root_shape->isCompound())
        {
            btCompoundShape* compound = (btCompoundShape*)root_shape;
            if (lua_shape->m_ShapeIndex < (uint32_t)compound->getNumChildShapes())
            {
                shape = compound->getChildShape((int)lua_shape->m_ShapeIndex);
            }
        }
        else if (lua_shape->m_ShapeIndex == 0)
        {
            shape = root_shape;
        }

        if (!shape && report_error)
        {
            luaL_error(L, "Invalid bullet3d shape handle: shape slot %u no longer exists.", lua_shape->m_ShapeIndex + 1);
        }
        if (out_owner)
        {
            *out_owner = owner;
        }
        return shape;
    }

    static btCollisionShape* CheckShape(lua_State* L, int index, Bullet3DLuaShape** out_lua_shape, btCollisionObject** out_owner)
    {
        Bullet3DLuaShape* lua_shape = CheckShapeInternal(L, index);
        btCollisionShape* shape = ResolveShape(L, lua_shape, true, out_owner);
        if (out_lua_shape)
        {
            *out_lua_shape = lua_shape;
        }
        return shape;
    }

    static void PushShape(lua_State* L, uint64_t owner_id, uint32_t shape_index)
    {
        Bullet3DLuaShape* lua_shape = (Bullet3DLuaShape*)lua_newuserdata(L, sizeof(Bullet3DLuaShape));
        lua_shape->m_OwnerId = owner_id;
        lua_shape->m_ShapeIndex = shape_index;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_SHAPE);
        lua_setmetatable(L, -2);
    }

    static uint32_t CheckShapeIndex(lua_State* L, int index, uint32_t shape_count)
    {
        lua_Number value = luaL_checknumber(L, index);
        if (!isfinite((double)value) || value < 1.0 || value > shape_count || floor((double)value) != value)
        {
            luaL_error(L, "shape_index must be an integer between 1 and %u.", shape_count);
            return 0;
        }
        return (uint32_t)value - 1;
    }

    static bool IsFiniteScalar(btScalar value)
    {
        return isfinite((double)value) != 0;
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static btScalar CheckPositiveLength(lua_State* L, int index, const char* name)
    {
        btScalar value = CheckBullet3DScalar(L, index, GetBullet3DPhysicsScale(), name);
        if (!(value > 0.0f))
        {
            luaL_error(L, "%s must be finite and greater than zero.", name);
        }
        return value;
    }

    static btVector3 CheckPositiveVector3(lua_State* L, int index, float scale, const char* name)
    {
        btVector3 value = CheckBullet3DVector3(L, index, scale, name);
        if (!(value.getX() > 0.0f) || !(value.getY() > 0.0f) || !(value.getZ() > 0.0f))
        {
            luaL_error(L, "%s components must be finite and greater than zero.", name);
        }
        return value;
    }

    void CheckBullet3DShapeDef(lua_State* L, int index, Bullet3DShapeDef* shape_def)
    {
        index = AbsIndex(L, index);
        luaL_checktype(L, index, LUA_TTABLE);

        shape_def->m_Dimensions = btVector3(0.0f, 0.0f, 0.0f);
        shape_def->m_Diameter = 0.0f;
        shape_def->m_Height = 0.0f;
        shape_def->m_VerticesIndex = 0;
        shape_def->m_VertexCount = 0;

        lua_getfield(L, index, "type");
        shape_def->m_Type = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        if (shape_def->m_Type == dmPhysicsDDF::CollisionShape::TYPE_SPHERE)
        {
            lua_getfield(L, index, "diameter");
            shape_def->m_Diameter = CheckPositiveLength(L, -1, "diameter");
            lua_pop(L, 1);
        }
        else if (shape_def->m_Type == dmPhysicsDDF::CollisionShape::TYPE_BOX)
        {
            lua_getfield(L, index, "dimensions");
            shape_def->m_Dimensions = CheckPositiveVector3(L, -1, GetBullet3DPhysicsScale(), "dimensions");
            lua_pop(L, 1);
        }
        else if (shape_def->m_Type == dmPhysicsDDF::CollisionShape::TYPE_CAPSULE)
        {
            lua_getfield(L, index, "diameter");
            shape_def->m_Diameter = CheckPositiveLength(L, -1, "diameter");
            lua_pop(L, 1);
            lua_getfield(L, index, "height");
            shape_def->m_Height = CheckPositiveLength(L, -1, "height");
            lua_pop(L, 1);
        }
        else if (shape_def->m_Type == dmPhysicsDDF::CollisionShape::TYPE_HULL)
        {
            lua_getfield(L, index, "vertices");
            luaL_checktype(L, -1, LUA_TTABLE);
            shape_def->m_VerticesIndex = lua_gettop(L);
            shape_def->m_VertexCount = (int)lua_objlen(L, -1);
            if (shape_def->m_VertexCount < 4)
            {
                luaL_error(L, "vertices must contain at least four points.");
            }
            for (int i = 1; i <= shape_def->m_VertexCount; ++i)
            {
                lua_rawgeti(L, shape_def->m_VerticesIndex, i);
                CheckBullet3DVector3(L, -1, GetBullet3DPhysicsScale(), "vertices");
                lua_pop(L, 1);
            }
        }
        else
        {
            luaL_error(L, "Unsupported shape type %d.", shape_def->m_Type);
        }
    }

    btConvexShape* CreateBullet3DConvexShape(lua_State* L, const Bullet3DShapeDef& shape_def)
    {
        switch (shape_def.m_Type)
        {
            case dmPhysicsDDF::CollisionShape::TYPE_SPHERE:
                return new btSphereShape(shape_def.m_Diameter * 0.5f);
            case dmPhysicsDDF::CollisionShape::TYPE_BOX:
                return new btBoxShape(shape_def.m_Dimensions * 0.5f);
            case dmPhysicsDDF::CollisionShape::TYPE_CAPSULE:
                return new btCapsuleShape(shape_def.m_Diameter * 0.5f, shape_def.m_Height);
            case dmPhysicsDDF::CollisionShape::TYPE_HULL:
            {
                dmArray<btVector3> vertices;
                vertices.SetCapacity(shape_def.m_VertexCount);
                for (int i = 1; i <= shape_def.m_VertexCount; ++i)
                {
                    lua_rawgeti(L, shape_def.m_VerticesIndex, i);
                    dmVMath::Vector3* vertex = dmScript::ToVector3(L, -1);
                    assert(vertex != 0);
                    btVector3 point(vertex->getX(), vertex->getY(), vertex->getZ());
                    vertices.Push(point * GetBullet3DPhysicsScale());
                    lua_pop(L, 1);
                }
                return new btConvexHullShape((const btScalar*)vertices.Begin(),
                                             shape_def.m_VertexCount,
                                             sizeof(btVector3));
            }
            default:
                return 0;
        }
    }

    static btVector3 CheckPreservableLocalScaling(lua_State* L, const btCollisionShape* shape)
    {
        const btVector3& scaling = shape->getLocalScaling();
        btScalar         inverse_x = 1.0f / scaling.getX();
        btScalar         inverse_y = 1.0f / scaling.getY();
        btScalar         inverse_z = 1.0f / scaling.getZ();
        if (!IsFiniteScalar(scaling.getX()) || !IsFiniteScalar(scaling.getY()) || !IsFiniteScalar(scaling.getZ()) ||
            !(scaling.getX() > 0.0f) || !(scaling.getY() > 0.0f) || !(scaling.getZ() > 0.0f) ||
            !IsFiniteScalar(inverse_x) || !IsFiniteScalar(inverse_y) || !IsFiniteScalar(inverse_z) ||
            !(inverse_x > 0.0f) || !(inverse_y > 0.0f) || !(inverse_z > 0.0f))
        {
            luaL_error(L, "The shape's existing local scaling cannot be preserved because it has no finite native inverse.");
        }
        return scaling;
    }

    static int NormalizeShapeType(lua_State* L, const btCollisionShape* shape)
    {
        switch (shape->getShapeType())
        {
            case SPHERE_SHAPE_PROXYTYPE:
                return dmPhysicsDDF::CollisionShape::TYPE_SPHERE;
            case BOX_SHAPE_PROXYTYPE:
                return dmPhysicsDDF::CollisionShape::TYPE_BOX;
            case CAPSULE_SHAPE_PROXYTYPE:
                return dmPhysicsDDF::CollisionShape::TYPE_CAPSULE;
            case CONVEX_HULL_SHAPE_PROXYTYPE:
                return dmPhysicsDDF::CollisionShape::TYPE_HULL;
            default:
                luaL_error(L, "Unsupported Defold Bullet3D shape type %d.", shape->getShapeType());
                return -1;
        }
    }

    static void CheckMutableShape(lua_State* L, Bullet3DLuaShape* lua_shape, btCollisionObject* owner, Bullet3DMutableShape* mutable_shape)
    {
        dmGameObject::HCollection collection = GetBullet3DCollisionObjectCollectionById(L, lua_shape->m_OwnerId);
        if (!collection || !owner->getUserPointer())
        {
            luaL_error(L, "Cannot mutate a shape owned by an unmanaged bullet3d collision object.");
        }

        uint32_t                      component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        dmGameObject::HComponentWorld component_world = dmGameObject::GetWorld(collection, component_type_index);
        dmGameObject::HComponent      component = (dmGameObject::HComponent)owner->getUserPointer();
        if (!component_world || CompCollisionObjectGetBullet3DCollisionObject(component) != owner)
        {
            luaL_error(L, "Cannot mutate a shape whose Defold collision component no longer exists.");
        }
        if (CompCollisionObjectIsBullet3DWorldLocked(component_world))
        {
            luaL_error(L, "Cannot mutate a bullet3d shape while its world is stepping.");
        }
        btCollisionShape* shape = ResolveShape(L, lua_shape, true, 0);

        mutable_shape->m_Owner = owner;
        mutable_shape->m_Shape = shape;
        mutable_shape->m_Component = component;
        mutable_shape->m_ComponentWorld = component_world;
        mutable_shape->m_ShapeIndex = lua_shape->m_ShapeIndex;
    }

    static void RefreshMutableShape(const Bullet3DMutableShape& mutable_shape)
    {
        CompCollisionObjectRefreshBullet3DShape(mutable_shape.m_ComponentWorld, mutable_shape.m_Component);
    }

    static bool ReplaceMutableShape(lua_State* L, const Bullet3DMutableShape& mutable_shape, btConvexShape* replacement)
    {
        if (CompCollisionObjectReplaceBullet3DShape(mutable_shape.m_ComponentWorld, mutable_shape.m_Component, mutable_shape.m_ShapeIndex, replacement))
        {
            return true;
        }
        delete replacement;
        luaL_error(L, "Could not replace bullet3d shape slot %u.", mutable_shape.m_ShapeIndex + 1);
        return false;
    }

    static int CollisionObject_GetShapeCount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, GetShapeCount(CheckBullet3DCollisionObject(L, 1)));
        return 1;
    }

    static int CollisionObject_GetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btCollisionObject* owner = CheckBullet3DCollisionObject(L, 1);
        uint64_t           owner_id = CheckBullet3DCollisionObjectId(L, 1);
        PushShape(L, owner_id, CheckShapeIndex(L, 2, GetShapeCount(owner)));
        return 1;
    }

    static int CollisionObject_GetShapes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btCollisionObject* owner = CheckBullet3DCollisionObject(L, 1);
        uint64_t           owner_id = CheckBullet3DCollisionObjectId(L, 1);
        uint32_t           shape_count = GetShapeCount(owner);
        lua_createtable(L, (int)shape_count, 0);
        for (uint32_t i = 0; i < shape_count; ++i)
        {
            PushShape(L, owner_id, i);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }

    static int Shape_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DLuaShape* lua_shape = ToShapeInternal(L, 1);
        lua_pushboolean(L, lua_shape && ResolveShape(L, lua_shape, false, 0));
        return 1;
    }

    static int Shape_GetCollisionObject(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DLuaShape* lua_shape = 0;
        CheckShape(L, 1, &lua_shape, 0);
        PushBullet3DCollisionObjectById(L, lua_shape->m_OwnerId);
        return 1;
    }

    static int Shape_GetIndex(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DLuaShape* lua_shape = 0;
        CheckShape(L, 1, &lua_shape, 0);
        lua_pushinteger(L, lua_shape->m_ShapeIndex + 1);
        return 1;
    }

    static int Shape_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, NormalizeShapeType(L, CheckShape(L, 1, 0, 0)));
        return 1;
    }

    static void PushHullVertices(lua_State* L, const btConvexHullShape* hull)
    {
        lua_createtable(L, hull->getNumPoints(), 0);
        for (int i = 0; i < hull->getNumPoints(); ++i)
        {
            PushBullet3DVector3(L, hull->getScaledPoint(i), GetBullet3DInvPhysicsScale());
            lua_rawseti(L, -2, i + 1);
        }
    }

    static int Shape_GetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btCollisionShape* shape = CheckShape(L, 1, 0, 0);
        float             inv_scale = GetBullet3DInvPhysicsScale();

        lua_newtable(L);
        lua_pushinteger(L, NormalizeShapeType(L, shape));
        lua_setfield(L, -2, "type");

        switch (shape->getShapeType())
        {
            case SPHERE_SHAPE_PROXYTYPE:
                lua_pushnumber(L, ((btSphereShape*)shape)->getRadius() * 2.0f * inv_scale);
                lua_setfield(L, -2, "diameter");
                break;
            case BOX_SHAPE_PROXYTYPE:
                PushBullet3DVector3(L, ((btBoxShape*)shape)->getHalfExtentsWithMargin() * 2.0f, inv_scale);
                lua_setfield(L, -2, "dimensions");
                break;
            case CAPSULE_SHAPE_PROXYTYPE:
            {
                btCapsuleShape* capsule = (btCapsuleShape*)shape;
                lua_pushnumber(L, capsule->getRadius() * 2.0f * inv_scale);
                lua_setfield(L, -2, "diameter");
                lua_pushnumber(L, capsule->getHalfHeight() * 2.0f * inv_scale);
                lua_setfield(L, -2, "height");
                break;
            }
            case CONVEX_HULL_SHAPE_PROXYTYPE:
                PushHullVertices(L, (btConvexHullShape*)shape);
                lua_setfield(L, -2, "vertices");
                break;
            default:
                return luaL_error(L, "Unsupported Defold Bullet3D shape type %d.", shape->getShapeType());
        }
        return 1;
    }

    static int Shape_SetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int                 top = lua_gettop(L);
        Bullet3DLuaShape*  lua_shape = 0;
        btCollisionObject* owner = 0;
        btCollisionShape*  shape = CheckShape(L, 1, &lua_shape, &owner);
        Bullet3DShapeDef    shape_def;
        CheckBullet3DShapeDef(L, 2, &shape_def);

        int shape_type = NormalizeShapeType(L, shape);
        if (shape_def.m_Type != shape_type)
        {
            return luaL_error(L, "Shape type must match the existing shape type.");
        }

        Bullet3DMutableShape mutable_shape;
        CheckMutableShape(L, lua_shape, owner, &mutable_shape);
        btVector3      local_scaling = CheckPreservableLocalScaling(L, shape);
        btConvexShape* replacement = 0;

        switch (shape->getShapeType())
        {
            case SPHERE_SHAPE_PROXYTYPE:
            {
                btScalar radius = shape_def.m_Diameter * 0.5f;
                btScalar unscaled_radius = radius / local_scaling.getX();
                if (!IsFiniteScalar(unscaled_radius) || !(unscaled_radius > 0.0f))
                {
                    return luaL_error(L, "diameter cannot be represented with the shape's existing local scaling.");
                }
                replacement = new btSphereShape(unscaled_radius);
                break;
            }
            case BOX_SHAPE_PROXYTYPE:
            {
                btVector3 unscaled_half_extents = shape_def.m_Dimensions * 0.5f / local_scaling;
                if (!IsFiniteScalar(unscaled_half_extents.getX()) || !IsFiniteScalar(unscaled_half_extents.getY()) || !IsFiniteScalar(unscaled_half_extents.getZ()) ||
                    !(unscaled_half_extents.getX() > 0.0f) || !(unscaled_half_extents.getY() > 0.0f) || !(unscaled_half_extents.getZ() > 0.0f))
                {
                    return luaL_error(L, "dimensions cannot be represented with the shape's existing local scaling.");
                }
                replacement = new btBoxShape(unscaled_half_extents);
                replacement->setMargin(shape->getMargin());
                break;
            }
            case CAPSULE_SHAPE_PROXYTYPE:
            {
                btScalar unscaled_radius = shape_def.m_Diameter * 0.5f / local_scaling.getX();
                btScalar unscaled_height = shape_def.m_Height / local_scaling.getY();
                if (!IsFiniteScalar(unscaled_radius) || !IsFiniteScalar(unscaled_height) || !(unscaled_radius > 0.0f) || !(unscaled_height > 0.0f))
                {
                    return luaL_error(L, "dimensions cannot be represented with the shape's existing local scaling.");
                }
                replacement = new btCapsuleShape(unscaled_radius, unscaled_height);
                break;
            }
            case CONVEX_HULL_SHAPE_PROXYTYPE:
            {
                for (int i = 1; i <= shape_def.m_VertexCount; ++i)
                {
                    lua_rawgeti(L, shape_def.m_VerticesIndex, i);
                    dmVMath::Vector3* vertex = dmScript::ToVector3(L, -1);
                    assert(vertex != 0);
                    btVector3 scaled_vertex(vertex->getX(), vertex->getY(), vertex->getZ());
                    scaled_vertex *= GetBullet3DPhysicsScale();
                    btVector3 unscaled_vertex = scaled_vertex / local_scaling;
                    if (!IsFiniteScalar(unscaled_vertex.getX()) || !IsFiniteScalar(unscaled_vertex.getY()) || !IsFiniteScalar(unscaled_vertex.getZ()) ||
                        (scaled_vertex.getX() != 0.0f && unscaled_vertex.getX() == 0.0f) ||
                        (scaled_vertex.getY() != 0.0f && unscaled_vertex.getY() == 0.0f) ||
                        (scaled_vertex.getZ() != 0.0f && unscaled_vertex.getZ() == 0.0f))
                    {
                        lua_pop(L, 1);
                        return luaL_error(L, "vertices cannot be represented with the shape's existing local scaling.");
                    }
                    lua_pop(L, 1);
                }

                dmArray<btVector3> vertices;
                vertices.SetCapacity(shape_def.m_VertexCount);
                for (int i = 1; i <= shape_def.m_VertexCount; ++i)
                {
                    lua_rawgeti(L, shape_def.m_VerticesIndex, i);
                    dmVMath::Vector3* vertex = dmScript::ToVector3(L, -1);
                    assert(vertex != 0);
                    btVector3 scaled_vertex(vertex->getX(), vertex->getY(), vertex->getZ());
                    scaled_vertex *= GetBullet3DPhysicsScale();
                    vertices.Push(scaled_vertex / local_scaling);
                    lua_pop(L, 1);
                }
                btConvexHullShape* hull = new btConvexHullShape((const btScalar*)vertices.Begin(),
                                                                shape_def.m_VertexCount,
                                                                sizeof(btVector3));
                hull->setMargin(shape->getMargin());
                replacement = hull;
                break;
            }
            default:
                return luaL_error(L, "Unsupported Defold Bullet3D shape type %d.", shape->getShapeType());
        }

        replacement->setLocalScaling(local_scaling);
        ReplaceMutableShape(L, mutable_shape, replacement);
        lua_settop(L, top);
        return 0;
    }

    static int Shape_GetLocalTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        Bullet3DLuaShape*  lua_shape = 0;
        btCollisionObject* owner = 0;
        CheckShape(L, 1, &lua_shape, &owner);
        btTransform       transform = btTransform::getIdentity();
        btCollisionShape* root_shape = owner->getCollisionShape();
        if (root_shape->isCompound())
        {
            transform = ((btCompoundShape*)root_shape)->getChildTransform((int)lua_shape->m_ShapeIndex);
        }
        PushBullet3DVector3(L, transform.getOrigin(), GetBullet3DInvPhysicsScale());
        PushBullet3DQuat(L, transform.getRotation());
        return 2;
    }

    static int Shape_SetLocalTransform(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaShape*  lua_shape = 0;
        btCollisionObject* owner = 0;
        CheckShape(L, 1, &lua_shape, &owner);
        Bullet3DMutableShape mutable_shape;
        CheckMutableShape(L, lua_shape, owner, &mutable_shape);

        btCollisionShape* root_shape = owner->getCollisionShape();
        if (!root_shape->isCompound())
        {
            return luaL_error(L, "A non-compound collision object's only shape has no local child transform.");
        }
        btVector3    position = CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "position");
        btQuaternion rotation = CheckBullet3DQuat(L, 3, "rotation");
        ((btCompoundShape*)root_shape)->updateChildTransform((int)lua_shape->m_ShapeIndex, btTransform(rotation, position));
        RefreshMutableShape(mutable_shape);
        return 0;
    }

    static int Shape_tostring(lua_State* L)
    {
        Bullet3DLuaShape* lua_shape = 0;
        btCollisionShape* shape = CheckShape(L, 1, &lua_shape, 0);
        lua_pushfstring(L, "Bullet3D.%s = %p (shape %d)", BULLET3D_TYPE_NAME_SHAPE, shape, (int)lua_shape->m_ShapeIndex + 1);
        return 1;
    }

    static int Shape_eq(lua_State* L)
    {
        Bullet3DLuaShape* a = ToShapeInternal(L, 1);
        Bullet3DLuaShape* b = ToShapeInternal(L, 2);
        lua_pushboolean(L, a && b && a->m_OwnerId == b->m_OwnerId && a->m_ShapeIndex == b->m_ShapeIndex);
        return 1;
    }

    static const luaL_reg Shape_methods[] = {
        { 0, 0 }
    };

    static const luaL_reg Shape_meta[] = {
        { "__tostring", Shape_tostring },
        { "__eq", Shape_eq },
        { 0, 0 }
    };

    static const luaL_reg CollisionObjectShape_functions[] = {
        { "get_shape_count", CollisionObject_GetShapeCount },
        { "get_shape", CollisionObject_GetShape },
        { "get_shapes", CollisionObject_GetShapes },
        { 0, 0 }
    };

    static const luaL_reg Shape_functions[] = {
        { "is_valid", Shape_IsValid },
        { "get_collision_object", Shape_GetCollisionObject },
        { "get_index", Shape_GetIndex },
        { "get_type", Shape_GetType },
        { "get_shape", Shape_GetShape },
        { "set_shape", Shape_SetShape },
        { "get_local_transform", Shape_GetLocalTransform },
        { "set_local_transform", Shape_SetLocalTransform },
        { 0, 0 }
    };

    static void SetIntegerConstant(lua_State* L, const char* name, int value)
    {
        lua_pushinteger(L, value);
        lua_setfield(L, -2, name);
    }

    void ScriptBullet3DInitializeShape(lua_State* L)
    {
        TYPE_HASH_SHAPE = dmScript::RegisterUserType(L, BULLET3D_TYPE_NAME_SHAPE, Shape_methods, Shape_meta);

        lua_getfield(L, -1, "collision_object");
        luaL_register(L, 0, CollisionObjectShape_functions);
        lua_pop(L, 1);

        lua_newtable(L);
        luaL_register(L, 0, Shape_functions);
        SetIntegerConstant(L, "SHAPE_TYPE_SPHERE", dmPhysicsDDF::CollisionShape::TYPE_SPHERE);
        SetIntegerConstant(L, "SHAPE_TYPE_BOX", dmPhysicsDDF::CollisionShape::TYPE_BOX);
        SetIntegerConstant(L, "SHAPE_TYPE_CAPSULE", dmPhysicsDDF::CollisionShape::TYPE_CAPSULE);
        SetIntegerConstant(L, "SHAPE_TYPE_HULL", dmPhysicsDDF::CollisionShape::TYPE_HULL);
        lua_setfield(L, -2, "shape");
    }

    void ScriptBullet3DFinalizeShape()
    {
        TYPE_HASH_SHAPE = 0;
    }
} // namespace dmGameSystem

/*# Bullet collision shape API
 *
 * Borrowed shape handles identify a one-based child slot on a Defold-owned
 * collision object. They remain attached to that logical slot when its native
 * shape is replaced and become invalid with their owning collision object.
 * Shape mutation is copy-on-write, so instances sharing a collision resource
 * are not modified together. Lengths use Defold world units.
 *
 * @document
 * @name bullet3d.shape
 * @namespace bullet3d.shape
 * @language Lua
 */

/*# Bullet collision shape
 *
 * Shape handles identify logical child slots and resolve the current native
 * shape on every call.
 *
 * @typedef
 * @name btCollisionShape
 * @param value [type:userdata]
 */

/*# Sphere shape type
 *
 * Value `0`. Shape data contains a positive numeric `diameter` in Defold units.
 *
 * @name bullet3d.shape.SHAPE_TYPE_SPHERE
 * @constant
 */

/*# Box shape type
 *
 * Value `1`. Shape data contains positive vector3 `dimensions` in Defold units.
 *
 * @name bullet3d.shape.SHAPE_TYPE_BOX
 * @constant
 */

/*# Capsule shape type
 *
 * Value `2`. Shape data contains a positive numeric `diameter` and positive
 * numeric cylindrical-section `height` in Defold units.
 *
 * @name bullet3d.shape.SHAPE_TYPE_CAPSULE
 * @constant
 */

/*# Convex hull shape type
 *
 * Value `3`. Shape data contains a `vertices` array with at least four finite
 * vector3 values in Defold units.
 *
 * @name bullet3d.shape.SHAPE_TYPE_HULL
 * @constant
 */

/*# Get the number of shapes attached to a collision object.
 * @name bullet3d.collision_object.get_shape_count
 * @param object [type:btCollisionObject] collision object
 * @return count [type:number] shape count
 */

/*# Get one attached shape by one-based index.
 * @name bullet3d.collision_object.get_shape
 * @param object [type:btCollisionObject] collision object
 * @param shape_index [type:number] one-based shape index
 * @return shape [type:btCollisionShape] borrowed logical shape handle
 */

/*# Get all attached shapes.
 * @name bullet3d.collision_object.get_shapes
 * @param object [type:btCollisionObject] collision object
 * @return shapes [type:table] array of borrowed shape handles
 * @examples
 *
 * Enumerate the logical shapes attached to a collision object:
 *
 * ```lua
 * function init(self)
 *     local object = bullet3d.get_collision_object("#collisionobject")
 *     for _, shape in ipairs(bullet3d.collision_object.get_shapes(object)) do
 *         local index = bullet3d.shape.get_index(shape)
 *         local data = bullet3d.shape.get_shape(shape)
 *         print("shape", index, "type", data.type)
 *     end
 * end
 * ```
 */

/*# Test whether a shape handle and its owner still exist.
 * @name bullet3d.shape.is_valid
 * @param shape [type:btCollisionShape] shape handle
 * @return valid [type:boolean] validity
 */

/*# Get the owning collision object.
 * @name bullet3d.shape.get_collision_object
 * @param shape [type:btCollisionShape] shape handle
 * @return object [type:btCollisionObject] owning collision object
 */

/*# Get the one-based child index.
 * @name bullet3d.shape.get_index
 * @param shape [type:btCollisionShape] shape handle
 * @return shape_index [type:number] one-based shape index
 */

/*# Get the normalized Defold shape type.
 * @name bullet3d.shape.get_type
 * @param shape [type:btCollisionShape] shape handle
 * @return type [type:number] one of `bullet3d.shape.SHAPE_TYPE_*`
 */

/*# Get shape geometry data.
 *
 * The returned table always contains `type`, one of `bullet3d.shape.SHAPE_TYPE_*`.
 * A sphere also contains numeric `diameter`; a box contains vector3
 * `dimensions`; a capsule contains numeric `diameter` and cylindrical-section
 * `height`; and a hull contains a `vertices` array of vector3 values. The table
 * uses Defold units and can be passed to a `bullet3d.world` shape query after
 * adding the desired `position` and optional `rotation` fields.
 *
 * @name bullet3d.shape.get_shape
 * @param shape [type:btCollisionShape] shape handle
 * @return data [type:table] typed shape geometry in Defold units
 */

/*# Set shape geometry data.
 *
 * The table uses the same format as `get_shape`. Its `type` must match the
 * existing shape because changing native shape type is not supported. Primitive
 * dimensions must be finite and greater than zero. Hulls require at least four
 * finite vertices.
 *
 * @name bullet3d.shape.set_shape
 * @param shape [type:btCollisionShape] shape handle
 * @param data [type:table] typed shape geometry in Defold units
 * @examples
 *
 * Increase the dimensions of the first box shape by 50 percent for this instance:
 *
 * ```lua
 * function init(self)
 *     local object = bullet3d.get_collision_object("#collisionobject")
 *     local shape = bullet3d.collision_object.get_shape(object, 1)
 *     local data = bullet3d.shape.get_shape(shape)
 *
 *     if data.type == bullet3d.shape.SHAPE_TYPE_BOX then
 *         data.dimensions = data.dimensions * 1.5
 *         bullet3d.shape.set_shape(shape, data)
 *     end
 * end
 * ```
 */

/*# Get a compound child's local transform.
 * @name bullet3d.shape.get_local_transform
 * @param shape [type:btCollisionShape] shape handle
 * @return position [type:vector3] local position
 * @return rotation [type:quaternion] local rotation
 */

/*# Set a compound child's local transform.
 * @name bullet3d.shape.set_local_transform
 * @param shape [type:btCollisionShape] shape handle
 * @param position [type:vector3] finite local position
 * @param rotation [type:quaternion] finite non-zero local rotation
 */
