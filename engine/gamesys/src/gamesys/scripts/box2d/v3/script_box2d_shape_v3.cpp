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

#include "gamesys.h"
#include "script_box2d_v3.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static uint32_t TYPE_HASH_SHAPE = 0;

    #define BOX2D_TYPE_NAME_SHAPE "b2shape"

    struct B2DLuaShape
    {
        b2ShapeId m_Shape;
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

    static void PushMassData(lua_State* L, const b2MassData& mass_data)
    {
        lua_newtable(L);

        lua_pushnumber(L, mass_data.mass);
        lua_setfield(L, -2, "mass");

        dmScript::PushVector3(L, FromB2(mass_data.center, GetInvPhysicsScale()));
        lua_setfield(L, -2, "center");

        lua_pushnumber(L, mass_data.rotationalInertia);
        lua_setfield(L, -2, "inertia");
    }

    static void PushCastOutput(lua_State* L, const b2CastOutput& output)
    {
        lua_newtable(L);

        dmScript::PushVector3(L, FromB2(output.point, GetInvPhysicsScale()));
        lua_setfield(L, -2, "point");

        dmScript::PushVector3(L, FromB2(output.normal, 1.0f));
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, output.fraction);
        lua_setfield(L, -2, "fraction");

        lua_pushinteger(L, output.iterations);
        lua_setfield(L, -2, "iterations");
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

        if (type == SHAPE_TYPE_SEGMENT)
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

    static B2DShapeDef CheckShapeUpdateDef(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        lua_getfield(L, index, "shape");
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return CheckShapeDef(L, index);
        }

        B2DShapeDef shape_def = CheckShapeDef(L, -1);
        lua_pop(L, 1);
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

        lua_getfield(L, index, "material");
        if (!lua_isnil(L, -1))
        {
            shape_create_def.material.userMaterialId = luaL_checkinteger(L, -1);
        }
        lua_pop(L, 1);

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

    static B2DLuaShape* CheckShapeInternal(lua_State* L, int index)
    {
        return (B2DLuaShape*)dmScript::CheckUserType(L, index, TYPE_HASH_SHAPE, "Expected user type " BOX2D_TYPE_NAME_SHAPE);
    }

    b2ShapeId* CheckShapeId(lua_State* L, int index)
    {
        B2DLuaShape* luashape = CheckShapeInternal(L, index);
        if (!b2Shape_IsValid(luashape->m_Shape))
        {
            luaL_error(L, "Invalid b2shape handle.");
            return 0;
        }
        return &luashape->m_Shape;
    }

    b2ShapeId* ToShapeId(lua_State* L, int index)
    {
        B2DLuaShape* luashape = (B2DLuaShape*)dmScript::ToUserType(L, index, TYPE_HASH_SHAPE);
        return luashape ? &luashape->m_Shape : 0;
    }

    void PushShapeId(lua_State* L, b2ShapeId shape_id)
    {
        B2DLuaShape* luashape = (B2DLuaShape*)lua_newuserdata(L, sizeof(B2DLuaShape));
        luashape->m_Shape = shape_id;

        luaL_getmetatable(L, BOX2D_TYPE_NAME_SHAPE);
        lua_setmetatable(L, -2);
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
                lua_pushinteger(L, SHAPE_TYPE_SEGMENT);
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
                lua_pushinteger(L, SHAPE_TYPE_SEGMENT);
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

    static b2ShapeId CheckShapeByBodyIndex(lua_State* L, int body_index, int shape_index_index)
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

    static b2ShapeId CheckShape(lua_State* L, int index)
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

        return CheckShapeByBodyIndex(L, index, index + 1);
    }

    static int GetShapeArgCount(lua_State* L, int index)
    {
        return ToShapeId(L, index) ? 1 : 2;
    }

    static int GetShapeValueIndex(lua_State* L, int index)
    {
        return index + GetShapeArgCount(L, index);
    }

    static void CheckChildIndex(lua_State* L, int index)
    {
        int child_index = luaL_checkinteger(L, index);
        if (child_index != 1)
        {
            luaL_error(L, "child_index %d out of range [1, 1].", child_index);
        }
    }

    static void CheckOptionalChildIndex(lua_State* L, int index)
    {
        if (!lua_isnoneornil(L, index))
        {
            CheckChildIndex(L, index);
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

    static int Shape_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId* shape = ToShapeId(L, 1);
        if (shape)
        {
            lua_pushboolean(L, b2Shape_IsValid(*shape));
            return 1;
        }

        b2BodyId* body = CheckBody(L, 1);
        int shape_index = luaL_checkinteger(L, 2);
        lua_pushboolean(L, b2Shape_IsValid(GetShapeByIndex(*body, shape_index)));
        return 1;
    }

    static int Shape_GetBody(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2BodyId body = b2Shape_GetBody(CheckShape(L, 1));
        CollisionComponent* component = (CollisionComponent*)b2Body_GetUserData(body);
        if (!component || !component->m_Instance)
        {
            return luaL_error(L, "Could not resolve shape body owner.");
        }

        dmGameObject::HCollection collection = dmGameObject::GetCollection(component->m_Instance);
        dmhash_t instance_id = dmGameObject::GetIdentifier(component->m_Instance);
        PushBody(L, &body, collection, instance_id);
        return 1;
    }

    static int Shape_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushWorldId(L, b2Shape_GetWorld(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        lua_pushinteger(L, NormalizeShapeTypeForLua(b2Shape_GetType(shape)));
        return 1;
    }

    static int Shape_GetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushShape(L, CheckShape(L, 1));
        return 1;
    }

    static int Shape_SetShape(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1);
        int shape_def_index = GetShapeValueIndex(L, 1);

        b2ChainId parent_chain = b2Shape_GetParentChain(shape);
        if (b2Chain_IsValid(parent_chain))
        {
            return luaL_error(L, "Cannot set chain segment shape geometry.");
        }

        b2WorldId world = b2Shape_GetWorld(shape);
        if (b2World_IsLocked(world))
        {
            return luaL_error(L, "Could not set shape. The world is locked.");
        }

        B2DShapeDef shape_def = CheckShapeUpdateDef(L, shape_def_index);
        switch (shape_def.m_Type)
        {
            case B2DShapeDef::TYPE_CIRCLE:
                b2Shape_SetCircle(shape, &shape_def.m_Circle);
                break;
            case B2DShapeDef::TYPE_CAPSULE:
                b2Shape_SetCapsule(shape, &shape_def.m_Capsule);
                break;
            case B2DShapeDef::TYPE_SEGMENT:
                b2Shape_SetSegment(shape, &shape_def.m_Segment);
                break;
            case B2DShapeDef::TYPE_POLYGON:
                b2Shape_SetPolygon(shape, &shape_def.m_Polygon);
                break;
        }

        int update_mass_index = shape_def_index + 1;
        bool update_mass = lua_gettop(L) >= update_mass_index && !lua_isnil(L, update_mass_index) && lua_toboolean(L, update_mass_index);
        if (update_mass)
        {
            b2Body_ApplyMassFromShapes(b2Shape_GetBody(shape));
        }
        return 0;
    }

    static int Shape_IsSensor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_IsSensor(CheckShape(L, 1)));
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
        lua_pushnumber(L, b2Shape_GetDensity(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_SetDensity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1);
        int value_index = GetShapeValueIndex(L, 1);
        int update_mass_index = value_index + 1;
        bool update_mass = lua_gettop(L) >= update_mass_index && !lua_isnil(L, update_mass_index) && lua_toboolean(L, update_mass_index);
        b2Shape_SetDensity(shape, luaL_checknumber(L, value_index), update_mass);
        return 0;
    }

    static int Shape_GetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Shape_GetFriction(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_SetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_SetFriction(CheckShape(L, 1), luaL_checknumber(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_GetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, b2Shape_GetRestitution(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_SetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_SetRestitution(CheckShape(L, 1), luaL_checknumber(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_GetMaterial(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Shape_GetMaterial(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_SetMaterial(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_SetMaterial(CheckShape(L, 1), luaL_checkinteger(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_EnableSensorEvents(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_EnableSensorEvents(CheckShape(L, 1), lua_toboolean(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_AreSensorEventsEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_AreSensorEventsEnabled(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_EnableContactEvents(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_EnableContactEvents(CheckShape(L, 1), lua_toboolean(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_AreContactEventsEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_AreContactEventsEnabled(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_EnablePreSolveEvents(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_EnablePreSolveEvents(CheckShape(L, 1), lua_toboolean(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_ArePreSolveEventsEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_ArePreSolveEventsEnabled(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_EnableHitEvents(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Shape_EnableHitEvents(CheckShape(L, 1), lua_toboolean(L, GetShapeValueIndex(L, 1)));
        return 0;
    }

    static int Shape_AreHitEventsEnabled(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, b2Shape_AreHitEventsEnabled(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_GetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        CheckOptionalChildIndex(L, GetShapeValueIndex(L, 1));
        PushFilterData(L, b2Shape_GetFilter(shape));
        return 1;
    }

    static int Shape_SetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1);
        int filter_index = GetShapeValueIndex(L, 1);
        if (!lua_istable(L, filter_index))
        {
            CheckOptionalChildIndex(L, filter_index);
            ++filter_index;
        }
        b2Shape_SetFilter(shape, CheckFilterData(L, filter_index));
        return 0;
    }

    static int Shape_Refilter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2ShapeId shape = CheckShape(L, 1);
        b2Shape_SetFilter(shape, b2Shape_GetFilter(shape));
        return 0;
    }

    static int Shape_TestPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        b2Vec2 point = CheckVec2(L, GetShapeValueIndex(L, 1), GetPhysicsScale());
        lua_pushboolean(L, b2Shape_TestPoint(shape, point));
        return 1;
    }

    static int Shape_RayCast(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        int value_index = GetShapeValueIndex(L, 1);

        b2RayCastInput input = {};
        input.origin = CheckVec2(L, value_index, GetPhysicsScale());
        input.translation = CheckVec2(L, value_index + 1, GetPhysicsScale());
        input.maxFraction = lua_isnoneornil(L, value_index + 2) ? 1.0f : luaL_checknumber(L, value_index + 2);

        b2CastOutput output = b2Shape_RayCast(shape, &input);
        if (!output.hit)
        {
            lua_pushnil(L);
            return 1;
        }

        PushCastOutput(L, output);
        return 1;
    }

    static int Shape_GetAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        CheckOptionalChildIndex(L, GetShapeValueIndex(L, 1));
        b2AABB aabb = b2Shape_GetAABB(shape);

        lua_newtable(L);
        dmScript::PushVector3(L, FromB2(aabb.lowerBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "lower");

        dmScript::PushVector3(L, FromB2(aabb.upperBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "upper");
        return 1;
    }

    static int Shape_GetContactCapacity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Shape_GetContactCapacity(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_GetContactData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        int capacity = b2Shape_GetContactCapacity(shape);

        lua_newtable(L);
        if (capacity == 0)
        {
            return 1;
        }

        dmArray<b2ContactData> contact_data;
        contact_data.SetCapacity(capacity);
        contact_data.SetSize(capacity);
        int count = b2Shape_GetContactData(shape, contact_data.Begin(), capacity);
        for (int i = 0; i < count; ++i)
        {
            PushContactData(L, contact_data[i]);
            lua_rawseti(L, -2, i + 1);
        }
        return 1;
    }

    static int Shape_GetSensorCapacity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, b2Shape_GetSensorCapacity(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_GetSensorOverlaps(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        int capacity = b2Shape_GetSensorCapacity(shape);

        lua_newtable(L);
        if (capacity == 0)
        {
            return 1;
        }

        dmArray<b2ShapeId> overlaps;
        overlaps.SetCapacity(capacity);
        overlaps.SetSize(capacity);
        int count = b2Shape_GetSensorOverlaps(shape, overlaps.Begin(), capacity);
        int result_index = 1;
        for (int i = 0; i < count; ++i)
        {
            if (!b2Shape_IsValid(overlaps[i]))
            {
                continue;
            }

            b2BodyId body = b2Shape_GetBody(overlaps[i]);
            PushShapeInfo(L, overlaps[i], GetShapeIndex(body, overlaps[i]));
            lua_rawseti(L, -2, result_index);
            ++result_index;
        }
        return 1;
    }

    static int Shape_GetMassData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushMassData(L, b2Shape_GetMassData(CheckShape(L, 1)));
        return 1;
    }

    static int Shape_GetClosestPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2ShapeId shape = CheckShape(L, 1);
        b2Vec2 target = CheckVec2(L, GetShapeValueIndex(L, 1), GetPhysicsScale());
        dmScript::PushVector3(L, FromB2(b2Shape_GetClosestPoint(shape, target), GetInvPhysicsScale()));
        return 1;
    }

    static int Shape_tostring(lua_State *L)
    {
        B2DLuaShape* luashape = CheckShapeInternal(L, 1);
        lua_pushfstring(L, "Box2D.%s = %p", BOX2D_TYPE_NAME_SHAPE, &luashape->m_Shape);
        return 1;
    }

    static int Shape_eq(lua_State *L)
    {
        b2ShapeId* a = ToShapeId(L, 1);
        b2ShapeId* b = ToShapeId(L, 2);
        bool equal = false;
        if (a && b)
        {
            b2ShapeId shape_a = *a;
            b2ShapeId shape_b = *b;
            equal = B2_ID_EQUALS(shape_a, shape_b);
        }
        lua_pushboolean(L, equal);
        return 1;
    }

    static const luaL_reg Shape_methods[] =
    {
        {0,0}
    };

    static const luaL_reg Shape_meta[] =
    {
        {"__tostring", Shape_tostring},
        {"__eq", Shape_eq},
        {0,0}
    };

    static const luaL_reg Shape_functions[] =
    {
        {"is_valid", Shape_IsValid},
        {"get_shape", Shape_GetShape},
        {"set_shape", Shape_SetShape},
        {"get_body", Shape_GetBody},
        {"get_world", Shape_GetWorld},
        {"get_type", Shape_GetType},
        {"is_sensor", Shape_IsSensor},
        {"set_sensor", Shape_SetSensor},
        {"get_density", Shape_GetDensity},
        {"set_density", Shape_SetDensity},
        {"get_friction", Shape_GetFriction},
        {"set_friction", Shape_SetFriction},
        {"get_restitution", Shape_GetRestitution},
        {"set_restitution", Shape_SetRestitution},
        {"get_material", Shape_GetMaterial},
        {"set_material", Shape_SetMaterial},
        {"enable_sensor_events", Shape_EnableSensorEvents},
        {"are_sensor_events_enabled", Shape_AreSensorEventsEnabled},
        {"enable_contact_events", Shape_EnableContactEvents},
        {"are_contact_events_enabled", Shape_AreContactEventsEnabled},
        {"enable_pre_solve_events", Shape_EnablePreSolveEvents},
        {"are_pre_solve_events_enabled", Shape_ArePreSolveEventsEnabled},
        {"enable_hit_events", Shape_EnableHitEvents},
        {"are_hit_events_enabled", Shape_AreHitEventsEnabled},
        {"get_filter_data", Shape_GetFilterData},
        {"set_filter_data", Shape_SetFilterData},
        {"refilter", Shape_Refilter},
        {"test_point", Shape_TestPoint},
        {"ray_cast", Shape_RayCast},
        {"get_aabb", Shape_GetAABB},
        {"get_contact_capacity", Shape_GetContactCapacity},
        {"get_contact_data", Shape_GetContactData},
        {"get_sensor_capacity", Shape_GetSensorCapacity},
        {"get_sensor_overlaps", Shape_GetSensorOverlaps},
        {"get_mass_data", Shape_GetMassData},
        {"get_closest_point", Shape_GetClosestPoint},
        {0,0}
    };

    void ScriptBox2DInitializeShape(lua_State* L)
    {
        TYPE_HASH_SHAPE = dmScript::RegisterUserType(L, BOX2D_TYPE_NAME_SHAPE, Shape_methods, Shape_meta);

        lua_newtable(L);
        luaL_register(L, 0, Shape_functions);

#define SET_CONSTANT(NAME, CUSTOM_NAME) \
        lua_pushnumber(L, (lua_Number) NAME); \
        lua_setfield(L, -2, CUSTOM_NAME);

        SET_CONSTANT(b2_circleShape, "SHAPE_TYPE_CIRCLE");
        SET_CONSTANT(b2_capsuleShape, "SHAPE_TYPE_CAPSULE");
        SET_CONSTANT(SHAPE_TYPE_SEGMENT, "SHAPE_TYPE_SEGMENT");
        SET_CONSTANT(SHAPE_TYPE_EDGE, "SHAPE_TYPE_EDGE");
        SET_CONSTANT(b2_polygonShape, "SHAPE_TYPE_POLYGON");
        SET_CONSTANT(SHAPE_TYPE_BOX, "SHAPE_TYPE_BOX");

#undef SET_CONSTANT

        lua_setfield(L, -2, "shape");
    }

    void ScriptBox2DFinalizeShape()
    {
        TYPE_HASH_SHAPE = 0;
    }
}

/*# Box2D b2Shape documentation
 *
 * Constants and functions for Box2D v3 shape tables used with
 * `b2d.body.create_shape` and returned from `b2d.shape.get_shape`.
 * Shape functions accept either a `b2Shape` handle as the first argument or
 * the legacy `body, shape_index` argument pair.
 *
 * @document
 * @name b2d.shape
 * @namespace b2d.shape
 * @language Lua
 */

/*# Box2D shape
 *
 * An opaque handle to one collision shape attached to a [type:b2Body]. Obtain
 * shape handles from [ref:b2d.body.get_shapes] or when creating shapes, then use
 * the functions in `b2d.shape` to inspect or modify them. A shape is owned by its
 * body and its handle becomes invalid when the shape or body is destroyed.
 *
 * @typedef
 * @name b2Shape
 * @param value [type:userdata] Box2D shape handle
 * @examples
 *
 * ```lua
 * local body = b2d.get_body("#collisionobject")
 * local shapes = b2d.body.get_shapes(body)
 * local shape = shapes[1].shape_id
 * pprint(b2d.shape.get_shape(shape))
 * ```
 */

/*# Get a shape's geometry.
 * @name b2d.shape.get_shape
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return shape [type:b2d.shape.definition] shape table with numeric `type` from `b2d.shape.SHAPE_TYPE_*`
 */

/*# Set a shape's geometry.
 * This updates the shape geometry using the same table format as
 * `b2d.body.create_shape` and `b2d.shape.get_shape`. The body mass is not
 * updated unless `update_mass` is true.
 * @warning This function is locked during callbacks.
 * @name b2d.shape.set_shape
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param definition [type:b2d.shape.definition] shape table with numeric `type` from `b2d.shape.SHAPE_TYPE_*`
 * @param update_mass [type: boolean] true to reset body mass from shapes
 * @examples
 *
 * ```lua
 * local body = b2d.get_body("#collisionobject")
 *
 * -- Move a circle shape relative to the body origin.
 * local circle = b2d.shape.get_shape(body, 1)
 * circle.center = vmath.vector3(24, 0, 0)
 * b2d.shape.set_shape(body, 1, circle, true)
 *
 * -- Replace a segment shape's local endpoints.
 * b2d.shape.set_shape(body, 2, {
 *     type = b2d.shape.SHAPE_TYPE_SEGMENT,
 *     v1 = vmath.vector3(-32, 0, 0),
 *     v2 = vmath.vector3( 32, 0, 0),
 * })
 *
 * -- Update a box shape using the polygon box convenience format.
 * b2d.shape.set_shape(body, 3, {
 *     type = b2d.shape.SHAPE_TYPE_BOX,
 *     hx = 16,
 *     hy = 8,
 *     center = vmath.vector3(0, 20, 0),
 *     angle = math.rad(30),
 * }, true)
 * ```
 */

/*# Circle shape type.
 * @name b2d.shape.SHAPE_TYPE_CIRCLE
 * @constant
 */

/*# Capsule shape type.
 * @name b2d.shape.SHAPE_TYPE_CAPSULE
 * @constant
 */

/*# Segment shape type.
 * @name b2d.shape.SHAPE_TYPE_SEGMENT
 * @constant
 */

/*# Edge shape type alias.
 * Compatibility alias for `b2d.shape.SHAPE_TYPE_SEGMENT`.
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

/*# Get shape material id.
 * @name b2d.shape.get_material
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return material [type: integer] shape material id
 */

/*# Set shape material id.
 * @warning This function is locked during callbacks.
 * @name b2d.shape.set_material
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param material [type: integer] shape material id
 */

/*# Validate a shape handle.
 * @name b2d.shape.is_valid
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return valid [type: boolean] true if the shape handle still refers to a live Box2D shape
 */

/*# Get the body owning a shape.
 * @name b2d.shape.get_body
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return body [type: b2Body] owning body
 */

/*# Get the world owning a shape.
 * @name b2d.shape.get_world
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return world [type: b2World] owning world
 */

/*# Enable or disable sensor events for a shape.
 * @name b2d.shape.enable_sensor_events
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param enable [type: boolean] true to enable sensor events
 */

/*# Check if sensor events are enabled for a shape.
 * @name b2d.shape.are_sensor_events_enabled
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return enabled [type: boolean] true if sensor events are enabled
 */

/*# Enable or disable contact events for a shape.
 * @name b2d.shape.enable_contact_events
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param enable [type: boolean] true to enable contact events
 */

/*# Check if contact events are enabled for a shape.
 * @name b2d.shape.are_contact_events_enabled
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return enabled [type: boolean] true if contact events are enabled
 */

/*# Enable or disable pre-solve events for a shape.
 * @name b2d.shape.enable_pre_solve_events
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param enable [type: boolean] true to enable pre-solve events
 */

/*# Check if pre-solve events are enabled for a shape.
 * @name b2d.shape.are_pre_solve_events_enabled
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return enabled [type: boolean] true if pre-solve events are enabled
 */

/*# Enable or disable hit events for a shape.
 * @name b2d.shape.enable_hit_events
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param enable [type: boolean] true to enable hit events
 */

/*# Check if hit events are enabled for a shape.
 * @name b2d.shape.are_hit_events_enabled
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return enabled [type: boolean] true if hit events are enabled
 */

/*# Ray cast a shape directly.
 * @name b2d.shape.ray_cast
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param origin [type: vector3] world ray origin
 * @param translation [type: vector3] world ray translation
 * @param [max_fraction] [type:number] optional maximum translation fraction, defaults to 1
 * @return hit [type:b2d.shape_cast_output|nil] hit table with `point`, `normal`, `fraction`, and `iterations`, or nil
 */

/*# Get shape contact capacity.
 * @name b2d.shape.get_contact_capacity
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return capacity [type: integer] maximum contact data count
 */

/*# Get touching contact data for a shape.
 * @name b2d.shape.get_contact_data
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return contacts [type:b2d.contact_data[]] array of contact tables
 */

/*# Get sensor overlap capacity.
 * @name b2d.shape.get_sensor_capacity
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return capacity [type: integer] maximum sensor overlap count
 */

/*# Get sensor overlaps.
 * @name b2d.shape.get_sensor_overlaps
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return overlaps [type:b2d.shape_info[]] array of shape info tables
 */

/*# Get mass data for a shape.
 * @name b2d.shape.get_mass_data
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @return data [type:b2d.mass_data] table with `mass`, `center`, and `inertia`
 */

/*# Get the closest point on a shape.
 * @name b2d.shape.get_closest_point
 * @param shape_id [type: b2Shape] shape handle from a shape info table, or pass `body, shape_index`
 * @param target [type: vector3] world target point
 * @return point [type: vector3] closest world point on the shape
 */
