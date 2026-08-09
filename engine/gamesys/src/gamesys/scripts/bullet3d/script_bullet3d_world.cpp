// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dmsdk/dlib/hashtable.h>
#include <gameobject/gameobject.h>
#include <script/script.h>

#include "components/comp_collision_object.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

//////////////////////////////////////////////////////////////////////////////
// btDiscreteDynamicsWorld
namespace dmGameSystem
{
#define BULLET3D_TYPE_NAME_WORLD "bullet3d_world"

    static uint32_t TYPE_HASH_WORLD = 0;

    struct Bullet3DLuaWorld
    {
        uint64_t m_Id;
        void*    m_ComponentWorld;
    };

    // Bullet worlds are raw pointers. Assign each live pointer a monotonically
    // increasing identity so stale Lua userdata cannot revive when a native
    // address is reused.
    static uint64_t                                g_NextBullet3DWorldId = 0;
    static dmHashTable64<btDiscreteDynamicsWorld*> g_Bullet3DWorlds;
    static dmHashTable64<uint64_t>                 g_Bullet3DWorldToId;

    struct Bullet3DQueryFilter
    {
        dmArray<btCollisionObject*> m_IgnoredObjects;
        uint16_t                    m_CategoryBits;
        uint16_t                    m_MaskBits;
        bool                        m_IncludeTriggers;
        bool                        m_ReportInitialOverlaps;
    };

    struct Bullet3DQueryFilterInput
    {
        uint16_t m_CategoryBits;
        uint16_t m_MaskBits;
        int      m_IgnoreIndex;
        int      m_IgnoreCount;
        bool     m_IgnoreIsArray;
        bool     m_IncludeTriggers;
        bool     m_ReportInitialOverlaps;
    };

    struct Bullet3DQueryObjectResult
    {
        btCollisionObject* m_Object;
    };

    struct Bullet3DCastResult
    {
        btCollisionObject* m_Object;
        btVector3          m_Point;
        btVector3          m_Normal;
        btScalar           m_Fraction;
        int                m_ShapeIndex;
        bool               m_InitialOverlap;
        bool               m_Inside;
    };

    struct Bullet3DContactResult
    {
        btCollisionObject* m_ObjectA;
        btCollisionObject* m_ObjectB;
        btVector3          m_PositionA;
        btVector3          m_PositionB;
        btVector3          m_NormalOnB;
        btScalar           m_Distance;
    };

    struct Bullet3DQueryShape
    {
        btConvexShape* m_Shape;
        btTransform    m_From;
        btTransform    m_To;
    };

    struct Bullet3DQueryShapeInput
    {
        btVector3    m_Position;
        btVector3    m_Dimensions;
        btQuaternion m_Rotation;
        btQuaternion m_TargetRotation;
        btScalar     m_Diameter;
        btScalar     m_Height;
        int          m_Type;
        int          m_VerticesIndex;
        int          m_VertexCount;
    };

    struct Bullet3DAsyncQueryFilter
    {
        dmArray<uint64_t> m_IgnoredObjectIds;
        uint16_t          m_CategoryBits;
        uint16_t          m_MaskBits;
        bool              m_IncludeTriggers;
        bool              m_ReportInitialOverlaps;
    };

    enum Bullet3DAsyncCastType
    {
        BULLET3D_ASYNC_CAST_RAY,
        BULLET3D_ASYNC_CAST_SHAPE,
    };

    struct Bullet3DAsyncCastRequest
    {
        btDiscreteDynamicsWorld*    m_World;
        uint64_t                    m_WorldId;
        dmScript::LuaCallbackInfo*  m_Callback;
        Bullet3DAsyncQueryFilter    m_Filter;
        Bullet3DQueryShape          m_QueryShape;
        btVector3                   m_RayOrigin;
        btVector3                   m_RayTarget;
        int                         m_MaxResults;
        Bullet3DAsyncCastType       m_Type;
    };

    static dmArray<Bullet3DAsyncCastRequest*> g_Bullet3DAsyncCastRequests;

    static void DestroyAsyncCastRequest(Bullet3DAsyncCastRequest* request)
    {
        if (request->m_QueryShape.m_Shape)
        {
            delete request->m_QueryShape.m_Shape;
        }
        if (request->m_Callback)
        {
            dmScript::DestroyCallback(request->m_Callback);
        }
        delete request;
    }

    static void CancelAsyncCastRequests(btDiscreteDynamicsWorld* world)
    {
        for (uint32_t i = g_Bullet3DAsyncCastRequests.Size(); i > 0; --i)
        {
            uint32_t index = i - 1;
            Bullet3DAsyncCastRequest* request = g_Bullet3DAsyncCastRequests[index];
            if (!world || request->m_World == world)
            {
                g_Bullet3DAsyncCastRequests.EraseSwap(index);
                DestroyAsyncCastRequest(request);
            }
        }
    }

    template <typename T>
    static void ArrayPush(dmArray<T>* values, const T& value)
    {
        if (values->Full())
        {
            values->SetCapacity(values->Capacity() ? values->Capacity() * 2 : 16);
        }
        values->Push(value);
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static bool GetCollisionObjectOwner(btCollisionObject* object, dmGameObject::HCollection* collection, dmhash_t* instance_id)
    {
        if (!object || !object->getUserPointer())
        {
            return false;
        }

        dmGameObject::HInstance instance = CompCollisionObjectGetInstance(object->getUserPointer());
        if (!instance)
        {
            return false;
        }

        dmGameObject::HCollection object_collection = dmGameObject::GetCollection(instance);
        if (!object_collection)
        {
            return false;
        }

        if (collection)
        {
            *collection = object_collection;
        }
        if (instance_id)
        {
            *instance_id = dmGameObject::GetIdentifier(instance);
        }
        return true;
    }

    static bool IsIgnoredObject(const Bullet3DQueryFilter* filter, const btCollisionObject* object)
    {
        for (uint32_t i = 0; i < filter->m_IgnoredObjects.Size(); ++i)
        {
            if (filter->m_IgnoredObjects[i] == object)
            {
                return true;
            }
        }
        return false;
    }

    static bool PassesQueryFilter(const Bullet3DQueryFilter* filter, const btBroadphaseProxy* proxy)
    {
        if (!proxy || !proxy->m_clientObject)
        {
            return false;
        }

        btCollisionObject* object = (btCollisionObject*)proxy->m_clientObject;
        uint16_t           object_group = (uint16_t)proxy->m_collisionFilterGroup;
        uint16_t           object_mask = (uint16_t)proxy->m_collisionFilterMask;
        if ((object_group & filter->m_MaskBits) == 0 || (filter->m_CategoryBits & object_mask) == 0)
        {
            return false;
        }
        if (!filter->m_IncludeTriggers && !object->hasContactResponse())
        {
            return false;
        }
        if (IsIgnoredObject(filter, object))
        {
            return false;
        }
        return GetCollisionObjectOwner(object, 0, 0);
    }

    static bool HasResultCapacity(uint32_t result_count, int max_results)
    {
        return max_results == 0 || result_count < (uint32_t)max_results;
    }

    static bool ContainsObject(const dmArray<Bullet3DQueryObjectResult>& results, const btCollisionObject* object)
    {
        for (uint32_t i = 0; i < results.Size(); ++i)
        {
            if (results[i].m_Object == object)
            {
                return true;
            }
        }
        return false;
    }

    static bool ContainsInitialCastObject(const dmArray<Bullet3DCastResult>& results, const btCollisionObject* object)
    {
        for (uint32_t i = 0; i < results.Size(); ++i)
        {
            if (results[i].m_Object == object && results[i].m_InitialOverlap)
            {
                return true;
            }
        }
        return false;
    }

    static int GetShapeIndex(const btCollisionWorld::LocalShapeInfo* shape_info)
    {
        // Bullet stores the zero-based child index of a compound shape in
        // m_triangleIndex and uses a negative shape part for that case.
        return shape_info && shape_info->m_shapePart < 0 && shape_info->m_triangleIndex >= 0 ? shape_info->m_triangleIndex + 1 : 0;
    }

    static int CheckMaxResults(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            return 0;
        }

        lua_Number max_results = luaL_checknumber(L, index);
        if (!isfinite((double)max_results) || max_results < 0.0 || max_results > INT_MAX || floor((double)max_results) != max_results)
        {
            luaL_error(L, "max_results must be an integer between 0 and %d.", INT_MAX);
            return 0;
        }
        return (int)max_results;
    }

