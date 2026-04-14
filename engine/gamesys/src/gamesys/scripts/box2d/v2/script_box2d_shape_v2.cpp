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
#include <limits.h>

#include <script/script.h>

#include "script_box2d_v2.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    enum
    {
        SHAPE_TYPE_BOX = b2Shape::e_polygon
    };

    static b2Vec2 CheckVec2(lua_State* L, int index, float scale)
    {
        dmVMath::Vector3* v = dmScript::CheckVector3(L, index);
        return b2Vec2(v->getX() * scale, v->getY() * scale);
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

    static void CheckVerticesTable(lua_State* L, int index, int min_count, int max_count, dmArray<b2Vec2>* out_vertices)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        int count = (int)lua_objlen(L, index);
        if (count < min_count || count > max_count)
        {
            luaL_error(L, "Expected %d-%d vertices, got %d.", min_count, max_count, count);
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

    const b2Shape* CheckShapeDef(lua_State* L, int index, FixtureShapeDef* out_shape)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "type");
        int type = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        if (type == b2Shape::e_circle)
        {
            lua_getfield(L, index, "radius");
            out_shape->m_Circle.m_radius = luaL_checknumber(L, -1) * GetPhysicsScale();
            lua_pop(L, 1);

            out_shape->m_Circle.m_p.SetZero();
            TryGetVec2Field(L, index, "center", &out_shape->m_Circle.m_p);
            out_shape->m_Circle.m_creationScale = out_shape->m_Circle.m_radius;
            out_shape->m_Circle.m_lastScale = 0.0f;
            out_shape->m_Circle.m_creationP = out_shape->m_Circle.m_p;
            return &out_shape->m_Circle;
        }

        if (type == b2Shape::e_edge)
        {
            lua_getfield(L, index, "v1");
            b2Vec2 v1 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);

            lua_getfield(L, index, "v2");
            b2Vec2 v2 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);

            out_shape->m_Edge.Set(v1, v2);
            out_shape->m_Edge.m_hasVertex0 = TryGetVec2Field(L, index, "v0", &out_shape->m_Edge.m_vertex0);
            out_shape->m_Edge.m_hasVertex3 = TryGetVec2Field(L, index, "v3", &out_shape->m_Edge.m_vertex3);
            out_shape->m_Edge.m_creationScale = 1.0f;
            out_shape->m_Edge.m_lastScale = 0.0f;
            return &out_shape->m_Edge;
        }

        if (type == b2Shape::e_polygon)
        {
            float hx = 0.0f;
            float hy = 0.0f;
            bool has_hx = TryGetNumberField(L, index, "hx", &hx);
            bool has_hy = TryGetNumberField(L, index, "hy", &hy);
            if (has_hx || has_hy)
            {
                if (!has_hx || !has_hy)
                {
                    luaL_error(L, "Polygon box shape requires both hx and hy.");
                    return 0;
                }

                b2Vec2 center(0.0f, 0.0f);
                float angle = 0.0f;
                bool has_center = TryGetVec2Field(L, index, "center", &center);
                bool has_angle = TryGetNumberField(L, index, "angle", &angle);
                if (has_center || has_angle)
                {
                    out_shape->m_Polygon.SetAsBox(hx * GetPhysicsScale(), hy * GetPhysicsScale(), center, angle);
                }
                else
                {
                    out_shape->m_Polygon.SetAsBox(hx * GetPhysicsScale(), hy * GetPhysicsScale());
                }
                out_shape->m_Polygon.m_creationScale = 1.0f;
                out_shape->m_Polygon.m_lastScale = 0.0f;
                return &out_shape->m_Polygon;
            }

            lua_getfield(L, index, "vertices");
            CheckVerticesTable(L, -1, 3, b2_maxPolygonVertices, &out_shape->m_Vertices);
            lua_pop(L, 1);
            out_shape->m_Polygon.Set(out_shape->m_Vertices.Begin(), out_shape->m_Vertices.Size());
            out_shape->m_Polygon.m_creationScale = 1.0f;
            out_shape->m_Polygon.m_lastScale = 0.0f;
            return &out_shape->m_Polygon;
        }

        if (type == b2Shape::e_chain)
        {
            bool is_loop = false;
            TryGetBooleanField(L, index, "loop", &is_loop);

            lua_getfield(L, index, "vertices");
            CheckVerticesTable(L, -1, is_loop ? 3 : 2, INT32_MAX, &out_shape->m_Vertices);
            lua_pop(L, 1);

            if (is_loop)
            {
                out_shape->m_Chain.CreateLoop(out_shape->m_Vertices.Begin(), out_shape->m_Vertices.Size());
            }
            else
            {
                out_shape->m_Chain.CreateChain(out_shape->m_Vertices.Begin(), out_shape->m_Vertices.Size());

                b2Vec2 prev_vertex;
                if (TryGetVec2Field(L, index, "prev_vertex", &prev_vertex))
                {
                    out_shape->m_Chain.SetPrevVertex(prev_vertex);
                }

                b2Vec2 next_vertex;
                if (TryGetVec2Field(L, index, "next_vertex", &next_vertex))
                {
                    out_shape->m_Chain.SetNextVertex(next_vertex);
                }
            }
            out_shape->m_Chain.m_creationScale = 1.0f;
            out_shape->m_Chain.m_lastScale = 0.0f;
            return &out_shape->m_Chain;
        }

        luaL_error(L, "Unsupported shape type %d.", type);
        return 0;
    }

    void ScriptBox2DInitializeShape(lua_State* L)
    {
        lua_newtable(L);

#define SET_CONSTANT(NAME, CUSTOM_NAME) \
        lua_pushnumber(L, (lua_Number) NAME); \
        lua_setfield(L, -2, CUSTOM_NAME);

        SET_CONSTANT(b2Shape::e_circle, "SHAPE_TYPE_CIRCLE");
        SET_CONSTANT(b2Shape::e_edge, "SHAPE_TYPE_EDGE");
        SET_CONSTANT(b2Shape::e_polygon, "SHAPE_TYPE_POLYGON");
        SET_CONSTANT(SHAPE_TYPE_BOX, "SHAPE_TYPE_BOX");
        SET_CONSTANT(b2Shape::e_chain, "SHAPE_TYPE_CHAIN");
        SET_CONSTANT(b2Shape::e_grid, "SHAPE_TYPE_GRID");

#undef SET_CONSTANT

        lua_setfield(L, -2, "shape");
    }
}

/*# Box2D b2Shape documentation
 *
 * Constants for functional shape tables used with `b2d.body.create_fixture`
 * and returned from `b2d.fixture.get_shape`.
 *
 * @document
 * @name b2d.shape
 * @namespace b2d.shape
 * @language Lua
 */

/*# Circle shape type.
 * @name b2d.shape.SHAPE_TYPE_CIRCLE
 * @constant
 */

/*# Edge shape type.
 * @name b2d.shape.SHAPE_TYPE_EDGE
 * @constant
 */

/*# Polygon shape type.
 * @name b2d.shape.SHAPE_TYPE_POLYGON
 * @constant
 */

/*# Box shape type alias.
 * Uses the polygon enum value, but indicates the `hx`/`hy` box convenience format.
 * @name b2d.shape.SHAPE_TYPE_BOX
 * @constant
 */

/*# Chain shape type.
 * @name b2d.shape.SHAPE_TYPE_CHAIN
 * @constant
 */

/*# Grid shape type.
 * @name b2d.shape.SHAPE_TYPE_GRID
 * @constant
 */
