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
    static uint32_t TYPE_HASH_CHAIN = 0;

    #define BOX2D_TYPE_NAME_CHAIN "b2chain"

    struct B2DLuaChain
    {
        b2ChainId m_Chain;
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

    static bool HasField(lua_State* L, int index, const char* field_name)
    {
        lua_getfield(L, index, field_name);
        bool has_field = !lua_isnil(L, -1);
        lua_pop(L, 1);
        return has_field;
    }

    static bool TryGetNumberField(lua_State* L, int index, const char* field_name, float* out_value)
    {
        lua_getfield(L, index, field_name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }

        *out_value = luaL_checknumber(L, -1);
        lua_pop(L, 1);
        return true;
    }

    static bool TryGetIntegerField(lua_State* L, int index, const char* field_name, int* out_value)
    {
        lua_getfield(L, index, field_name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }

        *out_value = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        return true;
    }

    static bool TryGetBooleanField(lua_State* L, int index, const char* field_name, bool* out_value)
    {
        lua_getfield(L, index, field_name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }

        *out_value = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return true;
    }

    static bool TryGetVec2Field(lua_State* L, int index, const char* field_name, b2Vec2* out_value)
    {
        lua_getfield(L, index, field_name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return false;
        }

        *out_value = CheckVec2(L, -1, GetPhysicsScale());
        lua_pop(L, 1);
        return true;
    }

    static void CheckVerticesTable(lua_State* L, int index, int min_count, dmArray<b2Vec2>* out_vertices)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        int count = (int)lua_objlen(L, index);
        if (count < min_count)
        {
            luaL_error(L, "Expected at least %d vertices, got %d.", min_count, count);
            return;
        }

        out_vertices->SetCapacity(count);
        out_vertices->SetSize(count);
        for (int i = 0; i < count; ++i)
        {
            lua_rawgeti(L, index, i + 1);
            (*out_vertices)[i] = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);
        }
    }

    static b2Filter CheckFilterData(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2Filter filter = b2DefaultFilter();

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

        lua_getfield(L, index, "group_index");
        if (!lua_isnil(L, -1))
        {
            filter.groupIndex = (int)luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

        return filter;
    }

    static void ValidateChainPoints(lua_State* L, const dmArray<b2Vec2>& points)
    {
        const float min_distance = 0.005f * b2GetLengthUnitsPerMeter();
        const float min_distance_squared = min_distance * min_distance;

        for (uint32_t i = 0; i < points.Size(); ++i)
        {
            for (uint32_t j = i + 1; j < points.Size(); ++j)
            {
                if (b2DistanceSquared(points[i], points[j]) < min_distance_squared)
                {
                    luaL_error(L, "Chain vertices must be separated by at least %g physics units.", min_distance);
                    return;
                }
            }
        }
    }

    static b2Vec2 SynthesizePreviousVertex(const dmArray<b2Vec2>& vertices)
    {
        return b2Add(vertices[0], b2Sub(vertices[0], vertices[1]));
    }

    static b2Vec2 SynthesizeNextVertex(const dmArray<b2Vec2>& vertices)
    {
        const uint32_t last = vertices.Size() - 1;
        return b2Add(vertices[last], b2Sub(vertices[last], vertices[last - 1]));
    }

    static void PushSegments(lua_State* L, b2ChainId chain)
    {
        int segment_count = b2Chain_GetSegmentCount(chain);
        dmArray<b2ShapeId> segments;
        segments.SetCapacity(segment_count);
        segments.SetSize(segment_count);

        int actual_count = b2Chain_GetSegments(chain, segments.Begin(), segment_count);

        lua_newtable(L);
        for (int i = 0; i < actual_count; ++i)
        {
            b2BodyId body = b2Shape_GetBody(segments[i]);
            PushShapeInfo(L, segments[i], GetShapeIndex(body, segments[i]));
            lua_rawseti(L, -2, i + 1);
        }
    }

    static B2DLuaChain* CheckChainInternal(lua_State* L, int index)
    {
        return (B2DLuaChain*)dmScript::CheckUserType(L, index, TYPE_HASH_CHAIN, "Expected user type " BOX2D_TYPE_NAME_CHAIN);
    }

    static b2ChainId* CheckChain(lua_State* L, int index)
    {
        B2DLuaChain* luachain = CheckChainInternal(L, index);
        if (!b2Chain_IsValid(luachain->m_Chain))
        {
            luaL_error(L, "Invalid b2chain handle.");
            return 0;
        }
        return &luachain->m_Chain;
    }

    static b2ChainId* ToChain(lua_State* L, int index)
    {
        B2DLuaChain* luachain = (B2DLuaChain*)dmScript::ToUserType(L, index, TYPE_HASH_CHAIN);
        return luachain ? &luachain->m_Chain : 0;
    }

    static void CheckChainUnlocked(lua_State* L, b2ChainId chain, const char* function_name)
    {
        b2WorldId world = b2Chain_GetWorld(chain);
        if (b2World_IsLocked(world))
        {
            luaL_error(L, "Could not call b2d.chain.%s. The world is locked.", function_name);
        }
    }

    static b2ShapeId CheckBodyShape(lua_State* L, int body_index, int shape_index_index)
    {
        b2BodyId* body = CheckBody(L, body_index);
        int shape_index = luaL_checkinteger(L, shape_index_index);
        b2ShapeId shape = GetShapeByIndex(*body, shape_index);
        if (!b2Shape_IsValid(shape))
        {
            luaL_error(L, "shape_index %d out of range.", shape_index);
            return b2_nullShapeId;
        }
        return shape;
    }

    static b2ShapeId CheckShapeArg(lua_State* L, int index)
    {
        b2ShapeId* shape = ToShapeId(L, index);
        if (shape)
        {
            if (!b2Shape_IsValid(*shape))
            {
                luaL_error(L, "Invalid b2shape handle.");
                return b2_nullShapeId;
            }
            return *shape;
        }

        return CheckBodyShape(L, index, index + 1);
    }

    void PushChain(lua_State* L, b2ChainId chain_id)
    {
        B2DLuaChain* luachain = (B2DLuaChain*)lua_newuserdata(L, sizeof(B2DLuaChain));
        luachain->m_Chain = chain_id;

        luaL_getmetatable(L, BOX2D_TYPE_NAME_CHAIN);
        lua_setmetatable(L, -2);
    }

    int Body_CreateChain(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        b2BodyId* body = CheckBody(L, 1);
        b2WorldId world = b2Body_GetWorld(*body);
        if (b2World_IsLocked(world))
        {
            return luaL_error(L, "Could not create chain. The world is locked.");
        }

        int definition_index = AbsIndex(L, 2);
        luaL_checktype(L, definition_index, LUA_TTABLE);

        bool loop = false;
        TryGetBooleanField(L, definition_index, "loop", &loop);

        if (loop && (HasField(L, definition_index, "prev_vertex") || HasField(L, definition_index, "next_vertex")))
        {
            return luaL_error(L, "Loop chains do not use prev_vertex or next_vertex.");
        }

        dmArray<b2Vec2> vertices;
        lua_getfield(L, definition_index, "vertices");
        CheckVerticesTable(L, -1, loop ? 4 : 2, &vertices);
        lua_pop(L, 1);

        dmArray<b2Vec2> chain_points;
        if (loop)
        {
            chain_points.SetCapacity(vertices.Size());
            chain_points.SetSize(vertices.Size());
            for (uint32_t i = 0; i < vertices.Size(); ++i)
            {
                chain_points[i] = vertices[i];
            }
        }
        else
        {
            chain_points.SetCapacity(vertices.Size() + 2);
            chain_points.SetSize(vertices.Size() + 2);

            b2Vec2 prev_vertex = SynthesizePreviousVertex(vertices);
            b2Vec2 next_vertex = SynthesizeNextVertex(vertices);
            TryGetVec2Field(L, definition_index, "prev_vertex", &prev_vertex);
            TryGetVec2Field(L, definition_index, "next_vertex", &next_vertex);

            chain_points[0] = prev_vertex;
            for (uint32_t i = 0; i < vertices.Size(); ++i)
            {
                chain_points[i + 1] = vertices[i];
            }
            chain_points[chain_points.Size() - 1] = next_vertex;
        }

        ValidateChainPoints(L, chain_points);

        b2SurfaceMaterial material = b2DefaultSurfaceMaterial();

        float float_value = 0.0f;
        if (TryGetNumberField(L, definition_index, "friction", &float_value))
        {
            material.friction = float_value;
        }

        if (TryGetNumberField(L, definition_index, "restitution", &float_value))
        {
            material.restitution = float_value;
        }

        int material_value = 0;
        if (TryGetIntegerField(L, definition_index, "material", &material_value))
        {
            material.userMaterialId = material_value;
        }

        b2ChainDef chain_def = b2DefaultChainDef();
        chain_def.userData = b2Body_GetUserData(*body);
        chain_def.points = chain_points.Begin();
        chain_def.count = chain_points.Size();
        chain_def.materials = &material;
        chain_def.materialCount = 1;
        chain_def.isLoop = loop;

        lua_getfield(L, definition_index, "filter");
        if (!lua_isnil(L, -1))
        {
            chain_def.filter = CheckFilterData(L, -1);
        }
        lua_pop(L, 1);

        bool enable_sensor_events = chain_def.enableSensorEvents;
        TryGetBooleanField(L, definition_index, "enable_sensor_events", &enable_sensor_events);
        chain_def.enableSensorEvents = enable_sensor_events;

        b2ChainId chain = b2CreateChain(*body, &chain_def);
        if (!b2Chain_IsValid(chain))
        {
            return luaL_error(L, "Could not create chain. The world may be locked.");
        }

        PushChain(L, chain);
        PushSegments(L, chain);
        return 2;
    }

    static int Chain_Destroy(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        B2DLuaChain* luachain = CheckChainInternal(L, 1);
        if (!b2Chain_IsValid(luachain->m_Chain))
        {
            return luaL_error(L, "Invalid b2chain handle.");
        }

        CheckChainUnlocked(L, luachain->m_Chain, "destroy");
        b2DestroyChain(luachain->m_Chain);
        luachain->m_Chain = b2_nullChainId;
        return 0;
    }

    static int Chain_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        B2DLuaChain* luachain = CheckChainInternal(L, 1);
        lua_pushboolean(L, b2Chain_IsValid(luachain->m_Chain));
        return 1;
    }

    static int Chain_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushWorldId(L, b2Chain_GetWorld(*CheckChain(L, 1)));
        return 1;
    }

    static int Chain_GetSegmentCount(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "get_segment_count");
        lua_pushinteger(L, b2Chain_GetSegmentCount(chain));
        return 1;
    }

    static int Chain_GetSegments(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "get_segments");
        PushSegments(L, chain);
        return 1;
    }

    static bool PointsEqual(const b2Vec2& a, const b2Vec2& b)
    {
        return b2DistanceSquared(a, b) < 0.000001f;
    }

    static int Chain_GetGeometry(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "get_geometry");

        int segment_count = b2Chain_GetSegmentCount(chain);
        dmArray<b2ShapeId> segments;
        segments.SetCapacity(segment_count);
        segments.SetSize(segment_count);
        segment_count = b2Chain_GetSegments(chain, segments.Begin(), segment_count);
        if (segment_count <= 0)
        {
            return luaL_error(L, "Could not resolve chain segments.");
        }

        b2ChainSegment first_segment = b2Shape_GetChainSegment(segments[0]);
        b2ChainSegment last_segment = b2Shape_GetChainSegment(segments[segment_count - 1]);
        bool loop = PointsEqual(last_segment.segment.point2, first_segment.segment.point1);

        lua_newtable(L);

        lua_pushboolean(L, loop);
        lua_setfield(L, -2, "loop");

        lua_pushinteger(L, segment_count);
        lua_setfield(L, -2, "segment_count");

        lua_newtable(L);
        if (loop)
        {
            for (int i = 0; i < segment_count; ++i)
            {
                b2ChainSegment segment = b2Shape_GetChainSegment(segments[i]);
                dmScript::PushVector3(L, FromB2(segment.segment.point1, GetInvPhysicsScale()));
                lua_rawseti(L, -2, i + 1);
            }
        }
        else
        {
            dmScript::PushVector3(L, FromB2(first_segment.segment.point1, GetInvPhysicsScale()));
            lua_rawseti(L, -2, 1);

            for (int i = 0; i < segment_count; ++i)
            {
                b2ChainSegment segment = b2Shape_GetChainSegment(segments[i]);
                dmScript::PushVector3(L, FromB2(segment.segment.point2, GetInvPhysicsScale()));
                lua_rawseti(L, -2, i + 2);
            }
        }
        lua_setfield(L, -2, "vertices");

        if (!loop)
        {
            dmScript::PushVector3(L, FromB2(first_segment.ghost1, GetInvPhysicsScale()));
            lua_setfield(L, -2, "prev_vertex");

            dmScript::PushVector3(L, FromB2(last_segment.ghost2, GetInvPhysicsScale()));
            lua_setfield(L, -2, "next_vertex");
        }

        return 1;
    }

    static int Chain_FromShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShapeArg(L, 1);
        b2ChainId chain = b2Shape_GetParentChain(shape);
        if (!b2Chain_IsValid(chain))
        {
            lua_pushnil(L);
            return 1;
        }

        PushChain(L, chain);
        return 1;
    }

    static int Chain_GetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Chain_GetFriction(*CheckChain(L, 1)));
        return 1;
    }

    static int Chain_SetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "set_friction");
        b2Chain_SetFriction(chain, luaL_checknumber(L, 2));
        return 0;
    }

    static int Chain_GetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Chain_GetRestitution(*CheckChain(L, 1)));
        return 1;
    }

    static int Chain_SetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "set_restitution");
        b2Chain_SetRestitution(chain, luaL_checknumber(L, 2));
        return 0;
    }

    static int Chain_GetMaterial(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Chain_GetMaterial(*CheckChain(L, 1)));
        return 1;
    }

    static int Chain_SetMaterial(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ChainId chain = *CheckChain(L, 1);
        CheckChainUnlocked(L, chain, "set_material");
        b2Chain_SetMaterial(chain, luaL_checkinteger(L, 2));
        return 0;
    }

    static int Chain_tostring(lua_State *L)
    {
        B2DLuaChain* luachain = CheckChainInternal(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_CHAIN, &luachain->m_Chain);
        return 1;
    }

    static int Chain_eq(lua_State *L)
    {
        b2ChainId* a = ToChain(L, 1);
        b2ChainId* b = ToChain(L, 2);
        lua_pushboolean(L, a && b && memcmp(a, b, sizeof(b2ChainId)) == 0);
        return 1;
    }

    static const luaL_reg Chain_methods[] =
    {
        {0,0}
    };

    static const luaL_reg Chain_meta[] =
    {
        {"__tostring", Chain_tostring},
        {"__eq", Chain_eq},
        {0,0}
    };

    static const luaL_reg Chain_functions[] =
    {
        {"destroy", Chain_Destroy},
        {"is_valid", Chain_IsValid},
        {"get_world", Chain_GetWorld},
        {"get_segment_count", Chain_GetSegmentCount},
        {"get_segments", Chain_GetSegments},
        {"get_geometry", Chain_GetGeometry},
        {"from_shape", Chain_FromShape},
        {"get_friction", Chain_GetFriction},
        {"set_friction", Chain_SetFriction},
        {"get_restitution", Chain_GetRestitution},
        {"set_restitution", Chain_SetRestitution},
        {"get_material", Chain_GetMaterial},
        {"set_material", Chain_SetMaterial},
        {0,0}
    };

    void ScriptBox2DInitializeChain(lua_State* L)
    {
        TYPE_HASH_CHAIN = dmScript::RegisterUserType(L, BOX2D_TYPE_NAME_CHAIN, Chain_methods, Chain_meta);

        lua_newtable(L);
        luaL_register(L, 0, Chain_functions);
        lua_setfield(L, -2, "chain");
    }

    void ScriptBox2DFinalizeChain()
    {
        TYPE_HASH_CHAIN = 0;
    }
}

