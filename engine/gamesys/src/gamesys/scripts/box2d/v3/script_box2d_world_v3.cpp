// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <string.h>

#include <box2d/box2d.h>

#include <dlib/array.h>
#include <script/script.h>
#include <gameobject/script.h>

#include "script_box2d_v3.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static uint32_t TYPE_HASH_WORLD = 0;

    #define BOX2D_TYPE_NAME_WORLD "b2world"

    struct B2DLuaWorld
    {
        b2WorldId m_World;
    };

    struct QueryContext
    {
        lua_State* m_L;
        int        m_TableIndex;
        int        m_Count;
        int        m_MaxResults;
    };

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

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static bool HasResultCapacity(const QueryContext* context)
    {
        return context->m_MaxResults <= 0 || context->m_Count < context->m_MaxResults;
    }

    static int CheckMaxResults(lua_State* L, int index)
    {
        if (lua_isnoneornil(L, index))
        {
            return 0;
        }

        int max_results = luaL_checkinteger(L, index);
        if (max_results < 0)
        {
            luaL_error(L, "max_results must be >= 0.");
            return 0;
        }
        return max_results;
    }

    static B2DLuaWorld* CheckWorldInternal(lua_State* L, int index)
    {
        return (B2DLuaWorld*)dmScript::CheckUserType(L, index, TYPE_HASH_WORLD, "Expected user type " BOX2D_TYPE_NAME_WORLD);
    }

    b2WorldId* CheckWorld(lua_State* L, int index)
    {
        B2DLuaWorld* luaworld = CheckWorldInternal(L, index);
        if (!b2World_IsValid(luaworld->m_World))
        {
            luaL_error(L, "Invalid b2world handle.");
            return 0;
        }
        return &luaworld->m_World;
    }

    static b2WorldId* ToWorld(lua_State* L, int index)
    {
        B2DLuaWorld* luaworld = (B2DLuaWorld*)dmScript::ToUserType(L, index, TYPE_HASH_WORLD);
        return luaworld ? &luaworld->m_World : 0;
    }

    void PushWorldId(lua_State* L, b2WorldId world_id)
    {
        B2DLuaWorld* luaworld = (B2DLuaWorld*)lua_newuserdata(L, sizeof(B2DLuaWorld));
        luaworld->m_World = world_id;

        luaL_getmetatable(L, BOX2D_TYPE_NAME_WORLD);
        lua_setmetatable(L, -2);
    }

    void PushWorld(lua_State* L, void* world)
    {
        if (world)
        {
            PushWorldId(L, *(b2WorldId*)world);
        }
        else
        {
            lua_pushnil(L);
        }
    }

    static void CheckWorldUnlocked(lua_State* L, b2WorldId world, const char* function_name)
    {
        if (b2World_IsLocked(world))
        {
            luaL_error(L, "Could not call b2d.world.%s. The world is locked.", function_name);
        }
    }

    static b2QueryFilter CheckQueryFilter(lua_State* L, int index)
    {
        b2QueryFilter filter = b2DefaultQueryFilter();
        if (lua_isnoneornil(L, index))
        {
            return filter;
        }

        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "category_bits");
        if (!lua_isnil(L, -1))
        {
            filter.categoryBits = (uint64_t)luaL_checknumber(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "mask_bits");
        if (!lua_isnil(L, -1))
        {
            filter.maskBits = (uint64_t)luaL_checknumber(L, -1);
        }
        lua_pop(L, 1);

        return filter;
    }

    static b2AABB CheckAABB(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2AABB aabb;

        lua_getfield(L, index, "lower");
        aabb.lowerBound = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        lua_getfield(L, index, "upper");
        aabb.upperBound = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        return aabb;
    }

    static b2Capsule CheckCapsule(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2Capsule capsule;

        lua_getfield(L, index, "center1");
        capsule.center1 = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        lua_getfield(L, index, "center2");
        capsule.center2 = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        lua_getfield(L, index, "radius");
        capsule.radius = luaL_checknumber(L, -1) * GetPhysicsScale();
        lua_pop(L, 1);

        return capsule;
    }

    static b2ShapeProxy MakeShapeProxy(const B2DShapeDef& shape_def)
    {
        b2ShapeProxy proxy = {};

        switch (shape_def.m_Type)
        {
            case B2DShapeDef::TYPE_CIRCLE:
                proxy.points[0] = shape_def.m_Circle.center;
                proxy.count = 1;
                proxy.radius = shape_def.m_Circle.radius;
                break;
            case B2DShapeDef::TYPE_CAPSULE:
                proxy.points[0] = shape_def.m_Capsule.center1;
                proxy.points[1] = shape_def.m_Capsule.center2;
                proxy.count = 2;
                proxy.radius = shape_def.m_Capsule.radius;
                break;
            case B2DShapeDef::TYPE_SEGMENT:
                proxy.points[0] = shape_def.m_Segment.point1;
                proxy.points[1] = shape_def.m_Segment.point2;
                proxy.count = 2;
                proxy.radius = 0.0f;
                break;
            case B2DShapeDef::TYPE_POLYGON:
                proxy.count = shape_def.m_Polygon.count;
                proxy.radius = shape_def.m_Polygon.radius;
                for (int i = 0; i < shape_def.m_Polygon.count; ++i)
                {
                    proxy.points[i] = shape_def.m_Polygon.vertices[i];
                }
                break;
        }

        return proxy;
    }

    static void PushTreeStats(lua_State* L, const b2TreeStats& stats)
    {
        lua_newtable(L);

        lua_pushinteger(L, stats.nodeVisits);
        lua_setfield(L, -2, "node_visits");

        lua_pushinteger(L, stats.leafVisits);
        lua_setfield(L, -2, "leaf_visits");
    }

    static void PushShapeInfoForShape(lua_State* L, b2ShapeId shape)
    {
        b2BodyId body = b2Shape_GetBody(shape);
        PushShapeInfo(L, shape, GetShapeIndex(body, shape));
    }

    static void PushCastHit(lua_State* L, b2ShapeId shape, b2Vec2 point, b2Vec2 normal, float fraction)
    {
        lua_newtable(L);

        PushShapeInfoForShape(L, shape);
        lua_setfield(L, -2, "shape");

        dmScript::PushVector3(L, FromB2(point, GetInvPhysicsScale()));
        lua_setfield(L, -2, "point");

        dmScript::PushVector3(L, FromB2(normal, 1.0f));
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, fraction);
        lua_setfield(L, -2, "fraction");
    }

    static bool OverlapResultCallback(b2ShapeId shape_id, void* user_context)
    {
        QueryContext* context = (QueryContext*)user_context;
        if (!HasResultCapacity(context))
        {
            return false;
        }

        PushShapeInfoForShape(context->m_L, shape_id);
        lua_rawseti(context->m_L, context->m_TableIndex, ++context->m_Count);
        return HasResultCapacity(context);
    }

    static float CastResultCallback(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* user_context)
    {
        QueryContext* context = (QueryContext*)user_context;
        if (!HasResultCapacity(context))
        {
            return 0.0f;
        }

        PushCastHit(context->m_L, shape_id, point, normal, fraction);
        lua_rawseti(context->m_L, context->m_TableIndex, ++context->m_Count);
        return HasResultCapacity(context) ? 1.0f : 0.0f;
    }

    static bool PlaneResultCallback(b2ShapeId shape_id, const b2PlaneResult* plane, void* user_context)
    {
        QueryContext* context = (QueryContext*)user_context;
        if (!HasResultCapacity(context))
        {
            return false;
        }

        lua_State* L = context->m_L;
        lua_newtable(L);

        PushShapeInfoForShape(L, shape_id);
        lua_setfield(L, -2, "shape");

        dmScript::PushVector3(L, FromB2(plane->plane.normal, 1.0f));
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, plane->plane.offset * GetInvPhysicsScale());
        lua_setfield(L, -2, "offset");

        lua_pushboolean(L, plane->hit);
        lua_setfield(L, -2, "hit");

        lua_rawseti(L, context->m_TableIndex, ++context->m_Count);
        return HasResultCapacity(context);
    }

    static int World_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaWorld* luaworld = CheckWorldInternal(L, 1);
        lua_pushboolean(L, b2World_IsValid(luaworld->m_World));
        return 1;
    }

    static int World_IsLocked(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2World_IsLocked(*CheckWorld(L, 1)));
        return 1;
    }

    static int World_GetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmScript::PushVector3(L, FromB2(b2World_GetGravity(*CheckWorld(L, 1)), GetInvPhysicsScale()));
        return 1;
    }

    static int World_SetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_gravity");
        b2World_SetGravity(world, CheckVec2(L, 2, GetPhysicsScale()));
        return 0;
    }

    static int World_EnableSleeping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "enable_sleeping");
        b2World_EnableSleeping(world, lua_toboolean(L, 2));
        return 0;
    }

    static int World_IsSleepingEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2World_IsSleepingEnabled(*CheckWorld(L, 1)));
        return 1;
    }

    static int World_EnableContinuous(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "enable_continuous");
        b2World_EnableContinuous(world, lua_toboolean(L, 2));
        return 0;
    }

    static int World_IsContinuousEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2World_IsContinuousEnabled(*CheckWorld(L, 1)));
        return 1;
    }

    static int World_SetRestitutionThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_restitution_threshold");
        b2World_SetRestitutionThreshold(world, luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int World_GetRestitutionThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2World_GetRestitutionThreshold(*CheckWorld(L, 1)) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int World_SetHitEventThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_hit_event_threshold");
        b2World_SetHitEventThreshold(world, luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int World_GetHitEventThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2World_GetHitEventThreshold(*CheckWorld(L, 1)) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int World_SetMaximumLinearSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_maximum_linear_speed");
        b2World_SetMaximumLinearSpeed(world, luaL_checknumber(L, 2) * GetPhysicsScale());
        return 0;
    }

    static int World_GetMaximumLinearSpeed(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2World_GetMaximumLinearSpeed(*CheckWorld(L, 1)) * (lua_Number)GetInvPhysicsScale());
        return 1;
    }

    static int World_EnableWarmStarting(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "enable_warm_starting");
        b2World_EnableWarmStarting(world, lua_toboolean(L, 2));
        return 0;
    }

    static int World_IsWarmStartingEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2World_IsWarmStartingEnabled(*CheckWorld(L, 1)));
        return 1;
    }

    static int World_GetAwakeBodyCount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2World_GetAwakeBodyCount(*CheckWorld(L, 1)));
        return 1;
    }

    static int World_SetContactTuning(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_contact_tuning");
        b2World_SetContactTuning(world, luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4) * GetPhysicsScale());
        return 0;
    }

    static int World_SetJointTuning(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "set_joint_tuning");
        b2World_SetJointTuning(world, luaL_checknumber(L, 2), luaL_checknumber(L, 3));
        return 0;
    }

    static int World_GetProfile(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Profile profile = b2World_GetProfile(*CheckWorld(L, 1));

        lua_newtable(L);

#define SET_PROFILE_FIELD(NAME, FIELD_NAME) \
        lua_pushnumber(L, profile.NAME); \
        lua_setfield(L, -2, FIELD_NAME);

        SET_PROFILE_FIELD(step, "step");
        SET_PROFILE_FIELD(pairs, "pairs");
        SET_PROFILE_FIELD(collide, "collide");
        SET_PROFILE_FIELD(solve, "solve");
        SET_PROFILE_FIELD(mergeIslands, "merge_islands");
        SET_PROFILE_FIELD(prepareStages, "prepare_stages");
        SET_PROFILE_FIELD(solveConstraints, "solve_constraints");
        SET_PROFILE_FIELD(prepareConstraints, "prepare_constraints");
        SET_PROFILE_FIELD(integrateVelocities, "integrate_velocities");
        SET_PROFILE_FIELD(warmStart, "warm_start");
        SET_PROFILE_FIELD(solveImpulses, "solve_impulses");
        SET_PROFILE_FIELD(integratePositions, "integrate_positions");
        SET_PROFILE_FIELD(relaxImpulses, "relax_impulses");
        SET_PROFILE_FIELD(applyRestitution, "apply_restitution");
        SET_PROFILE_FIELD(storeImpulses, "store_impulses");
        SET_PROFILE_FIELD(splitIslands, "split_islands");
        SET_PROFILE_FIELD(transforms, "transforms");
        SET_PROFILE_FIELD(hitEvents, "hit_events");
        SET_PROFILE_FIELD(refit, "refit");
        SET_PROFILE_FIELD(bullets, "bullets");
        SET_PROFILE_FIELD(sleepIslands, "sleep_islands");
        SET_PROFILE_FIELD(sensors, "sensors");

#undef SET_PROFILE_FIELD

        return 1;
    }

    static int World_GetCounters(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Counters counters = b2World_GetCounters(*CheckWorld(L, 1));

        lua_newtable(L);

#define SET_COUNTER_FIELD(NAME, FIELD_NAME) \
        lua_pushinteger(L, counters.NAME); \
        lua_setfield(L, -2, FIELD_NAME);

        SET_COUNTER_FIELD(bodyCount, "body_count");
        SET_COUNTER_FIELD(shapeCount, "shape_count");
        SET_COUNTER_FIELD(contactCount, "contact_count");
        SET_COUNTER_FIELD(jointCount, "joint_count");
        SET_COUNTER_FIELD(islandCount, "island_count");
        SET_COUNTER_FIELD(stackUsed, "stack_used");
        SET_COUNTER_FIELD(staticTreeHeight, "static_tree_height");
        SET_COUNTER_FIELD(treeHeight, "tree_height");
        SET_COUNTER_FIELD(byteCount, "byte_count");
        SET_COUNTER_FIELD(taskCount, "task_count");

#undef SET_COUNTER_FIELD

        lua_newtable(L);
        for (int i = 0; i < 12; ++i)
        {
            lua_pushinteger(L, counters.colorCounts[i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "color_counts");

        return 1;
    }

    static int World_Explode(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "explode");
        luaL_checktype(L, 2, LUA_TTABLE);

        b2ExplosionDef def = b2DefaultExplosionDef();

        lua_getfield(L, 2, "position");
        def.position = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);

        lua_getfield(L, 2, "radius");
        def.radius = luaL_checknumber(L, -1) * GetPhysicsScale();
        lua_pop(L, 1);

        lua_getfield(L, 2, "falloff");
        def.falloff = luaL_checknumber(L, -1) * GetPhysicsScale();
        lua_pop(L, 1);

        lua_getfield(L, 2, "impulse_per_length");
        def.impulsePerLength = luaL_checknumber(L, -1) * GetPhysicsScale();
        lua_pop(L, 1);

        lua_getfield(L, 2, "mask_bits");
        if (!lua_isnil(L, -1))
        {
            def.maskBits = (uint64_t)luaL_checknumber(L, -1);
        }
        lua_pop(L, 1);

        b2World_Explode(world, &def);
        return 0;
    }

    static int World_RebuildStaticTree(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "rebuild_static_tree");
        b2World_RebuildStaticTree(world);
        return 0;
    }

    static int World_EnableSpeculative(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "enable_speculative");
        b2World_EnableSpeculative(world, lua_toboolean(L, 2));
        return 0;
    }

    static int World_OverlapAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "overlap_aabb");
        b2AABB aabb = CheckAABB(L, 2);
        b2QueryFilter filter = CheckQueryFilter(L, 3);

        lua_newtable(L);
        QueryContext context = { L, AbsIndex(L, -1), 0, CheckMaxResults(L, 4) };
        b2TreeStats stats = b2World_OverlapAABB(world, aabb, filter, OverlapResultCallback, &context);

        PushTreeStats(L, stats);
        return 2;
    }

    static int World_OverlapShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "overlap_shape");
        B2DShapeDef shape_def = CheckShapeDef(L, 2);
        b2ShapeProxy proxy = MakeShapeProxy(shape_def);
        b2QueryFilter filter = CheckQueryFilter(L, 3);

        lua_newtable(L);
        QueryContext context = { L, AbsIndex(L, -1), 0, CheckMaxResults(L, 4) };
        b2TreeStats stats = b2World_OverlapShape(world, &proxy, filter, OverlapResultCallback, &context);

        PushTreeStats(L, stats);
        return 2;
    }

    static int World_CastRay(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "cast_ray");
        b2Vec2 origin = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        b2QueryFilter filter = CheckQueryFilter(L, 4);

        lua_newtable(L);
        QueryContext context = { L, AbsIndex(L, -1), 0, CheckMaxResults(L, 5) };
        b2TreeStats stats = b2World_CastRay(world, origin, translation, filter, CastResultCallback, &context);

        PushTreeStats(L, stats);
        return 2;
    }

    static int World_CastRayClosest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "cast_ray_closest");
        b2Vec2 origin = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        b2QueryFilter filter = CheckQueryFilter(L, 4);

        b2RayResult result = b2World_CastRayClosest(world, origin, translation, filter);
        if (!result.hit)
        {
            lua_pushnil(L);
            return 1;
        }

        PushCastHit(L, result.shapeId, result.point, result.normal, result.fraction);
        lua_pushinteger(L, result.nodeVisits);
        lua_setfield(L, -2, "node_visits");
        lua_pushinteger(L, result.leafVisits);
        lua_setfield(L, -2, "leaf_visits");
        return 1;
    }

    static int World_CastShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "cast_shape");
        B2DShapeDef shape_def = CheckShapeDef(L, 2);
        b2ShapeProxy proxy = MakeShapeProxy(shape_def);
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        b2QueryFilter filter = CheckQueryFilter(L, 4);

        lua_newtable(L);
        QueryContext context = { L, AbsIndex(L, -1), 0, CheckMaxResults(L, 5) };
        b2TreeStats stats = b2World_CastShape(world, &proxy, translation, filter, CastResultCallback, &context);

        PushTreeStats(L, stats);
        return 2;
    }

    static int World_CastMover(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "cast_mover");
        b2Capsule capsule = CheckCapsule(L, 2);
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        b2QueryFilter filter = CheckQueryFilter(L, 4);

        lua_pushnumber(L, b2World_CastMover(world, &capsule, translation, filter));
        return 1;
    }

    static int World_CollideMover(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2WorldId world = *CheckWorld(L, 1);
        CheckWorldUnlocked(L, world, "collide_mover");
        b2Capsule capsule = CheckCapsule(L, 2);
        b2QueryFilter filter = CheckQueryFilter(L, 3);

        lua_newtable(L);
        QueryContext context = { L, AbsIndex(L, -1), 0, CheckMaxResults(L, 4) };
        b2World_CollideMover(world, &capsule, filter, PlaneResultCallback, &context);
        return 1;
    }

    static int World_tostring(lua_State *L)
    {
        B2DLuaWorld* luaworld = CheckWorldInternal(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_WORLD, &luaworld->m_World);
        return 1;
    }

    static int World_eq(lua_State *L)
    {
        b2WorldId* a = ToWorld(L, 1);
        b2WorldId* b = ToWorld(L, 2);
        lua_pushboolean(L, a && b && memcmp(a, b, sizeof(b2WorldId)) == 0);
        return 1;
    }

    static const luaL_reg World_methods[] =
    {
        {0,0}
    };

    static const luaL_reg World_meta[] =
    {
        {"__tostring", World_tostring},
        {"__eq", World_eq},
        {0,0}
    };

    static const luaL_reg World_functions[] =
    {
        {"is_valid", World_IsValid},
        {"is_locked", World_IsLocked},
        {"get_gravity", World_GetGravity},
        {"set_gravity", World_SetGravity},
        {"enable_sleeping", World_EnableSleeping},
        {"is_sleeping_enabled", World_IsSleepingEnabled},
        {"enable_continuous", World_EnableContinuous},
        {"is_continuous_enabled", World_IsContinuousEnabled},
        {"set_restitution_threshold", World_SetRestitutionThreshold},
        {"get_restitution_threshold", World_GetRestitutionThreshold},
        {"set_hit_event_threshold", World_SetHitEventThreshold},
        {"get_hit_event_threshold", World_GetHitEventThreshold},
        {"set_maximum_linear_speed", World_SetMaximumLinearSpeed},
        {"get_maximum_linear_speed", World_GetMaximumLinearSpeed},
        {"enable_warm_starting", World_EnableWarmStarting},
        {"is_warm_starting_enabled", World_IsWarmStartingEnabled},
        {"get_awake_body_count", World_GetAwakeBodyCount},
        {"set_contact_tuning", World_SetContactTuning},
        {"set_joint_tuning", World_SetJointTuning},
        {"get_profile", World_GetProfile},
        {"get_counters", World_GetCounters},
        {"explode", World_Explode},
        {"rebuild_static_tree", World_RebuildStaticTree},
        {"enable_speculative", World_EnableSpeculative},
        {"overlap_aabb", World_OverlapAABB},
        {"overlap_shape", World_OverlapShape},
        {"cast_ray", World_CastRay},
        {"cast_ray_closest", World_CastRayClosest},
        {"cast_shape", World_CastShape},
        {"cast_mover", World_CastMover},
        {"collide_mover", World_CollideMover},
        {0,0}
    };

    void ScriptBox2DInitializeWorld(lua_State* L)
    {
        TYPE_HASH_WORLD = dmScript::RegisterUserType(L, BOX2D_TYPE_NAME_WORLD, World_methods, World_meta);

        lua_newtable(L);
        luaL_register(L, 0, World_functions);
        lua_setfield(L, -2, "world");
    }

    void ScriptBox2DFinalizeWorld()
    {
        TYPE_HASH_WORLD = 0;
    }
}

