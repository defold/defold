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

    static b2Filter CheckFilterData(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2Filter filter = b2DefaultFilter();

        lua_getfield(L, index, "category_bits");
        filter.categoryBits = (uint64_t)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "mask_bits");
        filter.maskBits = (uint64_t)luaL_checknumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "group_index");
        filter.groupIndex = (int)luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        return filter;
    }

    B2DShapeDef CheckShapeDef(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "type");
        int type = luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        B2DShapeDef shape_def = {};

        if (type == b2_circleShape)
        {
            shape_def.m_Type = B2DShapeDef::TYPE_CIRCLE;

            lua_getfield(L, index, "radius");
            shape_def.m_Circle.radius = luaL_checknumber(L, -1) * GetPhysicsScale();
            lua_pop(L, 1);

            shape_def.m_Circle.center = b2Vec2_zero;
            TryGetVec2Field(L, index, "center", &shape_def.m_Circle.center);
            return shape_def;
        }

        if (type == b2_capsuleShape)
        {
            shape_def.m_Type = B2DShapeDef::TYPE_CAPSULE;

            lua_getfield(L, index, "radius");
            shape_def.m_Capsule.radius = luaL_checknumber(L, -1) * GetPhysicsScale();
            lua_pop(L, 1);

            lua_getfield(L, index, "center1");
            shape_def.m_Capsule.center1 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);

            lua_getfield(L, index, "center2");
            shape_def.m_Capsule.center2 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);
            return shape_def;
        }

        if (type == SHAPE_TYPE_EDGE)
        {
            shape_def.m_Type = B2DShapeDef::TYPE_SEGMENT;

            lua_getfield(L, index, "v1");
            shape_def.m_Segment.point1 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);

            lua_getfield(L, index, "v2");
            shape_def.m_Segment.point2 = CheckVec2(L, -1, GetPhysicsScale());
            lua_pop(L, 1);
            return shape_def;
        }

        if (type == b2_polygonShape)
        {
            shape_def.m_Type = B2DShapeDef::TYPE_POLYGON;

            float hx = 0.0f;
            float hy = 0.0f;
            bool has_hx = TryGetNumberField(L, index, "hx", &hx);
            bool has_hy = TryGetNumberField(L, index, "hy", &hy);
            if (has_hx || has_hy)
            {
                if (!has_hx || !has_hy)
                {
                    luaL_error(L, "Polygon box shape requires both hx and hy.");
                    return shape_def;
                }

                b2Vec2 center = b2Vec2_zero;
                float angle = 0.0f;
                bool has_center = TryGetVec2Field(L, index, "center", &center);
                bool has_angle = TryGetNumberField(L, index, "angle", &angle);
                if (has_center || has_angle)
                {
                    shape_def.m_Polygon = b2MakeOffsetBox(hx * GetPhysicsScale(), hy * GetPhysicsScale(), center, b2MakeRot(angle));
                }
                else
                {
                    shape_def.m_Polygon = b2MakeBox(hx * GetPhysicsScale(), hy * GetPhysicsScale());
                }
                return shape_def;
            }

            dmArray<b2Vec2> vertices;
            lua_getfield(L, index, "vertices");
            CheckVerticesTable(L, -1, 3, B2_MAX_POLYGON_VERTICES, &vertices);
            lua_pop(L, 1);

            b2Hull hull = b2ComputeHull(vertices.Begin(), vertices.Size());
            if (hull.count < 3)
            {
                luaL_error(L, "Could not create polygon hull from %d vertices.", vertices.Size());
                return shape_def;
            }

            shape_def.m_Polygon = b2MakePolygon(&hull, 0.0f);
            return shape_def;
        }

        luaL_error(L, "Unsupported shape type %d.", type);
        return shape_def;
    }

    void CheckShapeCreateDef(lua_State* L, int index, B2DShapeDef* out_shape_def, b2ShapeDef* out_shape_create_def)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2ShapeDef shape_create_def = b2DefaultShapeDef();

        lua_getfield(L, index, "shape");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            *out_shape_def = CheckShapeDef(L, index);
        }
        else
        {
            *out_shape_def = CheckShapeDef(L, -1);
            lua_pop(L, 1);
        }

        float value = 0.0f;
        if (TryGetNumberField(L, index, "friction", &value))
        {
            shape_create_def.material.friction = value;
        }

        if (TryGetNumberField(L, index, "restitution", &value))
        {
            shape_create_def.material.restitution = value;
        }

        if (TryGetNumberField(L, index, "density", &value))
        {
            shape_create_def.density = value;
        }

        bool sensor = shape_create_def.isSensor;
        bool has_sensor = TryGetBooleanField(L, index, "sensor", &sensor);
        if (!has_sensor)
        {
            TryGetBooleanField(L, index, "is_sensor", &sensor);
        }
        shape_create_def.isSensor = sensor;

        lua_getfield(L, index, "filter");
        if (!lua_isnil(L, -1))
        {
            shape_create_def.filter = CheckFilterData(L, -1);
        }
        lua_pop(L, 1);

        lua_getfield(L, index, "user_data");
        if (!lua_isnil(L, -1))
        {
            luaL_error(L, "Shape user_data is not supported.");
            return;
        }
        lua_pop(L, 1);

        *out_shape_create_def = shape_create_def;
    }

    void PushShape(lua_State* L, b2ShapeId shape_id)
    {
        lua_newtable(L);

        switch (b2Shape_GetType(shape_id))
        {
            case b2_circleShape:
            {
                b2Circle circle = b2Shape_GetCircle(shape_id);
                lua_pushinteger(L, b2_circleShape);
                lua_setfield(L, -2, "type");

                lua_pushnumber(L, circle.radius * GetInvPhysicsScale());
                lua_setfield(L, -2, "radius");

                dmScript::PushVector3(L, FromB2(circle.center, GetInvPhysicsScale()));
                lua_setfield(L, -2, "center");
                return;
            }

            case b2_capsuleShape:
            {
                b2Capsule capsule = b2Shape_GetCapsule(shape_id);
                lua_pushinteger(L, b2_capsuleShape);
                lua_setfield(L, -2, "type");

                dmScript::PushVector3(L, FromB2(capsule.center1, GetInvPhysicsScale()));
                lua_setfield(L, -2, "center1");

                dmScript::PushVector3(L, FromB2(capsule.center2, GetInvPhysicsScale()));
                lua_setfield(L, -2, "center2");

                lua_pushnumber(L, capsule.radius * GetInvPhysicsScale());
                lua_setfield(L, -2, "radius");
                return;
            }

            case b2_segmentShape:
            {
                b2Segment segment = b2Shape_GetSegment(shape_id);
                lua_pushinteger(L, SHAPE_TYPE_EDGE);
                lua_setfield(L, -2, "type");

                dmScript::PushVector3(L, FromB2(segment.point1, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v1");

                dmScript::PushVector3(L, FromB2(segment.point2, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v2");
                return;
            }

            case b2_chainSegmentShape:
            {
                b2ChainSegment segment = b2Shape_GetChainSegment(shape_id);
                lua_pushinteger(L, SHAPE_TYPE_EDGE);
                lua_setfield(L, -2, "type");

                dmScript::PushVector3(L, FromB2(segment.ghost1, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v0");

                dmScript::PushVector3(L, FromB2(segment.segment.point1, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v1");

                dmScript::PushVector3(L, FromB2(segment.segment.point2, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v2");

                dmScript::PushVector3(L, FromB2(segment.ghost2, GetInvPhysicsScale()));
                lua_setfield(L, -2, "v3");
                return;
            }

            case b2_polygonShape:
            {
                b2Polygon polygon = b2Shape_GetPolygon(shape_id);
                lua_pushinteger(L, b2_polygonShape);
                lua_setfield(L, -2, "type");

                lua_newtable(L);
                for (int i = 0; i < polygon.count; ++i)
                {
                    dmScript::PushVector3(L, FromB2(polygon.vertices[i], GetInvPhysicsScale()));
                    lua_rawseti(L, -2, i + 1);
                }
                lua_setfield(L, -2, "vertices");
                return;
            }

            default:
            {
                luaL_error(L, "Unsupported shape type %d for Lua shape snapshot.", b2Shape_GetType(shape_id));
                return;
            }
        }
    }

    static b2ShapeId CheckShape(lua_State* L, int body_index, int shape_index_index)
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

    static void CheckChildIndex(lua_State* L, int index)
    {
        int child_index = luaL_checkinteger(L, index);
        if (child_index != 1)
        {
            luaL_error(L, "child_index %d out of range [1, 1].", child_index);
        }
    }

    static void PushFilterData(lua_State* L, const b2Filter& filter)
    {
        lua_newtable(L);

        lua_pushnumber(L, (lua_Number)filter.categoryBits);
        lua_setfield(L, -2, "category_bits");

        lua_pushnumber(L, (lua_Number)filter.maskBits);
        lua_setfield(L, -2, "mask_bits");

        lua_pushinteger(L, filter.groupIndex);
        lua_setfield(L, -2, "group_index");
    }

    static int Shape_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1, 2);
        lua_pushinteger(L, NormalizeShapeTypeForLua(b2Shape_GetType(shape)));
        return 1;
    }

    static int Shape_GetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushShape(L, CheckShape(L, 1, 2));
        return 1;
    }

    static int Shape_IsSensor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_IsSensor(CheckShape(L, 1, 2)));
        return 1;
    }

    static int Shape_SetSensor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        return luaL_error(L, "Box2D v3 does not support changing sensor state after shape creation.");
    }

    static int Shape_GetDensity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Shape_GetDensity(CheckShape(L, 1, 2)));
        return 1;
    }

    static int Shape_SetDensity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1, 2);
        bool update_mass = lua_gettop(L) >= 4 && !lua_isnil(L, 4) && lua_toboolean(L, 4);
        b2Shape_SetDensity(shape, luaL_checknumber(L, 3), update_mass);
        return 0;
    }

    static int Shape_GetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Shape_GetFriction(CheckShape(L, 1, 2)));
        return 1;
    }

    static int Shape_SetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_SetFriction(CheckShape(L, 1, 2), luaL_checknumber(L, 3));
        return 0;
    }

    static int Shape_GetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Shape_GetRestitution(CheckShape(L, 1, 2)));
        return 1;
    }

    static int Shape_SetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_SetRestitution(CheckShape(L, 1, 2), luaL_checknumber(L, 3));
        return 0;
    }

    static int Shape_GetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1, 2);
        CheckChildIndex(L, 3);
        PushFilterData(L, b2Shape_GetFilter(shape));
        return 1;
    }

    static int Shape_SetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1, 2);
        CheckChildIndex(L, 3);
        b2Shape_SetFilter(shape, CheckFilterData(L, 4));
        return 0;
    }

    static int Shape_Refilter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1, 2);
        b2Shape_SetFilter(shape, b2Shape_GetFilter(shape));
        return 0;
    }

    static int Shape_TestPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1, 2);
        b2Vec2 point = CheckVec2(L, 3, GetPhysicsScale());
        lua_pushboolean(L, b2Shape_TestPoint(shape, point));
        return 1;
    }

    static int Shape_GetAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1, 2);
        CheckChildIndex(L, 3);
        b2AABB aabb = b2Shape_GetAABB(shape);

        lua_newtable(L);
        dmScript::PushVector3(L, FromB2(aabb.lowerBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "lower");

        dmScript::PushVector3(L, FromB2(aabb.upperBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "upper");
        return 1;
    }

    static const luaL_reg Shape_functions[] =
    {
        {"get_shape", Shape_GetShape},
        {"get_type", Shape_GetType},
        {"is_sensor", Shape_IsSensor},
        {"set_sensor", Shape_SetSensor},
        {"get_density", Shape_GetDensity},
        {"set_density", Shape_SetDensity},
        {"get_friction", Shape_GetFriction},
        {"set_friction", Shape_SetFriction},
        {"get_restitution", Shape_GetRestitution},
        {"set_restitution", Shape_SetRestitution},
        {"get_filter_data", Shape_GetFilterData},
        {"set_filter_data", Shape_SetFilterData},
        {"refilter", Shape_Refilter},
        {"test_point", Shape_TestPoint},
        {"get_aabb", Shape_GetAABB},
        {0,0}
    };

    void ScriptBox2DInitializeShape(lua_State* L)
    {
        lua_newtable(L);
        luaL_register(L, 0, Shape_functions);

#define SET_CONSTANT(NAME, CUSTOM_NAME) \
        lua_pushnumber(L, (lua_Number) NAME); \
        lua_setfield(L, -2, CUSTOM_NAME);

        SET_CONSTANT(b2_circleShape, "SHAPE_TYPE_CIRCLE");
        SET_CONSTANT(b2_capsuleShape, "SHAPE_TYPE_CAPSULE");
        SET_CONSTANT(SHAPE_TYPE_EDGE, "SHAPE_TYPE_EDGE");
        SET_CONSTANT(SHAPE_TYPE_EDGE, "SHAPE_TYPE_SEGMENT");
        SET_CONSTANT(b2_polygonShape, "SHAPE_TYPE_POLYGON");
        SET_CONSTANT(SHAPE_TYPE_BOX, "SHAPE_TYPE_BOX");

#undef SET_CONSTANT

        lua_setfield(L, -2, "shape");
    }
}