/*# Box2D b2Chain documentation
 *
 * Functions for Box2D v3 chains. A chain owns multiple connected segment
 * shapes, so it is represented by a separate `b2Chain` handle.
 *
 * @document
 * @name b2d.chain
 * @namespace b2d.chain
 * @language Lua
 * @version 3
 */

/*# Box2D chain
 * @typedef
 * @name b2d.chain
 * @param value [type:userdata]
 */

/*# Destroy a chain.
 * Destroying a chain removes all segment shapes owned by the chain. Destroying
 * any segment shape through `b2d.body.destroy_shape` also destroys its parent chain.
 * @warning This function is locked during callbacks.
 * @name b2d.chain.destroy
 * @param chain [type: b2Chain] chain
 */

/*# Validate a chain handle.
 * @name b2d.chain.is_valid
 * @param chain [type: b2Chain] chain
 * @return valid [type: boolean] true if the chain handle still refers to a live Box2D chain
 */

/*# Get the world owning a chain.
 * @name b2d.chain.get_world
 * @param chain [type: b2Chain] chain
 * @return world [type: b2World] owning world
 */

/*# Get the number of segment shapes in a chain.
 * @name b2d.chain.get_segment_count
 * @param chain [type: b2Chain] chain
 * @return count [type: number] segment count
 */