/*# Box2D b2World documentation
 *
 * Functions for querying and tuning the Box2D v3 world owned by the current
 * Defold collection. World creation, destruction, stepping, debug draw,
 * callbacks, and user data remain owned by Defold.
 *
 * Query filters are optional tables with `category_bits` and `mask_bits`.
 * When omitted, Box2D default query filtering is used.
 *
 * Shape result tables include `shape_id`, `index`, `type`, `sensor`,
 * `density`, `friction`, `restitution`, `material`, `child_count`, and
 * `is_chain_segment`.
 * Chain segment shapes report `type = b2d.shape.SHAPE_TYPE_SEGMENT`.
 *
 * Tree stats tables include `node_visits` and `leaf_visits`.
 *
 * Cast hit tables include `shape`, `point`, `normal`, and `fraction`.
 *
 * @document
 * @name b2d.world
 * @namespace b2d.world
 * @language Lua
 * @version 3
 */

/*# Check whether a world handle is valid.
 * @name b2d.world.is_valid
 * @param world [type: b2World] world
 * @return valid [type: boolean] true if the world handle is valid
 */

/*# Check whether the world is locked.
 * The world is locked during callbacks and some simulation phases. Functions
 * marked as locked during callbacks cannot be called while this returns true.
 * @name b2d.world.is_locked
 * @param world [type: b2World] world
 * @return locked [type: boolean] true if the world is locked
 */