    static int CheckAsyncCastCallback(lua_State* L, int* max_results)
    {
        int top = lua_gettop(L);
        if (top < 4 || top > 6)
        {
            luaL_error(L, "expected callback after optional filter and max_results arguments");
            return 0;
        }
        luaL_checktype(L, top, LUA_TFUNCTION);
        *max_results = top == 6 ? CheckMaxResults(L, 5) : 0;
        return top;
    }

    static uint16_t CheckFilterBits(lua_State* L, int index, const char* field_name)
    {
        lua_Number value = luaL_checknumber(L, index);
        if (!isfinite((double)value) || value < 0.0 || value > 0xffff || floor((double)value) != value)
        {
            luaL_error(L, "%s must be an integer between 0 and 65535.", field_name);
            return 0;
        }
        return (uint16_t)value;
    }

    static bool CheckBooleanField(lua_State* L, int index, const char* field_name)
    {
        if (!lua_isboolean(L, index))
        {
            luaL_error(L, "%s must be a boolean.", field_name);
            return false;
        }
        return lua_toboolean(L, index) != 0;
    }

    static bool IsFiniteScalar(btScalar value)
    {
        return isfinite((double)value) != 0;
    }

    static void CheckFiniteVector3(lua_State* L, const btVector3& value, const char* field_name)
    {
        if (!IsFiniteScalar(value.getX()) || !IsFiniteScalar(value.getY()) || !IsFiniteScalar(value.getZ()))
        {
            luaL_error(L, "%s components must be finite.", field_name);
        }
    }

    static btVector3 CheckFiniteVector3(lua_State* L, int index, const char* field_name, float scale)
    {
        btVector3 value = CheckBullet3DVector3(L, index, scale);
        CheckFiniteVector3(L, value, field_name);
        return value;
    }

    static void InitializeQueryFilterInput(Bullet3DQueryFilterInput* input)
    {
        input->m_CategoryBits = 0xffff;
        input->m_MaskBits = 0xffff;
        input->m_IgnoreIndex = 0;
        input->m_IgnoreCount = 0;
        input->m_IgnoreIsArray = false;
        input->m_IncludeTriggers = true;
        input->m_ReportInitialOverlaps = false;
    }

    static void CheckQueryFilterInput(lua_State* L, int index, Bullet3DQueryFilterInput* input)
    {
        InitializeQueryFilterInput(input);

        if (lua_isnoneornil(L, index))
        {
            return;
        }

        index = AbsIndex(L, index);
        luaL_checktype(L, index, LUA_TTABLE);

        // Read every named field exactly once. In particular, this keeps an
        // __index metamethod from returning a different value after validation.
        lua_getfield(L, index, "category_bits");
        if (!lua_isnil(L, -1))
        {
            input->m_CategoryBits = CheckFilterBits(L, -1, "category_bits");
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "mask_bits");
        if (!lua_isnil(L, -1))
        {
            input->m_MaskBits = CheckFilterBits(L, -1, "mask_bits");
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "include_triggers");
        if (!lua_isnil(L, -1))
        {
            input->m_IncludeTriggers = CheckBooleanField(L, -1, "include_triggers");
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "report_initial_overlaps");
        if (!lua_isnil(L, -1))
        {
            input->m_ReportInitialOverlaps = CheckBooleanField(L, -1, "report_initial_overlaps");
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "ignore");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return;
        }