/*# Get the segment shapes owned by a chain.
 * @name b2d.chain.get_segments
 * @param chain [type: b2Chain] chain
 * @return segments [type: table] array of shape info tables for the chain segments. Each entry includes `shape_id`.
 */

/*# Get the chain geometry.
 * Returns a chain geometry table with `loop`, `segment_count`, and `vertices`.
 * Open chains also include `prev_vertex` and `next_vertex` ghost vertices.
 * @name b2d.chain.get_geometry
 * @param chain [type: b2Chain] chain
 * @return geometry [type: table] chain geometry table
 */

/*# Get the parent chain for a chain segment shape.
 * Returns `nil` if the shape is not a chain segment.
 * @name b2d.chain.from_shape
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return chain [type: b2Chain|nil] parent chain, or `nil` if the shape is not a chain segment
 */

/*# Get chain friction.
 * @name b2d.chain.get_friction
 * @param chain [type: b2Chain] chain
 * @return friction [type: number] chain friction
 */

/*# Set chain friction.
 * @warning This function is locked during callbacks.
 * @name b2d.chain.set_friction
 * @param chain [type: b2Chain] chain
 * @param friction [type: number] chain friction
 */

/*# Get chain restitution.
 * @name b2d.chain.get_restitution
 * @param chain [type: b2Chain] chain
 * @return restitution [type: number] chain restitution
 */

/*# Set chain restitution.
 * @warning This function is locked during callbacks.
 * @name b2d.chain.set_restitution
 * @param chain [type: b2Chain] chain
 * @param restitution [type: number] chain restitution
 */

/*# Get chain material id.
 * @name b2d.chain.get_material
 * @param chain [type: b2Chain] chain
 * @return material [type: number] chain material id
 */

/*# Set chain material id.
 * @warning This function is locked during callbacks.
 * @name b2d.chain.set_material
 * @param chain [type: b2Chain] chain
 * @param material [type: number] chain material id
 */