/*# Get world gravity.
 * @name b2d.world.get_gravity
 * @param world [type: b2World] world
 * @return gravity [type: vector3] gravity vector
 */

/*# Set world gravity.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_gravity
 * @param world [type: b2World] world
 * @param gravity [type: vector3] gravity vector
 */

/*# Enable or disable world sleeping.
 * @warning This function is locked during callbacks.
 * @name b2d.world.enable_sleeping
 * @param world [type: b2World] world
 * @param enable [type: boolean] true to allow sleeping
 */

/*# Get whether world sleeping is enabled.
 * @name b2d.world.is_sleeping_enabled
 * @param world [type: b2World] world
 * @return enabled [type: boolean] true if sleeping is enabled
 */

/*# Enable or disable continuous collision.
 * @warning This function is locked during callbacks.
 * @name b2d.world.enable_continuous
 * @param world [type: b2World] world
 * @param enable [type: boolean] true to enable continuous collision
 */

/*# Get whether continuous collision is enabled.
 * @name b2d.world.is_continuous_enabled
 * @param world [type: b2World] world
 * @return enabled [type: boolean] true if continuous collision is enabled
 */

/*# Set the restitution threshold.
 * Collisions below this relative speed use inelastic collision response.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_restitution_threshold
 * @param world [type: b2World] world
 * @param threshold [type: number] restitution threshold in project units per second
 */