        input->m_IgnoreIndex = lua_gettop(L);
        input->m_IgnoreIsArray = lua_istable(L, -1);
        if (input->m_IgnoreIsArray)
        {
            input->m_IgnoreCount = (int)lua_objlen(L, -1);
            for (int i = 1; i <= input->m_IgnoreCount; ++i)
            {
                lua_rawgeti(L, input->m_IgnoreIndex, i);
                CheckBullet3DCollisionObject(L, -1);
                lua_pop(L, 1);
            }
            return;
        }
        CheckBullet3DCollisionObject(L, -1);
    }

    static void CreateQueryFilter(lua_State* L, const Bullet3DQueryFilterInput& input, Bullet3DQueryFilter* filter)
    {
        filter->m_CategoryBits = input.m_CategoryBits;
        filter->m_MaskBits = input.m_MaskBits;
        filter->m_IncludeTriggers = input.m_IncludeTriggers;
        filter->m_ReportInitialOverlaps = input.m_ReportInitialOverlaps;

        if (input.m_IgnoreIndex == 0)
        {
            return;
        }

        if (input.m_IgnoreIsArray)
        {
            if (input.m_IgnoreCount > 0)
            {
                filter->m_IgnoredObjects.SetCapacity(input.m_IgnoreCount);
            }
            for (int i = 1; i <= input.m_IgnoreCount; ++i)
            {
                lua_rawgeti(L, input.m_IgnoreIndex, i);
                filter->m_IgnoredObjects.Push(ToBullet3DCollisionObject(L, -1));
                lua_pop(L, 1);
            }
            return;
        }
        ArrayPush(&filter->m_IgnoredObjects, ToBullet3DCollisionObject(L, input.m_IgnoreIndex));
    }

    static void CheckQueryFilter(lua_State* L, int index, Bullet3DQueryFilter* filter)
    {
        int                      top = lua_gettop(L);
        Bullet3DQueryFilterInput input;
        CheckQueryFilterInput(L, index, &input);
        CreateQueryFilter(L, input, filter);
        lua_settop(L, top);
    }

    static void CreateAsyncQueryFilter(lua_State* L, const Bullet3DQueryFilterInput& input, Bullet3DAsyncQueryFilter* filter)
    {
        filter->m_CategoryBits = input.m_CategoryBits;
        filter->m_MaskBits = input.m_MaskBits;
        filter->m_IncludeTriggers = input.m_IncludeTriggers;
        filter->m_ReportInitialOverlaps = input.m_ReportInitialOverlaps;

        if (input.m_IgnoreIndex == 0)
        {
            return;
        }

        if (input.m_IgnoreIsArray)
        {
            if (input.m_IgnoreCount > 0)
            {
                filter->m_IgnoredObjectIds.SetCapacity(input.m_IgnoreCount);
            }
            for (int i = 1; i <= input.m_IgnoreCount; ++i)
            {
                lua_rawgeti(L, input.m_IgnoreIndex, i);
                filter->m_IgnoredObjectIds.Push(CheckBullet3DCollisionObjectId(L, -1));
                lua_pop(L, 1);
            }
            return;
        }
        ArrayPush(&filter->m_IgnoredObjectIds, CheckBullet3DCollisionObjectId(L, input.m_IgnoreIndex));
    }

    static void ResolveAsyncQueryFilter(lua_State* L, const Bullet3DAsyncQueryFilter& input, Bullet3DQueryFilter* filter)
    {
        filter->m_CategoryBits = input.m_CategoryBits;
        filter->m_MaskBits = input.m_MaskBits;
        filter->m_IncludeTriggers = input.m_IncludeTriggers;
        filter->m_ReportInitialOverlaps = input.m_ReportInitialOverlaps;
        if (!input.m_IgnoredObjectIds.Empty())
        {
            filter->m_IgnoredObjects.SetCapacity(input.m_IgnoredObjectIds.Size());
        }
        for (uint32_t i = 0; i < input.m_IgnoredObjectIds.Size(); ++i)
        {
            btCollisionObject* object = ToBullet3DCollisionObjectById(L, input.m_IgnoredObjectIds[i]);
            if (object)
            {
                filter->m_IgnoredObjects.Push(object);
            }
        }
    }

    static btVector3 CheckPositiveDimensions(lua_State* L, int index, const char* field_name)
    {
        btVector3 dimensions = CheckFiniteVector3(L, index, field_name, GetBullet3DPhysicsScale());
        if (!(dimensions.getX() > 0.0f) || !(dimensions.getY() > 0.0f) || !(dimensions.getZ() > 0.0f))
        {
            luaL_error(L, "%s components must be greater than zero.", field_name);
        }
        return dimensions;
    }

    static btScalar CheckPositiveLength(lua_State* L, int index, const char* field_name)
    {
        btScalar value = (btScalar)luaL_checknumber(L, index) * GetBullet3DPhysicsScale();
        if (!IsFiniteScalar(value) || !(value > 0.0f))
        {
            luaL_error(L, "%s must be finite and greater than zero.", field_name);
        }
        return value;
    }

    static void CheckQueryShapeInput(lua_State* L, int index, Bullet3DQueryShapeInput* input)
    {
        index = AbsIndex(L, index);
        luaL_checktype(L, index, LUA_TTABLE);

        input->m_Position = btVector3(0.0f, 0.0f, 0.0f);
        input->m_Dimensions = btVector3(0.0f, 0.0f, 0.0f);
        input->m_Rotation = btQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
        input->m_TargetRotation = input->m_Rotation;
        input->m_Diameter = 0.0f;
        input->m_Height = 0.0f;
        input->m_VerticesIndex = 0;
        input->m_VertexCount = 0;

        // Keep the parsed native values and any variable-length vertex table;
        // native allocations happen only after this validation phase succeeds.
        lua_getfield(L, index, "type");
        input->m_Type = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "position");
        if (!lua_isnil(L, -1))
        {
            input->m_Position = CheckFiniteVector3(L, -1, "position", GetBullet3DPhysicsScale());
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "rotation");
        if (!lua_isnil(L, -1))
        {
            input->m_Rotation = CheckBullet3DFiniteQuat(L, -1, "rotation");
            input->m_TargetRotation = input->m_Rotation;
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "target_rotation");
        if (!lua_isnil(L, -1))
        {
            input->m_TargetRotation = CheckBullet3DFiniteQuat(L, -1, "target_rotation");
        }
        lua_pop(L, 1);

        if (input->m_Type == dmPhysicsDDF::CollisionShape::TYPE_SPHERE)
        {
            lua_getfield(L, index, "diameter");
            input->m_Diameter = CheckPositiveLength(L, -1, "diameter");
            lua_pop(L, 1);
        }
        else if (input->m_Type == dmPhysicsDDF::CollisionShape::TYPE_BOX)
        {
            lua_getfield(L, index, "dimensions");
            input->m_Dimensions = CheckPositiveDimensions(L, -1, "dimensions");
            lua_pop(L, 1);
        }
        else if (input->m_Type == dmPhysicsDDF::CollisionShape::TYPE_CAPSULE)
        {
            lua_getfield(L, index, "diameter");
            input->m_Diameter = CheckPositiveLength(L, -1, "diameter");
            lua_pop(L, 1);
            lua_getfield(L, index, "height");
            input->m_Height = CheckPositiveLength(L, -1, "height");
            lua_pop(L, 1);
        }
        else if (input->m_Type == dmPhysicsDDF::CollisionShape::TYPE_HULL)
        {
            lua_getfield(L, index, "vertices");
            luaL_checktype(L, -1, LUA_TTABLE);
            input->m_VerticesIndex = lua_gettop(L);
            input->m_VertexCount = (int)lua_objlen(L, -1);
            if (input->m_VertexCount < 4)
            {
                luaL_error(L, "vertices must contain at least four points.");
            }
            for (int i = 1; i <= input->m_VertexCount; ++i)
            {
                lua_rawgeti(L, input->m_VerticesIndex, i);
                CheckFiniteVector3(L, -1, "vertices", GetBullet3DPhysicsScale());
                lua_pop(L, 1);
            }
            return;
        }
        else
        {
            luaL_error(L, "Unsupported shape type %d.", input->m_Type);
        }
    }

    static Bullet3DQueryShape CreateQueryShape(lua_State* L, const Bullet3DQueryShapeInput& input)
    {
        btConvexShape* shape = 0;
        if (input.m_Type == dmPhysicsDDF::CollisionShape::TYPE_SPHERE)
        {
            shape = new btSphereShape(input.m_Diameter * 0.5f);
        }
        else if (input.m_Type == dmPhysicsDDF::CollisionShape::TYPE_BOX)
        {
            shape = new btBoxShape(input.m_Dimensions * 0.5f);
        }
        else if (input.m_Type == dmPhysicsDDF::CollisionShape::TYPE_CAPSULE)
        {
            shape = new btCapsuleShape(input.m_Diameter * 0.5f, input.m_Height);
        }
        else
        {
            dmArray<btVector3> vertices;
            vertices.SetCapacity(input.m_VertexCount);
            for (int i = 1; i <= input.m_VertexCount; ++i)
            {
                lua_rawgeti(L, input.m_VerticesIndex, i);
                dmVMath::Vector3* vertex = dmScript::ToVector3(L, -1);
                // The pinned array was fully validated before any native allocation,
                // and no Lua code can run between validation and this raw array read.
                assert(vertex != 0);
                vertices.Push(btVector3(vertex->getX() * GetBullet3DPhysicsScale(), vertex->getY() * GetBullet3DPhysicsScale(), vertex->getZ() * GetBullet3DPhysicsScale()));
                lua_pop(L, 1);
            }
            shape = new btConvexHullShape((const btScalar*)vertices.Begin(), input.m_VertexCount, sizeof(btVector3));
        }

        Bullet3DQueryShape query_shape;
        query_shape.m_Shape = shape;
        query_shape.m_From = btTransform(input.m_Rotation, input.m_Position);
        query_shape.m_To = btTransform(input.m_TargetRotation, input.m_Position);
        return query_shape;
    }

    static bool CollisionObjectBelongsToWorld(const btDiscreteDynamicsWorld* world, const btCollisionObject* object)
    {
        const btCollisionObjectArray& objects = world->getCollisionObjectArray();
        for (int i = 0; i < objects.size(); ++i)
        {
            if (objects[i] == object)
            {
                return true;
            }
        }
        return false;
    }

    static void CheckCollisionObjectBelongsToWorld(lua_State* L, btDiscreteDynamicsWorld* world, btCollisionObject* object, const char* argument_name)
    {
        if (!CollisionObjectBelongsToWorld(world, object))
        {
            luaL_error(L, "%s does not belong to the supplied bullet3d world.", argument_name);
        }
    }

    static void CheckNonZeroTranslation(lua_State* L, const btVector3& translation)
    {
        btScalar length_squared = translation.length2();
        if (!IsFiniteScalar(length_squared))
        {
            luaL_error(L, "translation length must be finite.");
        }
        if (length_squared == 0.0f)
        {
            luaL_error(L, "translation must not be zero. Use an overlap query for stationary shapes.");
        }
    }

    static void SynchronizeWorldAABBs(btDiscreteDynamicsWorld* world)
    {
        // Script setters update the Bullet transform immediately. Refreshing all
        // broadphase bounds makes queries observe those transforms in the same frame.
        world->updateAabbs();
    }

    class Bullet3DAABBQueryCallback : public btBroadphaseAabbCallback
    {
        public:
        Bullet3DAABBQueryCallback(const Bullet3DQueryFilter* filter, dmArray<Bullet3DQueryObjectResult>* results, int max_results)
            : m_Filter(filter)
            , m_Results(results)
            , m_MaxResults(max_results)
        {
        }

        virtual bool process(const btBroadphaseProxy* proxy)
        {
            if (!HasResultCapacity(m_Results->Size(), m_MaxResults))
            {
                return false;
            }
            if (PassesQueryFilter(m_Filter, proxy))
            {
                Bullet3DQueryObjectResult result = { (btCollisionObject*)proxy->m_clientObject };
                ArrayPush(m_Results, result);
            }
            return HasResultCapacity(m_Results->Size(), m_MaxResults);
        }

        private:
        const Bullet3DQueryFilter*          m_Filter;
        dmArray<Bullet3DQueryObjectResult>* m_Results;
        int                                 m_MaxResults;
    };

    class Bullet3DRayQueryCallback : public btCollisionWorld::RayResultCallback
    {
        public:
        Bullet3DRayQueryCallback(const Bullet3DQueryFilter* filter, const btVector3& origin, const btVector3& target, dmArray<Bullet3DCastResult>* results, bool closest)
            : m_Filter(filter)
            , m_Origin(origin)
            , m_Target(target)
            , m_Results(results)
            , m_Closest(closest)
        {
            m_collisionFilterGroup = (short int)filter->m_CategoryBits;
            m_collisionFilterMask = (short int)filter->m_MaskBits;
        }

        virtual bool needsCollision(btBroadphaseProxy* proxy) const
        {
            return PassesQueryFilter(m_Filter, proxy);
        }

        virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& ray_result, bool normal_in_world_space)
        {
            if (ray_result.m_hitFraction <= 0.0f)
            {
                return m_closestHitFraction;
            }
            Bullet3DCastResult result;
            result.m_Object = ray_result.m_collisionObject;
            result.m_Point.setInterpolate3(m_Origin, m_Target, ray_result.m_hitFraction);
            result.m_Normal = normal_in_world_space ? ray_result.m_hitNormalLocal : result.m_Object->getWorldTransform().getBasis() * ray_result.m_hitNormalLocal;
            result.m_Fraction = ray_result.m_hitFraction;
            result.m_ShapeIndex = GetShapeIndex(ray_result.m_localShapeInfo);
            result.m_InitialOverlap = false;
            result.m_Inside = false;
            if (m_Closest)
            {
                if (m_Results->Empty())
                {
                    ArrayPush(m_Results, result);
                }
                else
                {
                    (*m_Results)[0] = result;
                }
                m_closestHitFraction = result.m_Fraction;
                return result.m_Fraction;
            }
            ArrayPush(m_Results, result);
            return m_closestHitFraction;
        }

        private:
        const Bullet3DQueryFilter*   m_Filter;
        btVector3                    m_Origin;
        btVector3                    m_Target;
        dmArray<Bullet3DCastResult>* m_Results;
        bool                         m_Closest;
    };

    class Bullet3DConvexQueryCallback : public btCollisionWorld::ConvexResultCallback
    {
        public:
        Bullet3DConvexQueryCallback(const Bullet3DQueryFilter* filter, dmArray<Bullet3DCastResult>* results, bool closest)
            : m_Filter(filter)
            , m_Results(results)
            , m_Closest(closest)
        {
            m_collisionFilterGroup = (short int)filter->m_CategoryBits;
            m_collisionFilterMask = (short int)filter->m_MaskBits;
        }

        virtual bool needsCollision(btBroadphaseProxy* proxy) const
        {
            return PassesQueryFilter(m_Filter, proxy);
        }

        virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convex_result, bool normal_in_world_space)
        {
            if (convex_result.m_hitFraction <= 0.0f)
            {
                return m_closestHitFraction;
            }
            Bullet3DCastResult result;
            result.m_Object = convex_result.m_hitCollisionObject;
            result.m_Point = convex_result.m_hitPointLocal;
            result.m_Normal = normal_in_world_space ? convex_result.m_hitNormalLocal : result.m_Object->getWorldTransform().getBasis() * convex_result.m_hitNormalLocal;
            result.m_Fraction = convex_result.m_hitFraction;
            result.m_ShapeIndex = GetShapeIndex(convex_result.m_localShapeInfo);
            result.m_InitialOverlap = false;
            result.m_Inside = false;
            if (m_Closest)
            {
                if (m_Results->Empty())
                {
                    ArrayPush(m_Results, result);
                }
                else
                {
                    (*m_Results)[0] = result;
                }
                m_closestHitFraction = result.m_Fraction;
                return result.m_Fraction;
            }
            ArrayPush(m_Results, result);
            return m_closestHitFraction;
        }

        private:
        const Bullet3DQueryFilter*   m_Filter;
        dmArray<Bullet3DCastResult>* m_Results;
        bool                         m_Closest;
    };

    class Bullet3DOverlapContactCallback : public btCollisionWorld::ContactResultCallback
    {
        public:
        Bullet3DOverlapContactCallback(const Bullet3DQueryFilter* filter, btCollisionObject* query_object, dmArray<Bullet3DQueryObjectResult>* results, int max_results)
            : m_Filter(filter)
            , m_QueryObject(query_object)
            , m_Results(results)
            , m_MaxResults(max_results)
        {
            m_collisionFilterGroup = (short int)filter->m_CategoryBits;
            m_collisionFilterMask = (short int)filter->m_MaskBits;
        }

        virtual bool needsCollision(btBroadphaseProxy* proxy) const
        {
            return HasResultCapacity(m_Results->Size(), m_MaxResults) && PassesQueryFilter(m_Filter, proxy);
        }

        virtual btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObject* col_obj_0, int part_id_0, int index_0, const btCollisionObject* col_obj_1, int part_id_1, int index_1)
        {
            (void)part_id_0;
            (void)index_0;
            (void)part_id_1;
            (void)index_1;
            if (cp.getDistance() > 0.0f || !HasResultCapacity(m_Results->Size(), m_MaxResults))
            {
                return 0.0f;
            }

            btCollisionObject* other = (btCollisionObject*)(col_obj_0 == m_QueryObject ? col_obj_1 : col_obj_0);
            if (other != m_QueryObject && !ContainsObject(*m_Results, other) && GetCollisionObjectOwner(other, 0, 0))
            {
                Bullet3DQueryObjectResult result = { other };
                ArrayPush(m_Results, result);
            }
            return 0.0f;
        }

        private:
        const Bullet3DQueryFilter*          m_Filter;
        btCollisionObject*                  m_QueryObject;
        dmArray<Bullet3DQueryObjectResult>* m_Results;
        int                                 m_MaxResults;
    };

    class Bullet3DContactQueryCallback : public btCollisionWorld::ContactResultCallback
    {
        public:
        Bullet3DContactQueryCallback(const Bullet3DQueryFilter* filter, btCollisionObject* object_a, dmArray<Bullet3DContactResult>* results, int max_results)
            : m_Filter(filter)
            , m_ObjectA(object_a)
            , m_Results(results)
            , m_MaxResults(max_results)
        {
            m_collisionFilterGroup = (short int)filter->m_CategoryBits;
            m_collisionFilterMask = (short int)filter->m_MaskBits;
        }

        virtual bool needsCollision(btBroadphaseProxy* proxy) const
        {
            return HasResultCapacity(m_Results->Size(), m_MaxResults) && PassesQueryFilter(m_Filter, proxy);
        }

        virtual btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObject* col_obj_0, int part_id_0, int index_0, const btCollisionObject* col_obj_1, int part_id_1, int index_1)
        {
            (void)part_id_0;
            (void)index_0;
            (void)part_id_1;
            (void)index_1;
            if (!HasResultCapacity(m_Results->Size(), m_MaxResults))
            {
                return 0.0f;
            }

            bool                     requested_is_object_0 = col_obj_0 == m_ObjectA;
            const btCollisionObject* object_a = requested_is_object_0 ? col_obj_0 : col_obj_1;
            const btCollisionObject* object_b = requested_is_object_0 ? col_obj_1 : col_obj_0;
            if (!GetCollisionObjectOwner((btCollisionObject*)object_a, 0, 0) || !GetCollisionObjectOwner((btCollisionObject*)object_b, 0, 0))
            {
                return 0.0f;
            }

            Bullet3DContactResult result;
            result.m_ObjectA = (btCollisionObject*)object_a;
            result.m_ObjectB = (btCollisionObject*)object_b;
            result.m_PositionA = requested_is_object_0 ? cp.getPositionWorldOnA() : cp.getPositionWorldOnB();
            result.m_PositionB = requested_is_object_0 ? cp.getPositionWorldOnB() : cp.getPositionWorldOnA();
            result.m_NormalOnB = requested_is_object_0 ? cp.m_normalWorldOnB : -cp.m_normalWorldOnB;
            result.m_Distance = cp.getDistance();
            ArrayPush(m_Results, result);
            return 0.0f;
        }

        private:
        const Bullet3DQueryFilter*      m_Filter;
        btCollisionObject*              m_ObjectA;
        dmArray<Bullet3DContactResult>* m_Results;
        int                             m_MaxResults;
    };

    static uint64_t WorldPtrToKey(const btDiscreteDynamicsWorld* world)
    {
        return (uint64_t)(uintptr_t)world;
    }

    static void EnsureWorldCapacity()
    {
        if (g_Bullet3DWorlds.Full())
        {
            g_Bullet3DWorlds.OffsetCapacity(16);
            g_Bullet3DWorldToId.OffsetCapacity(16);
        }
    }

    static uint64_t AllocateWorldId(lua_State* L)
    {
        if (g_NextBullet3DWorldId == UINT64_MAX)
        {
            luaL_error(L, "The bullet3d world identity space is exhausted.");
            return 0;
        }
        return ++g_NextBullet3DWorldId;
    }

    static void InvalidateWorldId(uint64_t id)
    {
        btDiscreteDynamicsWorld** world = g_Bullet3DWorlds.Get(id);
        if (!world)
        {
            return;
        }

        uint64_t  key = WorldPtrToKey(*world);
        uint64_t* mapped_id = g_Bullet3DWorldToId.Get(key);
        if (mapped_id && *mapped_id == id)
        {
            g_Bullet3DWorldToId.Erase(key);
        }
        g_Bullet3DWorlds.Erase(id);
    }

    static Bullet3DLuaWorld* CheckWorldInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaWorld*)dmScript::CheckUserType(L, index, TYPE_HASH_WORLD, "Expected user type " BULLET3D_TYPE_NAME_WORLD);
    }

    static Bullet3DLuaWorld* ToWorldInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaWorld*)dmScript::ToUserType(L, index, TYPE_HASH_WORLD);
    }

    btDiscreteDynamicsWorld* ToBullet3DWorld(lua_State* L, int index)
    {
        Bullet3DLuaWorld* lua_world = ToWorldInternal(L, index);
        if (!lua_world)
        {
            return 0;
        }
        btDiscreteDynamicsWorld** world = g_Bullet3DWorlds.Get(lua_world->m_Id);
        return world ? *world : 0;
    }

    bool IsBullet3DWorldValid(lua_State* L, int index)
    {
        return ToBullet3DWorld(L, index) != 0;
    }

    btDiscreteDynamicsWorld* CheckBullet3DWorld(lua_State* L, int index)
    {
        Bullet3DLuaWorld*         lua_world = CheckWorldInternal(L, index);
        btDiscreteDynamicsWorld** world_ptr = g_Bullet3DWorlds.Get(lua_world->m_Id);
        btDiscreteDynamicsWorld*  world = world_ptr ? *world_ptr : 0;
        if (!world)
        {
            luaL_error(L, "Invalid bullet3d world handle.");
            return 0;
        }
        return world;
    }

    void PushBullet3DWorld(lua_State* L, void* world_ptr, void* component_world)
    {
        if (!world_ptr)
        {
            lua_pushnil(L);
            return;
        }

        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)world_ptr;
        EnsureWorldCapacity();

        uint64_t  id = 0;
        uint64_t  key = WorldPtrToKey(world);
        uint64_t* existing_id = g_Bullet3DWorldToId.Get(key);
        if (existing_id && g_Bullet3DWorlds.Get(*existing_id))
        {
            id = *existing_id;
        }
        else
        {
            if (existing_id)
            {
                g_Bullet3DWorldToId.Erase(key);
            }
            id = AllocateWorldId(L);
            g_Bullet3DWorlds.Put(id, world);
            g_Bullet3DWorldToId.Put(key, id);
        }

        Bullet3DLuaWorld* lua_world = (Bullet3DLuaWorld*)lua_newuserdata(L, sizeof(Bullet3DLuaWorld));
        lua_world->m_Id = id;
        lua_world->m_ComponentWorld = component_world;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_WORLD);
        lua_setmetatable(L, -2);
    }

    void ScriptBullet3DInvalidateWorld(void* world_ptr)
    {
        if (!world_ptr)
        {
            return;
        }

        CancelAsyncCastRequests((btDiscreteDynamicsWorld*)world_ptr);

        uint64_t* id = g_Bullet3DWorldToId.Get(WorldPtrToKey((btDiscreteDynamicsWorld*)world_ptr));
        if (id)
        {
            InvalidateWorldId(*id);
        }
    }

    static int World_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, IsBullet3DWorldValid(L, 1));
        return 1;
    }

    static int World_GetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DWorld(L, 1)->getGravity(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int World_SetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaWorld* lua_world = CheckWorldInternal(L, 1);
        CheckBullet3DWorld(L, 1);
        CompCollisionObjectSetBullet3DWorldGravity(lua_world->m_ComponentWorld, *dmScript::CheckVector3(L, 2));
        return 0;
    }

    static int World_GetCollisionObjectCount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DWorld(L, 1)->getNumCollisionObjects());
        return 1;
    }

    static bool PushCollisionObjectResult(lua_State* L, btCollisionObject* object)
    {
        dmGameObject::HCollection collection = 0;
        dmhash_t                  instance_id = 0;
        if (!GetCollisionObjectOwner(object, &collection, &instance_id))
        {
            return false;
        }
        PushBullet3DCollisionObject(L, object, collection, instance_id);
        return true;
    }

    static void PushObjectResults(lua_State* L, const dmArray<Bullet3DQueryObjectResult>& results, int max_results)
    {
        lua_newtable(L);
        int output_count = 0;
        for (uint32_t i = 0; i < results.Size() && HasResultCapacity(output_count, max_results); ++i)
        {
            if (GetCollisionObjectOwner(results[i].m_Object, 0, 0))
            {
                lua_newtable(L);
                PushCollisionObjectResult(L, results[i].m_Object);
                lua_setfield(L, -2, "object");
                lua_rawseti(L, -2, ++output_count);
            }
        }
    }

    static void PushCastResult(lua_State* L, const Bullet3DCastResult& result)
    {
        lua_newtable(L);

        PushCollisionObjectResult(L, result.m_Object);
        lua_setfield(L, -2, "object");

        PushBullet3DVector3(L, result.m_Point, GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "point");

        PushBullet3DVector3(L, result.m_Normal, 1.0f);
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, result.m_Fraction);
        lua_setfield(L, -2, "fraction");

        lua_pushboolean(L, result.m_InitialOverlap);
        lua_setfield(L, -2, "initial_overlap");

        lua_pushboolean(L, result.m_Inside);
        lua_setfield(L, -2, "inside");

        if (result.m_ShapeIndex > 0)
        {
            lua_pushinteger(L, result.m_ShapeIndex);
            lua_setfield(L, -2, "shape_index");
        }
    }

    static int CompareCastResults(const void* a, const void* b)
    {
        btScalar fraction_a = ((const Bullet3DCastResult*)a)->m_Fraction;
        btScalar fraction_b = ((const Bullet3DCastResult*)b)->m_Fraction;
        return fraction_a < fraction_b ? -1 : fraction_a > fraction_b ? 1 :
                                                                        0;
    }

    static void SortCastResults(dmArray<Bullet3DCastResult>* results)
    {
        if (results->Size() > 1)
        {
            qsort(results->Begin(), results->Size(), sizeof(Bullet3DCastResult), CompareCastResults);
        }
    }

    static void PushCastResults(lua_State* L, const dmArray<Bullet3DCastResult>& results, int max_results)
    {
        lua_newtable(L);
        int output_count = 0;
        for (uint32_t i = 0; i < results.Size() && HasResultCapacity(output_count, max_results); ++i)
        {
            if (GetCollisionObjectOwner(results[i].m_Object, 0, 0))
            {
                PushCastResult(L, results[i]);
                lua_rawseti(L, -2, ++output_count);
            }
        }
    }

    static void PushContactResult(lua_State* L, const Bullet3DContactResult& result)
    {
        lua_newtable(L);

        PushCollisionObjectResult(L, result.m_ObjectA);
        lua_setfield(L, -2, "object_a");

        PushCollisionObjectResult(L, result.m_ObjectB);
        lua_setfield(L, -2, "object_b");

        PushBullet3DVector3(L, result.m_PositionA, GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "position_a");

        PushBullet3DVector3(L, result.m_PositionB, GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "position_b");

        PushBullet3DVector3(L, result.m_NormalOnB, 1.0f);
        lua_setfield(L, -2, "normal_on_b");

        lua_pushnumber(L, result.m_Distance * GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "distance");
    }

    static void PushContactResults(lua_State* L, const dmArray<Bullet3DContactResult>& results, int max_results)
    {
        lua_newtable(L);
        int output_count = 0;
        for (uint32_t i = 0; i < results.Size() && HasResultCapacity(output_count, max_results); ++i)
        {
            if (GetCollisionObjectOwner(results[i].m_ObjectA, 0, 0) && GetCollisionObjectOwner(results[i].m_ObjectB, 0, 0))
            {
                PushContactResult(L, results[i]);
                lua_rawseti(L, -2, ++output_count);
            }
        }
    }

    static void CollectShapeOverlaps(btDiscreteDynamicsWorld* world, btConvexShape* shape, const btTransform& transform, const Bullet3DQueryFilter* filter, int max_results, dmArray<Bullet3DQueryObjectResult>* results)
    {
        btCollisionObject query_object;
        query_object.setCollisionShape(shape);
        query_object.setWorldTransform(transform);
        Bullet3DOverlapContactCallback callback(filter, &query_object, results, max_results);
        world->contactTest(&query_object, callback);
    }

    static void CollectInitialCastOverlaps(btDiscreteDynamicsWorld* world, btConvexShape* shape, const btTransform& transform, const Bullet3DQueryFilter* filter, bool inside, int max_results, dmArray<Bullet3DCastResult>* results)
    {
        if (!filter->m_ReportInitialOverlaps)
        {
            return;
        }

        dmArray<Bullet3DQueryObjectResult> overlaps;
        CollectShapeOverlaps(world, shape, transform, filter, max_results, &overlaps);
        for (uint32_t i = 0; i < overlaps.Size() && HasResultCapacity(results->Size(), max_results); ++i)
        {
            if (ContainsInitialCastObject(*results, overlaps[i].m_Object))
            {
                continue;
            }

            Bullet3DCastResult result;
            result.m_Object = overlaps[i].m_Object;
            result.m_Point = transform.getOrigin();
            result.m_Normal = btVector3(0.0f, 0.0f, 0.0f);
            result.m_Fraction = 0.0f;
            result.m_ShapeIndex = 0;
            result.m_InitialOverlap = true;
            result.m_Inside = inside;
            ArrayPush(results, result);
        }
    }

    static void ExecuteAsyncCastRequest(Bullet3DAsyncCastRequest* request)
    {
        if (!dmScript::IsCallbackValid(request->m_Callback))
        {
            return;
        }
        lua_State* L = dmScript::GetCallbackLuaContext(request->m_Callback);
        Bullet3DQueryFilter filter;
        ResolveAsyncQueryFilter(L, request->m_Filter, &filter);

        SynchronizeWorldAABBs(request->m_World);
        dmArray<Bullet3DCastResult> results;
        bool                        closest = request->m_MaxResults == 1;
        if (request->m_Type == BULLET3D_ASYNC_CAST_RAY)
        {
            btSphereShape point_shape(0.0f);
            btTransform   origin_transform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f), request->m_RayOrigin);
            CollectInitialCastOverlaps(request->m_World, &point_shape, origin_transform, &filter, true, request->m_MaxResults, &results);
            if (!closest || results.Empty())
            {
                Bullet3DRayQueryCallback callback(&filter, request->m_RayOrigin, request->m_RayTarget, &results, closest);
                request->m_World->rayTest(request->m_RayOrigin, request->m_RayTarget, callback);
            }
        }
        else
        {
            CollectInitialCastOverlaps(request->m_World, request->m_QueryShape.m_Shape, request->m_QueryShape.m_From, &filter, false, request->m_MaxResults, &results);
            if (!closest || results.Empty())
            {
                Bullet3DConvexQueryCallback callback(&filter, &results, closest);
                request->m_World->convexSweepTest(request->m_QueryShape.m_Shape, request->m_QueryShape.m_From, request->m_QueryShape.m_To, callback);
            }
        }
        SortCastResults(&results);

        DM_LUA_STACK_CHECK(L, 0);
        if (!dmScript::SetupCallback(request->m_Callback))
        {
            dmLogError("Failed to setup Bullet3D cast callback (has the calling script been destroyed?)");
            return;
        }
        PushCastResults(L, results, request->m_MaxResults);
        dmScript::PCall(L, 2, 0);
        dmScript::TeardownCallback(request->m_Callback);
    }

    void ScriptBullet3DProcessWorldQueries(void* world_ptr)
    {
        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)world_ptr;
        dmArray<Bullet3DAsyncCastRequest*> requests;
        uint32_t pending_count = g_Bullet3DAsyncCastRequests.Size();
        if (pending_count == 0)
        {
            return;
        }
        requests.SetCapacity(pending_count);

        uint32_t retained_count = 0;
        for (uint32_t i = 0; i < pending_count; ++i)
        {
            Bullet3DAsyncCastRequest* request = g_Bullet3DAsyncCastRequests[i];
            if (request->m_World == world)
            {
                requests.Push(request);
            }
            else
            {
                g_Bullet3DAsyncCastRequests[retained_count++] = request;
            }
        }
        g_Bullet3DAsyncCastRequests.SetSize(retained_count);

        for (uint32_t i = 0; i < requests.Size(); ++i)
        {
            Bullet3DAsyncCastRequest* request = requests[i];
            btDiscreteDynamicsWorld** registered_world = g_Bullet3DWorlds.Get(request->m_WorldId);
            if (registered_world && *registered_world == request->m_World)
            {
                ExecuteAsyncCastRequest(request);
            }
            DestroyAsyncCastRequest(request);
        }
    }

    static int World_OverlapAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_getfield(L, 2, "lower");
        btVector3 lower = CheckFiniteVector3(L, -1, "aabb.lower", GetBullet3DPhysicsScale());
        lua_pop(L, 1);
        lua_getfield(L, 2, "upper");
        btVector3 upper = CheckFiniteVector3(L, -1, "aabb.upper", GetBullet3DPhysicsScale());
        lua_pop(L, 1);
        if (lower.getX() > upper.getX() || lower.getY() > upper.getY() || lower.getZ() > upper.getZ())
        {
            return luaL_error(L, "aabb.lower components must not exceed aabb.upper components.");
        }

        int                 max_results = CheckMaxResults(L, 4);
        Bullet3DQueryFilter filter;
        CheckQueryFilter(L, 3, &filter);

        SynchronizeWorldAABBs(world);
        dmArray<Bullet3DQueryObjectResult> results;
        Bullet3DAABBQueryCallback          callback(&filter, &results, max_results);
        world->getBroadphase()->aabbTest(lower, upper, callback);
        PushObjectResults(L, results, max_results);
        return 1;
    }

    static int World_OverlapPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        btVector3                point = CheckFiniteVector3(L, 2, "point", GetBullet3DPhysicsScale());
        int                      max_results = CheckMaxResults(L, 4);
        Bullet3DQueryFilter      filter;
        CheckQueryFilter(L, 3, &filter);

        SynchronizeWorldAABBs(world);
        btSphereShape                      point_shape(0.0f);
        dmArray<Bullet3DQueryObjectResult> results;
        CollectShapeOverlaps(world, &point_shape, btTransform(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f), point), &filter, max_results, &results);
        PushObjectResults(L, results, max_results);
        return 1;
    }

    static int World_OverlapShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        int                      max_results = CheckMaxResults(L, 4);
        int                      top = lua_gettop(L);
        Bullet3DQueryFilterInput filter_input;
        Bullet3DQueryShapeInput  shape_input;
        CheckQueryFilterInput(L, 3, &filter_input);
        CheckQueryShapeInput(L, 2, &shape_input);
        Bullet3DQueryFilter filter;
        CreateQueryFilter(L, filter_input, &filter);
        Bullet3DQueryShape query_shape = CreateQueryShape(L, shape_input);
        lua_settop(L, top);

        SynchronizeWorldAABBs(world);
        dmArray<Bullet3DQueryObjectResult> results;
        CollectShapeOverlaps(world, query_shape.m_Shape, query_shape.m_From, &filter, max_results, &results);
        delete query_shape.m_Shape;
        PushObjectResults(L, results, max_results);
        return 1;
    }

    static int World_CastRayAsync(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaWorld*         lua_world = CheckWorldInternal(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        btVector3                 origin = CheckFiniteVector3(L, 2, "origin", GetBullet3DPhysicsScale());
        btVector3                 translation = CheckFiniteVector3(L, 3, "translation", GetBullet3DPhysicsScale());
        CheckNonZeroTranslation(L, translation);
        btVector3 target = origin + translation;
        CheckFiniteVector3(L, target, "ray target");

        int max_results = 0;
        int callback_index = CheckAsyncCastCallback(L, &max_results);
        int top = lua_gettop(L);
        Bullet3DQueryFilterInput filter_input;
        if (callback_index == 4)
        {
            InitializeQueryFilterInput(&filter_input);
        }
        else
        {
            CheckQueryFilterInput(L, 4, &filter_input);
        }

        Bullet3DAsyncCastRequest* request = new Bullet3DAsyncCastRequest;
        request->m_World = world;
        request->m_WorldId = lua_world->m_Id;
        request->m_Callback = 0;
        request->m_QueryShape.m_Shape = 0;
        request->m_RayOrigin = origin;
        request->m_RayTarget = target;
        request->m_MaxResults = max_results;
        request->m_Type = BULLET3D_ASYNC_CAST_RAY;
        CreateAsyncQueryFilter(L, filter_input, &request->m_Filter);
        lua_settop(L, top);
        request->m_Callback = dmScript::CreateCallback(L, callback_index);
        if (!request->m_Callback)
        {
            delete request;
            return luaL_error(L, "could not create callback for bullet3d.world.cast_ray_async");
        }
        ArrayPush(&g_Bullet3DAsyncCastRequests, request);
        return 0;
    }

    static int World_CastShapeAsync(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaWorld*         lua_world = CheckWorldInternal(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        btVector3                 translation = CheckFiniteVector3(L, 3, "translation", GetBullet3DPhysicsScale());
        CheckNonZeroTranslation(L, translation);
        int max_results = 0;
        int callback_index = CheckAsyncCastCallback(L, &max_results);
        int top = lua_gettop(L);

        Bullet3DQueryFilterInput filter_input;
        if (callback_index == 4)
        {
            InitializeQueryFilterInput(&filter_input);
        }
        else
        {
            CheckQueryFilterInput(L, 4, &filter_input);
        }
        Bullet3DQueryShapeInput shape_input;
        CheckQueryShapeInput(L, 2, &shape_input);
        btVector3 target = shape_input.m_Position + translation;
        CheckFiniteVector3(L, target, "shape target");

        Bullet3DAsyncCastRequest* request = new Bullet3DAsyncCastRequest;
        request->m_World = world;
        request->m_WorldId = lua_world->m_Id;
        request->m_Callback = 0;
        request->m_RayOrigin = btVector3(0.0f, 0.0f, 0.0f);
        request->m_RayTarget = btVector3(0.0f, 0.0f, 0.0f);
        request->m_MaxResults = max_results;
        request->m_Type = BULLET3D_ASYNC_CAST_SHAPE;
        CreateAsyncQueryFilter(L, filter_input, &request->m_Filter);
        request->m_QueryShape = CreateQueryShape(L, shape_input);
        request->m_QueryShape.m_To.setOrigin(target);
        lua_settop(L, top);

        request->m_Callback = dmScript::CreateCallback(L, callback_index);
        if (!request->m_Callback)
        {
            delete request->m_QueryShape.m_Shape;
            delete request;
            return luaL_error(L, "could not create callback for bullet3d.world.cast_shape_async");
        }
        ArrayPush(&g_Bullet3DAsyncCastRequests, request);
        return 0;
    }

    static int World_ContactTest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        btCollisionObject*       object = CheckBullet3DCollisionObject(L, 2);
        CheckCollisionObjectBelongsToWorld(L, world, object, "object");
        int                 max_results = CheckMaxResults(L, 4);
        Bullet3DQueryFilter filter;
        CheckQueryFilter(L, 3, &filter);

        SynchronizeWorldAABBs(world);
        dmArray<Bullet3DContactResult> results;
        Bullet3DContactQueryCallback   callback(&filter, object, &results, max_results);
        world->contactTest(object, callback);
        PushContactResults(L, results, max_results);
        return 1;
    }

    static int World_ContactPairTest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        btCollisionObject*       object_a = CheckBullet3DCollisionObject(L, 2);
        btCollisionObject*       object_b = CheckBullet3DCollisionObject(L, 3);
        CheckCollisionObjectBelongsToWorld(L, world, object_a, "object_a");
        CheckCollisionObjectBelongsToWorld(L, world, object_b, "object_b");
        if (object_a == object_b)
        {
            return luaL_error(L, "object_a and object_b must refer to different collision objects.");
        }
        int                 max_results = CheckMaxResults(L, 4);

        Bullet3DQueryFilter filter;
        filter.m_CategoryBits = 0xffff;
        filter.m_MaskBits = 0xffff;
        filter.m_IncludeTriggers = true;
        filter.m_ReportInitialOverlaps = false;

        SynchronizeWorldAABBs(world);
        dmArray<Bullet3DContactResult> results;
        Bullet3DContactQueryCallback   callback(&filter, object_a, &results, max_results);
        world->contactPairTest(object_a, object_b, callback);
        PushContactResults(L, results, max_results);
        return 1;
    }

    static int World_GetCollisionObjects(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btDiscreteDynamicsWorld*           world = CheckBullet3DWorld(L, 1);
        int                                max_results = CheckMaxResults(L, 2);

        dmArray<Bullet3DQueryObjectResult> results;
        const btCollisionObjectArray&      objects = world->getCollisionObjectArray();
        for (int i = 0; i < objects.size() && HasResultCapacity(results.Size(), max_results); ++i)
        {
            if (GetCollisionObjectOwner(objects[i], 0, 0))
            {
                Bullet3DQueryObjectResult result = { objects[i] };
                ArrayPush(&results, result);
            }
        }
        PushObjectResults(L, results, max_results);
        return 1;
    }

    static int World_tostring(lua_State* L)
    {
        btDiscreteDynamicsWorld* world = CheckBullet3DWorld(L, 1);
        lua_pushfstring(L, "Bullet3D.%s = %p", BULLET3D_TYPE_NAME_WORLD, world);
        return 1;
    }

    static int World_eq(lua_State* L)
    {
        Bullet3DLuaWorld* a = ToWorldInternal(L, 1);
        Bullet3DLuaWorld* b = ToWorldInternal(L, 2);
        lua_pushboolean(L, a && b && a->m_Id == b->m_Id);
        return 1;
    }

    static const luaL_reg World_methods[] = {
        { 0, 0 }
    };

    static const luaL_reg World_meta[] = {
        { "__tostring", World_tostring },
        { "__eq", World_eq },
        { 0, 0 }
    };

    static const luaL_reg World_functions[] = {
        { "is_valid", World_IsValid },

        { "get_gravity", World_GetGravity },
        { "set_gravity", World_SetGravity },

        { "get_collision_object_count", World_GetCollisionObjectCount },
        { "get_num_collision_objects", World_GetCollisionObjectCount },

        { "overlap_aabb", World_OverlapAABB },
        { "overlap_point", World_OverlapPoint },
        { "overlap_shape", World_OverlapShape },

        { "cast_ray_async", World_CastRayAsync },
        { "cast_shape_async", World_CastShapeAsync },

        { "contact_test", World_ContactTest },
        { "contact_pair_test", World_ContactPairTest },

        { "get_collision_objects", World_GetCollisionObjects },
        { 0, 0 }
    };

    void ScriptBullet3DInitializeWorld(lua_State* L)
    {
        TYPE_HASH_WORLD = dmScript::RegisterUserType(L, BULLET3D_TYPE_NAME_WORLD, World_methods, World_meta);

        lua_newtable(L);
        luaL_register(L, 0, World_functions);
        lua_setfield(L, -2, "world");
    }

    void ScriptBullet3DFinalizeWorld()
    {
        CancelAsyncCastRequests(0);
        TYPE_HASH_WORLD = 0;
        g_Bullet3DWorlds.Clear();
        g_Bullet3DWorldToId.Clear();
    }
} // namespace dmGameSystem

