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

#include <stdio.h>

#include <Box2D/Collision/b2Distance.h>
#include <Box2D/Collision/b2TimeOfImpact.h>
#include <Box2D/Dynamics/b2ContactManager.h>
#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/b2World.h>

#include <dlib/array.h>
#include <script/script.h>
#include <gameobject/script.h>

#include "components/comp_collision_object.h"
#include "script_box2d_v2.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    struct QueryFilter
    {
        uint16 m_CategoryBits;
        uint16 m_MaskBits;
        int16  m_GroupIndex;
        bool   m_HasGroupIndex;
    };

    struct QueryStats
    {
        int m_NodeVisits;
        int m_LeafVisits;
    };

    struct FixtureChild
    {
        b2Fixture* m_Fixture;
        int32      m_ChildIndex;
    };

    struct QueryContext
    {
        lua_State*            m_L;
        int                   m_TableIndex;
        int                   m_Count;
        int                   m_MaxResults;
        QueryFilter           m_Filter;
        QueryStats            m_Stats;
        dmArray<FixtureChild> m_Seen;
    };

    static b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        return b2Vec2(v->getX() * scale, v->getY() * scale);
    }

    static dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale)
    {
        return dmVMath::Vector3(p.x * inv_scale, p.y * inv_scale, 0);
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static b2World* CheckWorld(lua_State* L, int index)
    {
        b2World* world = lua_islightuserdata(L, index) ? (b2World*)lua_touserdata(L, index) : 0;
        if (!world)
        {
            luaL_error(L, "Expected b2World.");
            return 0;
        }
        return world;
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

    static bool HasResultCapacity(const QueryContext* context)
    {
        return context->m_MaxResults <= 0 || context->m_Count < context->m_MaxResults;
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

        if (!aabb.IsValid())
        {
            luaL_error(L, "Invalid AABB.");
        }

        return aabb;
    }

    static QueryFilter CheckQueryFilter(lua_State* L, int index)
    {
        QueryFilter filter = {};
        filter.m_CategoryBits = 0xffff;
        filter.m_MaskBits = 0xffff;
        filter.m_GroupIndex = 0;
        filter.m_HasGroupIndex = false;

        if (lua_isnoneornil(L, index))
        {
            return filter;
        }

        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "category_bits");
        if (!lua_isnil(L, -1))
        {
            filter.m_CategoryBits = (uint16)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "mask_bits");
        if (!lua_isnil(L, -1))
        {
            filter.m_MaskBits = (uint16)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "group_index");
        if (!lua_isnil(L, -1))
        {
            filter.m_GroupIndex = (int16)luaL_checkinteger(L, -1);
            filter.m_HasGroupIndex = true;
        }
        lua_pop(L, 1);

        return filter;
    }

    static bool MatchesFilter(b2Fixture* fixture, int32 child_index, const QueryFilter& query_filter)
    {
        const b2Filter& fixture_filter = fixture->GetFilterData(child_index);
        if (query_filter.m_HasGroupIndex && fixture_filter.groupIndex != query_filter.m_GroupIndex)
        {
            return false;
        }

        return (fixture_filter.categoryBits & query_filter.m_MaskBits) != 0 && (query_filter.m_CategoryBits & fixture_filter.maskBits) != 0;
    }

    static int GetFixtureIndex(b2Body* body, const b2Fixture* fixture)
    {
        int fixture_index = 1;
        for (b2Fixture* current = body->GetFixtureList(); current; current = current->GetNext(), ++fixture_index)
        {
            if (current == fixture)
            {
                return fixture_index;
            }
        }
        return 0;
    }

    static void PushBodyForBody(lua_State* L, b2Body* body)
    {
        dmGameObject::HCollection collection = 0;
        dmhash_t instance_id = 0;

        void* user_data = body->GetUserData();
        dmGameObject::HInstance instance = user_data ? CompCollisionObjectGetInstance(user_data) : 0;
        if (instance)
        {
            collection = dmGameObject::GetCollection(instance);
            instance_id = dmGameObject::GetIdentifier(instance);
        }

        PushBody(L, body, collection, instance_id);
    }

    static void PushFixtureInfo(lua_State* L, b2Fixture* fixture, int32 child_index)
    {
        b2Body* body = fixture->GetBody();

        lua_newtable(L);

        PushBodyForBody(L, body);
        lua_setfield(L, -2, "body");

        lua_pushinteger(L, GetFixtureIndex(body, fixture));
        lua_setfield(L, -2, "index");

        lua_pushinteger(L, child_index + 1);
        lua_setfield(L, -2, "child_index");

        lua_pushinteger(L, fixture->GetType());
        lua_setfield(L, -2, "type");

        lua_pushboolean(L, fixture->IsSensor());
        lua_setfield(L, -2, "sensor");

        lua_pushnumber(L, fixture->GetDensity());
        lua_setfield(L, -2, "density");

        lua_pushnumber(L, fixture->GetFriction());
        lua_setfield(L, -2, "friction");

        lua_pushnumber(L, fixture->GetRestitution());
        lua_setfield(L, -2, "restitution");

        lua_pushinteger(L, fixture->GetShape()->GetChildCount());
        lua_setfield(L, -2, "child_count");
    }

    static bool HasSeenFixtureChild(const QueryContext* context, b2Fixture* fixture, int32 child_index)
    {
        for (uint32_t i = 0; i < context->m_Seen.Size(); ++i)
        {
            if (context->m_Seen[i].m_Fixture == fixture && context->m_Seen[i].m_ChildIndex == child_index)
            {
                return true;
            }
        }
        return false;
    }

    static void MarkSeenFixtureChild(QueryContext* context, b2Fixture* fixture, int32 child_index)
    {
        if (context->m_Seen.Remaining() == 0)
        {
            if (context->m_Seen.Capacity() == 0)
            {
                context->m_Seen.SetCapacity(16);
            }
            else
            {
                context->m_Seen.OffsetCapacity(16);
            }
        }

        uint32_t index = context->m_Seen.Size();
        context->m_Seen.SetSize(index + 1);
        FixtureChild& entry = context->m_Seen[index];
        entry.m_Fixture = fixture;
        entry.m_ChildIndex = child_index;
    }

    static bool PushFixtureResult(QueryContext* context, b2Fixture* fixture, int32 child_index)
    {
        if (!HasResultCapacity(context) || HasSeenFixtureChild(context, fixture, child_index))
        {
            return HasResultCapacity(context);
        }

        MarkSeenFixtureChild(context, fixture, child_index);
        PushFixtureInfo(context->m_L, fixture, child_index);
        lua_rawseti(context->m_L, context->m_TableIndex, ++context->m_Count);
        return HasResultCapacity(context);
    }

    static void PushCastHit(lua_State* L, b2Fixture* fixture, int32 child_index, const b2Vec2& point, const b2Vec2& normal, float fraction)
    {
        lua_newtable(L);

        PushFixtureInfo(L, fixture, child_index);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "shape");
        lua_setfield(L, -2, "fixture");

        dmScript::PushVector3(L, FromB2(point, GetInvPhysicsScale()));
        lua_setfield(L, -2, "point");

        dmScript::PushVector3(L, FromB2(normal, 1.0f));
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, fraction);
        lua_setfield(L, -2, "fraction");
    }

    static void PushTreeStats(lua_State* L, const QueryStats& stats)
    {
        lua_newtable(L);

        lua_pushinteger(L, stats.m_NodeVisits);
        lua_setfield(L, -2, "node_visits");

        lua_pushinteger(L, stats.m_LeafVisits);
        lua_setfield(L, -2, "leaf_visits");
    }

    static b2AABB ComputeShapeAABB(const b2Shape* shape, const b2Transform& transform)
    {
        b2AABB result;
        bool has_aabb = false;
        const int child_count = shape->GetChildCount();
        for (int i = 0; i < child_count; ++i)
        {
            b2AABB child_aabb;
            shape->ComputeAABB(&child_aabb, transform, i);
            if (has_aabb)
            {
                result.Combine(child_aabb);
            }
            else
            {
                result = child_aabb;
                has_aabb = true;
            }
        }
        return result;
    }

    static bool SupportsDistanceProxy(const b2Shape* shape)
    {
        return shape->GetType() != b2Shape::e_grid;
    }

    static b2Sweep MakeSweep(const b2Vec2& local_center, const b2Vec2& start_center, const b2Vec2& end_center, float start_angle, float end_angle)
    {
        b2Sweep sweep;
        sweep.localCenter = local_center;
        sweep.c0 = start_center;
        sweep.c = end_center;
        sweep.a0 = start_angle;
        sweep.a = end_angle;
        sweep.alpha0 = 0.0f;
        return sweep;
    }

    static bool ComputeShapeCastHit(const b2Shape* query_shape, int32 query_child_index, b2Fixture* fixture, int32 fixture_child_index, const b2Vec2& translation, float* out_fraction, b2Vec2* out_point, b2Vec2* out_normal)
    {
        if (!SupportsDistanceProxy(query_shape) || !SupportsDistanceProxy(fixture->GetShape()))
        {
            return false;
        }

        b2DistanceProxy proxy_a;
        b2DistanceProxy proxy_b;
        proxy_a.Set(query_shape, query_child_index);
        proxy_b.Set(fixture->GetShape(), fixture_child_index);

        b2Body* body = fixture->GetBody();
        b2Sweep sweep_a = MakeSweep(b2Vec2_zero, b2Vec2_zero, translation, 0.0f, 0.0f);
        b2Sweep sweep_b = MakeSweep(body->GetLocalCenter(), body->GetWorldCenter(), body->GetWorldCenter(), body->GetAngle(), body->GetAngle());

        b2TOIInput toi_input;
        toi_input.proxyA = proxy_a;
        toi_input.proxyB = proxy_b;
        toi_input.sweepA = sweep_a;
        toi_input.sweepB = sweep_b;
        toi_input.tMax = 1.0f;

        b2TOIOutput toi_output;
        b2TimeOfImpact(&toi_output, &toi_input);
        if (toi_output.state != b2TOIOutput::e_touching && toi_output.state != b2TOIOutput::e_overlapped)
        {
            return false;
        }

        b2Transform xf_a;
        b2Transform xf_b;
        sweep_a.GetTransform(&xf_a, toi_output.t);
        sweep_b.GetTransform(&xf_b, toi_output.t);

        b2SimplexCache cache;
        cache.count = 0;

        b2DistanceInput distance_input;
        distance_input.proxyA = proxy_a;
        distance_input.proxyB = proxy_b;
        distance_input.transformA = xf_a;
        distance_input.transformB = xf_b;
        distance_input.useRadii = true;

        b2DistanceOutput distance_output;
        b2Distance(&distance_output, &cache, &distance_input);

        cache.count = 0;
        b2DistanceInput normal_input = distance_input;
        normal_input.useRadii = false;
        b2DistanceOutput normal_output;
        b2Distance(&normal_output, &cache, &normal_input);

        b2Vec2 normal = normal_output.pointA - normal_output.pointB;
        float length = normal.Length();
        if (length > b2_epsilon)
        {
            normal *= 1.0f / length;
        }
        else if (translation.LengthSquared() > b2_epsilon * b2_epsilon)
        {
            normal.Set(-translation.x, -translation.y);
            normal.Normalize();
        }
        else
        {
            normal.Set(1.0f, 0.0f);
        }

        *out_fraction = toi_output.t;
        *out_point = distance_output.pointB;
        *out_normal = normal;
        return true;
    }

    class OverlapAABBCallback
    {
    public:
        OverlapAABBCallback(const b2BroadPhase* broad_phase, QueryContext* context)
        : m_BroadPhase(broad_phase)
        , m_Context(context)
        {
        }

        bool QueryCallback(int32 proxy_id)
        {
            ++m_Context->m_Stats.m_LeafVisits;

            b2FixtureProxy* proxy = (b2FixtureProxy*)m_BroadPhase->GetUserData(proxy_id);
            if (!proxy || !MatchesFilter(proxy->fixture, proxy->childIndex, m_Context->m_Filter))
            {
                return true;
            }

            return PushFixtureResult(m_Context, proxy->fixture, proxy->childIndex);
        }

    private:
        const b2BroadPhase* m_BroadPhase;
        QueryContext*      m_Context;
    };

    class OverlapShapeCallback
    {
    public:
        OverlapShapeCallback(const b2BroadPhase* broad_phase, QueryContext* context, const b2Shape* query_shape)
        : m_BroadPhase(broad_phase)
        , m_Context(context)
        , m_QueryShape(query_shape)
        {
            b2Transform identity;
            identity.SetIdentity();
            m_QueryTransform = identity;
        }

        bool QueryCallback(int32 proxy_id)
        {
            ++m_Context->m_Stats.m_LeafVisits;

            b2FixtureProxy* proxy = (b2FixtureProxy*)m_BroadPhase->GetUserData(proxy_id);
            if (!proxy || !MatchesFilter(proxy->fixture, proxy->childIndex, m_Context->m_Filter))
            {
                return true;
            }

            const int query_child_count = m_QueryShape->GetChildCount();
            b2Transform fixture_transform = proxy->fixture->GetBody()->GetTransform();
            for (int i = 0; i < query_child_count; ++i)
            {
                if (b2TestOverlap(m_QueryShape, i, proxy->fixture->GetShape(), proxy->childIndex, m_QueryTransform, fixture_transform))
                {
                    return PushFixtureResult(m_Context, proxy->fixture, proxy->childIndex);
                }
            }
            return true;
        }

    private:
        const b2BroadPhase* m_BroadPhase;
        QueryContext*      m_Context;
        const b2Shape*     m_QueryShape;
        b2Transform        m_QueryTransform;
    };

    class ShapeCastCallback
    {
    public:
        ShapeCastCallback(const b2BroadPhase* broad_phase, QueryContext* context, const b2Shape* query_shape, const b2Vec2& translation)
        : m_BroadPhase(broad_phase)
        , m_Context(context)
        , m_QueryShape(query_shape)
        , m_Translation(translation)
        {
        }

        bool QueryCallback(int32 proxy_id)
        {
            ++m_Context->m_Stats.m_LeafVisits;

            b2FixtureProxy* proxy = (b2FixtureProxy*)m_BroadPhase->GetUserData(proxy_id);
            if (!proxy || !MatchesFilter(proxy->fixture, proxy->childIndex, m_Context->m_Filter))
            {
                return true;
            }

            float best_fraction = 1.0f;
            b2Vec2 best_point = b2Vec2_zero;
            b2Vec2 best_normal = b2Vec2_zero;
            bool hit = false;
            const int query_child_count = m_QueryShape->GetChildCount();
            for (int i = 0; i < query_child_count; ++i)
            {
                float fraction = 0.0f;
                b2Vec2 point;
                b2Vec2 normal;
                if (ComputeShapeCastHit(m_QueryShape, i, proxy->fixture, proxy->childIndex, m_Translation, &fraction, &point, &normal) && fraction <= best_fraction)
                {
                    best_fraction = fraction;
                    best_point = point;
                    best_normal = normal;
                    hit = true;
                }
            }

            if (!hit || !HasResultCapacity(m_Context))
            {
                return HasResultCapacity(m_Context);
            }

            PushCastHit(m_Context->m_L, proxy->fixture, proxy->childIndex, best_point, best_normal, best_fraction);
            lua_rawseti(m_Context->m_L, m_Context->m_TableIndex, ++m_Context->m_Count);
            return HasResultCapacity(m_Context);
        }

    private:
        const b2BroadPhase* m_BroadPhase;
        QueryContext*      m_Context;
        const b2Shape*     m_QueryShape;
        b2Vec2             m_Translation;
    };

    class RayCastCallback : public b2RayCastCallback
    {
    public:
        RayCastCallback(QueryContext* context)
        : m_Context(context)
        {
        }

        float32 ReportFixture(b2Fixture* fixture, int32 child_index, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
        {
            ++m_Context->m_Stats.m_LeafVisits;

            if (!MatchesFilter(fixture, child_index, m_Context->m_Filter))
            {
                return -1.0f;
            }

            PushCastHit(m_Context->m_L, fixture, child_index, point, normal, fraction);
            lua_rawseti(m_Context->m_L, m_Context->m_TableIndex, ++m_Context->m_Count);
            return HasResultCapacity(m_Context) ? 1.0f : 0.0f;
        }

    private:
        QueryContext* m_Context;
    };

    class ClosestRayCastCallback : public b2RayCastCallback
    {
    public:
        ClosestRayCastCallback(QueryContext* context)
        : m_Context(context)
        , m_Fixture(0)
        , m_ChildIndex(0)
        , m_Point(b2Vec2_zero)
        , m_Normal(b2Vec2_zero)
        , m_Fraction(1.0f)
        {
        }

        float32 ReportFixture(b2Fixture* fixture, int32 child_index, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
        {
            ++m_Context->m_Stats.m_LeafVisits;

            if (!MatchesFilter(fixture, child_index, m_Context->m_Filter))
            {
                return -1.0f;
            }

            m_Fixture = fixture;
            m_ChildIndex = child_index;
            m_Point = point;
            m_Normal = normal;
            m_Fraction = fraction;
            return fraction;
        }

        bool HasHit() const
        {
            return m_Fixture != 0;
        }

        void PushHit(lua_State* L) const
        {
            PushCastHit(L, m_Fixture, m_ChildIndex, m_Point, m_Normal, m_Fraction);
        }

    private:
        QueryContext* m_Context;
        b2Fixture*   m_Fixture;
        int32        m_ChildIndex;
        b2Vec2       m_Point;
        b2Vec2       m_Normal;
        float32      m_Fraction;
    };

    static void InitQueryContext(QueryContext* context, lua_State* L, int table_index, const QueryFilter& filter, int max_results)
    {
        context->m_L = L;
        context->m_TableIndex = table_index;
        context->m_Count = 0;
        context->m_MaxResults = max_results;
        context->m_Filter = filter;
        context->m_Stats.m_NodeVisits = 0;
        context->m_Stats.m_LeafVisits = 0;
    }

    static int World_OverlapAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2World* world = CheckWorld(L, 1);
        b2AABB aabb = CheckAABB(L, 2);
        QueryFilter filter = CheckQueryFilter(L, 3);

        lua_newtable(L);
        QueryContext context;
        InitQueryContext(&context, L, AbsIndex(L, -1), filter, CheckMaxResults(L, 4));
        const b2BroadPhase& broad_phase = world->GetContactManager().m_broadPhase;
        OverlapAABBCallback callback(&broad_phase, &context);
        broad_phase.Query(&callback, aabb);

        PushTreeStats(L, context.m_Stats);
        return 2;
    }

    static int World_OverlapShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2World* world = CheckWorld(L, 1);

        FixtureShapeDef shape_def;
        const b2Shape* query_shape = CheckShapeDef(L, 2, &shape_def);
        b2Transform identity;
        identity.SetIdentity();
        b2AABB query_aabb = ComputeShapeAABB(query_shape, identity);

        QueryFilter filter = CheckQueryFilter(L, 3);
        lua_newtable(L);
        QueryContext context;
        InitQueryContext(&context, L, AbsIndex(L, -1), filter, CheckMaxResults(L, 4));
        const b2BroadPhase& broad_phase = world->GetContactManager().m_broadPhase;
        OverlapShapeCallback callback(&broad_phase, &context, query_shape);
        broad_phase.Query(&callback, query_aabb);

        PushTreeStats(L, context.m_Stats);
        return 2;
    }

    static int World_CastRay(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2World* world = CheckWorld(L, 1);
        b2Vec2 origin = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        QueryFilter filter = CheckQueryFilter(L, 4);

        lua_newtable(L);
        QueryContext context;
        InitQueryContext(&context, L, AbsIndex(L, -1), filter, CheckMaxResults(L, 5));
        RayCastCallback callback(&context);
        world->RayCast(&callback, origin, origin + translation);

        PushTreeStats(L, context.m_Stats);
        return 2;
    }

    static int World_CastRayClosest(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2World* world = CheckWorld(L, 1);
        b2Vec2 origin = CheckVec2(L, 2, GetPhysicsScale());
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());
        QueryFilter filter = CheckQueryFilter(L, 4);

        QueryContext context;
        InitQueryContext(&context, L, 0, filter, 1);
        ClosestRayCastCallback callback(&context);
        world->RayCast(&callback, origin, origin + translation);
        if (!callback.HasHit())
        {
            lua_pushnil(L);
            return 1;
        }

        callback.PushHit(L);
        lua_pushinteger(L, context.m_Stats.m_NodeVisits);
        lua_setfield(L, -2, "node_visits");
        lua_pushinteger(L, context.m_Stats.m_LeafVisits);
        lua_setfield(L, -2, "leaf_visits");
        return 1;
    }

    static int World_CastShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2World* world = CheckWorld(L, 1);

        FixtureShapeDef shape_def;
        const b2Shape* query_shape = CheckShapeDef(L, 2, &shape_def);
        b2Vec2 translation = CheckVec2(L, 3, GetPhysicsScale());

        b2Transform start_transform;
        start_transform.SetIdentity();
        b2Transform end_transform;
        end_transform.Set(translation, 0.0f);
        b2AABB query_aabb = ComputeShapeAABB(query_shape, start_transform);
        query_aabb.Combine(ComputeShapeAABB(query_shape, end_transform));

        QueryFilter filter = CheckQueryFilter(L, 4);
        lua_newtable(L);
        QueryContext context;
        InitQueryContext(&context, L, AbsIndex(L, -1), filter, CheckMaxResults(L, 5));
        const b2BroadPhase& broad_phase = world->GetContactManager().m_broadPhase;
        ShapeCastCallback callback(&broad_phase, &context, query_shape, translation);
        broad_phase.Query(&callback, query_aabb);

        PushTreeStats(L, context.m_Stats);
        return 2;
    }

    static const luaL_reg World_functions[] =
    {
        {"overlap_aabb", World_OverlapAABB},
        {"overlap_shape", World_OverlapShape},
        {"cast_ray", World_CastRay},
        {"cast_ray_closest", World_CastRayClosest},
        {"cast_shape", World_CastShape},
        {0,0}
    };

    void ScriptBox2DInitializeWorld(lua_State* L)
    {
        lua_newtable(L);
        luaL_register(L, 0, World_functions);
        lua_setfield(L, -2, "world");
    }
}