/*# Get the restitution threshold.
 * @name b2d.world.get_restitution_threshold
 * @param world [type: b2World] world
 * @return threshold [type: number] restitution threshold in project units per second
 */

/*# Set the hit event threshold.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_hit_event_threshold
 * @param world [type: b2World] world
 * @param threshold [type: number] hit event threshold in project units per second
 */

/*# Get the hit event threshold.
 * @name b2d.world.get_hit_event_threshold
 * @param world [type: b2World] world
 * @return threshold [type: number] hit event threshold in project units per second
 */

/*# Set the maximum linear speed.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_maximum_linear_speed
 * @param world [type: b2World] world
 * @param speed [type: number] maximum linear speed in project units per second
 */

/*# Get the maximum linear speed.
 * @name b2d.world.get_maximum_linear_speed
 * @param world [type: b2World] world
 * @return speed [type: number] maximum linear speed in project units per second
 */

/*# Enable or disable warm starting.
 * @warning This function is locked during callbacks.
 * @name b2d.world.enable_warm_starting
 * @param world [type: b2World] world
 * @param enable [type: boolean] true to enable warm starting
 */

/*# Get whether warm starting is enabled.
 * @name b2d.world.is_warm_starting_enabled
 * @param world [type: b2World] world
 * @return enabled [type: boolean] true if warm starting is enabled
 */