/*# Bullet dynamics world API
 *
 * Read and tune the Bullet dynamics world owned by the current collection.
 * Defold remains responsible for world lifetime, stepping, collision objects,
 * callbacks, and debug drawing.
 *
 * World and collision-object values returned by this API are borrowed,
 * generational handles. They become invalid when their collection or owning
 * game object is deleted and must not be retained as native pointers.
 *
 * All positions, distances, translations, dimensions and contact distances use
 * Defold world units. The binding converts them using `physics.scale`. Rotations
 * and unit normals are not scaled. Query functions refresh Bullet broadphase
 * AABBs before execution, so collision-object transform changes are visible.
 *
 * Query filters are optional tables with these fields:
 *
 * `category_bits`
 * : [type:number] unsigned 16-bit category bits, default `65535`
 *
 * `mask_bits`
 * : [type:number] unsigned 16-bit mask bits, default `65535`
 *
 * `include_triggers`
 * : [type:boolean] include objects without contact response, default `true`
 *
 * `ignore`
 * : [type:btCollisionObject|table] one collision-object handle or an array of handles to exclude
 *
 * `report_initial_overlaps`
 * : [type:boolean] report shapes overlapping the cast origin as synthesized fraction-zero hits, default `false`
 *
 * `report_initial_overlaps` is a `bullet3d.world` query option only. It does
 * not change `physics.raycast()` or either Box2D backend.
 *
 * Category and mask checks are reciprocal: the query category must match the
 * object's mask and the object's category must match the query mask.
 *
 * Temporary query shapes use the same size fields as [ref:physics.get_shape]:
 * a sphere has `type` and `diameter`, a box has `type` and `dimensions`, a
 * Y-axis capsule has `type`, `diameter` and `height`, and a convex hull has
 * `type` and a `vertices` array with at least four `vector3` values. The `type`
 * is one of the `physics.SHAPE_TYPE_*` constants. Every shape can specify
 * `position` and `rotation`; their defaults are zero and the identity rotation.
 * Cast shapes can also specify `target_rotation`, which defaults to `rotation`.
 * Capsule `height` is the length of the cylindrical middle section; total
 * end-to-end height is `height + diameter`. Query sizes are always expressed
 * in Defold world units. The legacy Bullet3D `physics.get_shape` result exposes
 * native-scaled primitive sizes when `physics.scale` differs from one, so such
 * a table must be converted back to world units before it is used as a query
 * shape. `physics.get_shape` also does not return convex-hull vertices, so a
 * hull query table must supply its own `vertices` array.
 * Hull vertices describe a convex hull; concave input is convexified by Bullet.
 * All query vectors and scalar sizes must be finite. Diameters, dimensions and
 * capsule heights must be greater than zero; hulls require at least four finite
 * vertices. Cast translations must be finite and non-zero. Query rotations
 * must be finite, non-zero quaternions and are normalized by the binding. AABB
 * lower bounds must not exceed their corresponding upper bounds.
 *
 * Overlap and enumeration results are arrays of `{ object = handle }` tables.
 * Cast results contain `object`, `point`, `normal`, `fraction`,
 * `initial_overlap`, and `inside`. `shape_index` is present when Bullet reports
 * a compound child and is one-based. Cast arrays are sorted by ascending
 * fraction. `fraction` is in `[0, 1]` along the supplied translation. For native
 * hits, `normal` is the hit object's outward unit surface normal; synthesized
 * initial-overlap hits use a zero normal. `inside` is true for a synthesized
 * ray-origin hit when Bullet reports signed contact distance less than or equal
 * to zero. It denotes initial contact or penetration rather than strict
 * geometric containment, and exact-surface cases follow Bullet's contact
 * tolerance. Shape-cast initial overlaps set only `initial_overlap`.
 *
 * Contact results contain `object_a`, `object_b`, `position_a`, `position_b`,
 * `normal_on_b`, and signed `distance`. Positions are points on their named
 * objects, and `normal_on_b` points from object B toward object A. A negative
 * distance is penetration and a small positive distance is Bullet's contact
 * margin. Object order is always normalized to the order supplied by the caller.
 *
 * `max_results` is optional. Zero or omission means unlimited results. A
 * negative value is an error. Broadphase overlaps, native world enumeration,
 * contacts, and equal-fraction cast hits have unspecified order. A capped query
 * can therefore return a different equal-priority subset after world changes.
 * Overlap, contact, and enumeration queries execute immediately and do not
 * advance simulation. Ray and shape casts are asynchronous: they are queued,
 * executed after the next physics step, and delivered to a callback. A callback
 * queued from another cast callback is deferred until a later physics step.
 * Native fraction-zero cast callbacks are suppressed. Starting overlaps are
 * omitted by default, or reported through the exact, deduplicated synthesis
 * enabled by `report_initial_overlaps`; this avoids direction-dependent Bullet
 * results for casts that start touching or penetrating another object.
 *
 * @document
 * @name bullet3d.world
 * @namespace bullet3d.world
 * @language Lua
 */