/*# Box2D b2World documentation
 *
 * Query and cast functions for the Defold-owned Box2D v2 world.
 *
 * @document
 * @name b2d.world
 * @namespace b2d.world
 * @language Lua
 */

/*# Overlap an AABB.
 * @name b2d.world.overlap_aabb
 * @param world [type: b2World] world from `b2d.get_world` or `b2d.body.get_world`
 * @param aabb [type: table] table with `lower` and `upper` vector3 fields
 * @param filter [type: table] optional query filter with `category_bits`, `mask_bits`, and optional `group_index`
 * @param max_results [type: number] optional maximum result count
 * @return fixtures [type: table] array of fixture info tables
 * @return stats [type: table] table with `node_visits` and `leaf_visits`
 */

/*# Overlap a shape.
 * @name b2d.world.overlap_shape
 * @param world [type: b2World] world from `b2d.get_world` or `b2d.body.get_world`
 * @param shape [type: table] shape table using the same format as the `shape` field in `b2d.body.create_fixture`
 * @param filter [type: table] optional query filter with `category_bits`, `mask_bits`, and optional `group_index`
 * @param max_results [type: number] optional maximum result count
 * @return fixtures [type: table] array of fixture info tables
 * @return stats [type: table] table with `node_visits` and `leaf_visits`
 */