/*# Get the number of awake bodies.
 * @name b2d.world.get_awake_body_count
 * @param world [type: b2World] world
 * @return count [type: integer] awake body count
 */

/*# Set contact solver tuning.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_contact_tuning
 * @param world [type: b2World] world
 * @param hertz [type: number] contact stiffness frequency in hertz
 * @param damping_ratio [type: number] contact damping ratio
 * @param pushout [type: number] pushout velocity in project units per second
 */

/*# Set joint solver tuning.
 * @warning This function is locked during callbacks.
 * @name b2d.world.set_joint_tuning
 * @param world [type: b2World] world
 * @param hertz [type: number] joint stiffness frequency in hertz
 * @param damping_ratio [type: number] joint damping ratio
 */

/*# Get world profiling data.
 * The returned table contains Box2D timing fields including `step`, `pairs`,
 * `collide`, `solve`, `merge_islands`, `prepare_stages`, `solve_constraints`,
 * `prepare_constraints`, `integrate_velocities`, `warm_start`,
 * `solve_impulses`, `integrate_positions`, `relax_impulses`,
 * `apply_restitution`, `store_impulses`, `split_islands`, `transforms`,
 * `hit_events`, `refit`, `bullets`, `sleep_islands`, and `sensors`.
 * @name b2d.world.get_profile
 * @param world [type: b2World] world
 * @return profile [type:b2d.world_profile] world profiling data
 */