/*# Test whether a world handle is valid
 * @name bullet3d.world.is_valid
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return valid [type:boolean] `true` if the native world still exists
 */

/*# Get world gravity
 * @name bullet3d.world.get_gravity
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Set world gravity
 * @name bullet3d.world.set_gravity
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Get the number of collision objects in the world
 * @name bullet3d.world.get_collision_object_count
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return count [type:number] number of collision objects
 */

/*# Get the number of collision objects in the world
 * Alias for [ref:bullet3d.world.get_collision_object_count].
 *
 * @name bullet3d.world.get_num_collision_objects
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @return count [type:number] number of collision objects
 */

/*# Enumerate collision objects
 * Returns the Defold-owned collision objects currently registered in the world.
 * Internal or unmanaged Bullet objects without Defold ownership metadata are
 * not exposed.
 *
 * @name bullet3d.world.get_collision_objects
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param [max_results] [type:number] maximum number of results, or zero for all
 * @return results [type:table] array of `{ object = btCollisionObject }` tables
 */

/*# Find broadphase AABB overlaps
 * Finds collision objects whose Bullet broadphase bounds overlap the supplied
 * world-space AABB. This is intentionally a broadphase query and can include
 * objects whose actual collision geometry does not intersect the box. Use
 * [ref:bullet3d.world.overlap_point] or
 * [ref:bullet3d.world.overlap_shape] for exact narrow-phase overlap tests.
 *
 * @name bullet3d.world.overlap_aabb
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param aabb [type:table] table with world-space `lower` and `upper` vector3 bounds
 * @param [filter] [type:table] query filter
 * @param [max_results] [type:number] maximum number of results, or zero for all
 * @return results [type:table] overlap-result array
 */