/*# Cast a ray.
 * @name b2d.world.cast_ray
 * @param world [type: b2World] world from `b2d.get_world` or `b2d.body.get_world`
 * @param origin [type: vector3] world ray origin
 * @param translation [type: vector3] world ray translation
 * @param filter [type: table] optional query filter with `category_bits`, `mask_bits`, and optional `group_index`
 * @param max_results [type: number] optional maximum result count
 * @return hits [type: table] array of hit tables with `fixture`, `shape`, `point`, `normal`, and `fraction`
 * @return stats [type: table] table with `node_visits` and `leaf_visits`
 */

/*# Cast a ray and return the closest hit.
 * @name b2d.world.cast_ray_closest
 * @param world [type: b2World] world from `b2d.get_world` or `b2d.body.get_world`
 * @param origin [type: vector3] world ray origin
 * @param translation [type: vector3] world ray translation
 * @param filter [type: table] optional query filter with `category_bits`, `mask_bits`, and optional `group_index`
 * @return hit [type: table] hit table with `fixture`, `shape`, `point`, `normal`, `fraction`, `node_visits`, and `leaf_visits`, or nil
 */

/*# Cast a shape.
 * Uses Box2D v2 time-of-impact for fixture child shapes that support distance proxies.
 * Grid fixture children are skipped.
 * @name b2d.world.cast_shape
 * @param world [type: b2World] world from `b2d.get_world` or `b2d.body.get_world`
 * @param shape [type: table] shape table using the same format as the `shape` field in `b2d.body.create_fixture`
 * @param translation [type: vector3] world shape translation
 * @param filter [type: table] optional query filter with `category_bits`, `mask_bits`, and optional `group_index`
 * @param max_results [type: number] optional maximum result count
 * @return hits [type: table] array of hit tables with `fixture`, `shape`, `point`, `normal`, and `fraction`
 * @return stats [type: table] table with `node_visits` and `leaf_visits`
 */