/*# Get world counters.
 * The returned table contains `body_count`, `shape_count`, `contact_count`,
 * `joint_count`, `island_count`, `stack_used`, `static_tree_height`,
 * `tree_height`, `byte_count`, `task_count`, and `color_counts`.
 * @name b2d.world.get_counters
 * @param world [type: b2World] world
 * @return counters [type:b2d.world_counters] world counters
 */

/*# Apply an explosion impulse.
 * The definition table requires `position`, `radius`, `falloff`, and
 * `impulse_per_length`. It may also include `mask_bits`.
 * @warning This function is locked during callbacks.
 * @name b2d.world.explode
 * @param world [type: b2World] world
 * @param definition [type:b2d.explosion_definition] explosion definition
 */

/*# Rebuild the static broad-phase tree.
 * @warning This function is locked during callbacks.
 * @name b2d.world.rebuild_static_tree
 * @param world [type: b2World] world
 */

/*# Enable or disable speculative collision.
 * @warning This function is locked during callbacks.
 * @name b2d.world.enable_speculative
 * @param world [type: b2World] world
 * @param enable [type: boolean] true to enable speculative collision
 */

/*# Find shapes overlapping an AABB.
 * The AABB table has `lower` and `upper` `vector3` fields.
 * @warning This function is locked during callbacks.
 * @name b2d.world.overlap_aabb
 * @param world [type: b2World] world
 * @param aabb [type:b2d.aabb] AABB table with `lower` and `upper`
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @param [max_results] [type:integer] optional maximum result count. Omit or pass 0 for unlimited results.
 * @return hits [type:b2d.shape_info[]] array of shape info tables
 * @return stats [type:b2d.tree_stats] tree stats table
 */