/*# Find collision objects containing a point
 * Performs an exact narrow-phase test using a temporary zero-radius Bullet
 * sphere at the world-space point. A result is returned only for a contact with
 * signed distance less than or equal to zero, so broadphase-only false positives
 * are removed. Results on an exact surface follow Bullet's contact tolerance.
 *
 * @name bullet3d.world.overlap_point
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param point [type:vector3] point in world space
 * @param [filter] [type:table] query filter
 * @param [max_results] [type:number] maximum number of results, or zero for all
 * @return results [type:table] overlap-result array
 */

/*# Find collision objects overlapping a convex shape
 * Performs an exact Bullet contact test for a temporary sphere, box, Y-axis
 * capsule, or convex hull. Multiple native contact points for the same target
 * object are deduplicated in the returned overlap array.
 *
 * @name bullet3d.world.overlap_shape
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param shape [type:table] convex query-shape table
 * @param [filter] [type:table] query filter
 * @param [max_results] [type:number] maximum number of results, or zero for all
 * @return results [type:table] overlap-result array
 */

/*# Cast a ray asynchronously
 * Casts from `origin` to `origin + translation`. Translation must be non-zero.
 * The request returns without running the query. After the next physics step,
 * `callback(self, hits)` receives the cast-result array sorted by fraction.
 * Bullet 2.77 normally does not report a ray whose start and end are both inside
 * the same convex hull. Set `filter.report_initial_overlaps = true` to perform
 * an exact point-overlap test at the origin and synthesize one deduplicated hit
 * per initially touching or overlapping object with `fraction = 0`, zero `normal`,
 * `point = origin`, `initial_overlap = true`, and `inside = true`. The point is
 * the query origin, not a surface contact. This explicitly supports the
 * inside-hull behavior requested by issue #5348. Fraction-zero native callbacks
 * and starting overlaps are suppressed when the option is false.
 *
 * @name bullet3d.world.cast_ray_async
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param origin [type:vector3] ray origin in world space
 * @param translation [type:vector3] non-zero ray displacement in world units
 * @param [filter] [type:table] query filter
 * @param [max_results] [type:number] maximum sorted hits, or zero for all
 * @param callback [type:function] function called as `callback(self, hits)`
 */