/*# Find shapes overlapping a shape proxy.
 * The shape table uses the same circle, capsule, segment, polygon, and box formats
 * as `b2d.body.create_shape`.
 * @warning This function is locked during callbacks.
 * @name b2d.world.overlap_shape
 * @param world [type: b2World] world
 * @param shape [type:b2d.shape.definition] shape table
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @param [max_results] [type:integer] optional maximum result count. Omit or pass 0 for unlimited results.
 * @return hits [type:b2d.shape_info[]] array of shape info tables
 * @return stats [type:b2d.tree_stats] tree stats table
 */

/*# Cast a ray and collect hits.
 * The translation is the ray displacement from `origin`. Result order is not
 * guaranteed by Box2D.
 * @warning This function is locked during callbacks.
 * @name b2d.world.cast_ray
 * @param world [type: b2World] world
 * @param origin [type: vector3] ray start position
 * @param translation [type: vector3] ray displacement
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @param [max_results] [type:integer] optional maximum result count. Omit or pass 0 for unlimited results.
 * @return hits [type:b2d.shape_cast_hit[]] array of cast hit tables
 * @return stats [type:b2d.tree_stats] tree stats table
 */

/*# Cast a ray and return the closest hit.
 * The translation is the ray displacement from `origin`.
 * @warning This function is locked during callbacks.
 * @name b2d.world.cast_ray_closest
 * @param world [type: b2World] world
 * @param origin [type: vector3] ray start position
 * @param translation [type: vector3] ray displacement
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @return hit [type:b2d.shape_cast_hit|nil] closest cast hit table with `node_visits` and `leaf_visits`, or `nil` on miss
 */

/*# Cast a shape and collect hits.
 * The shape table uses the same circle, capsule, segment, polygon, and box formats
 * as `b2d.body.create_shape`. The translation is the shape displacement.
 * @warning This function is locked during callbacks.
 * @name b2d.world.cast_shape
 * @param world [type: b2World] world
 * @param shape [type:b2d.shape.definition] shape table
 * @param translation [type: vector3] shape displacement
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @param [max_results] [type:integer] optional maximum result count. Omit or pass 0 for unlimited results.
 * @return hits [type:b2d.shape_cast_hit[]] array of cast hit tables
 * @return stats [type:b2d.tree_stats] tree stats table
 */

/*# Cast a mover capsule.
 * The capsule table has `center1`, `center2`, and `radius` fields. The return
 * value is the fraction of `translation` that can be traveled before collision,
 * or 1 if there is no hit.
 * @warning This function is locked during callbacks.
 * @name b2d.world.cast_mover
 * @param world [type: b2World] world
 * @param capsule [type:b2d.mover_capsule] capsule table with `center1`, `center2`, and `radius`
 * @param translation [type: vector3] capsule displacement
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @return fraction [type: number] travel fraction before collision
 */

/*# Collide a mover capsule against the world.
 * The capsule table has `center1`, `center2`, and `radius` fields. Plane result
 * tables include `shape`, `normal`, `offset`, and `hit`.
 * @warning This function is locked during callbacks.
 * @name b2d.world.collide_mover
 * @param world [type: b2World] world
 * @param capsule [type:b2d.mover_capsule] capsule table with `center1`, `center2`, and `radius`
 * @param [filter] [type:b2d.query_filter] optional query filter with `category_bits` and `mask_bits`
 * @param [max_results] [type:integer] optional maximum result count. Omit or pass 0 for unlimited results.
 * @return planes [type:b2d.mover_plane[]] array of plane result tables
 */