/*# Sweep a convex shape asynchronously
 * Sweeps the temporary shape from `shape.position` by `translation`, while
 * interpolating from `shape.rotation` to `shape.target_rotation`. Translation
 * must be non-zero. Bullet's convex sweep supports only convex query shapes.
 * The request returns without running the query. After the next physics step,
 * `callback(self, hits)` receives the cast-result array sorted by fraction.
 *
 * When `filter.report_initial_overlaps` is true, an exact contact test at the
 * starting transform synthesizes one deduplicated hit per overlapping object
 * with `fraction = 0`, `point = shape.position`, zero `normal`,
 * `initial_overlap = true`, and `inside = false`. The point is the query-shape
 * origin, not a surface contact, and the result does not report penetration depth.
 *
 * @name bullet3d.world.cast_shape_async
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param shape [type:table] convex query-shape table with optional target rotation
 * @param translation [type:vector3] non-zero sweep displacement in world units
 * @param [filter] [type:table] query filter
 * @param [max_results] [type:number] maximum sorted hits, or zero for all
 * @param callback [type:function] function called as `callback(self, hits)`
 */

/*# Test one collision object against the world
 * Runs Bullet's discrete contact test between `object` and matching objects in
 * the same world. The supplied object is always `object_a` in returned contacts.
 * The borrowed collision-object handle must belong to `world`. Bullet may return
 * several contact points for one object pair and may include small positive
 * contact-margin distances.
 *
 * @name bullet3d.world.contact_test
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param object [type:btCollisionObject] collision object belonging to the world
 * @param [filter] [type:table] filter applied to candidate `object_b` values
 * @param [max_results] [type:number] maximum number of contact points, or zero for all
 * @return contacts [type:table] normalized contact-result array
 */

/*# Test a collision-object pair
 * Runs Bullet's discrete pair contact algorithm without changing the simulation.
 * Both borrowed handles must belong to `world` and must identify different
 * objects. The output preserves the caller's A/B order even when Bullet's
 * internal manifold order is reversed. Collision filters are not applied to an
 * explicitly selected pair.
 *
 * @name bullet3d.world.contact_pair_test
 * @param world [type:btDiscreteDynamicsWorld] world handle
 * @param object_a [type:btCollisionObject] first collision object in the world
 * @param object_b [type:btCollisionObject] different second collision object in the world
 * @param [max_results] [type:number] maximum number of contact points, or zero for all
 * @return contacts [type:table] normalized contact-result array
 */
