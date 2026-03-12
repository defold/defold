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

#include <Box2D/Dynamics/b2Fixture.h>

#include <script/script.h>
#include <gameobject/script.h>

#include "script_box2d_v2.h"

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
        return b2Vec2(v->getX() * scale, v->getY() * scale);
    }

    static dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale)
    {
        return dmVMath::Vector3(p.x * inv_scale, p.y * inv_scale, 0);
    }

    static b2Filter CheckFilterData(lua_State* L, int index)
    {
        luaL_checktype(L, index, LUA_TTABLE);

        b2Filter filter = {};

        lua_getfield(L, index, "category_bits");
        filter.categoryBits = (uint16)luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "mask_bits");
        filter.maskBits = (uint16)luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, index, "group_index");
        filter.groupIndex = (int16)luaL_checkinteger(L, -1);
        lua_pop(L, 1);

        return filter;
    }

    static void PushFilterData(lua_State* L, const b2Filter& filter)
    {
        lua_newtable(L);

        lua_pushinteger(L, filter.categoryBits);
        lua_setfield(L, -2, "category_bits");

        lua_pushinteger(L, filter.maskBits);
        lua_setfield(L, -2, "mask_bits");

        lua_pushinteger(L, filter.groupIndex);
        lua_setfield(L, -2, "group_index");
    }

    static int CheckChildIndex(lua_State* L, b2Fixture* fixture, int index)
    {
        int child_index = luaL_checkinteger(L, index);
        if (child_index <= 0)
        {
            luaL_error(L, "child_index must be >= 1.");
            return 0;
        }

        const int child_count = fixture->GetShape()->GetChildCount();
        if (child_index > child_count)
        {
            luaL_error(L, "child_index %d out of range [1, %d].", child_index, child_count);
            return 0;
        }
        return child_index - 1;
    }

    static int Fixture_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        lua_pushinteger(L, fixture->GetType());
        return 1;
    }

    static int Fixture_IsSensor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        lua_pushboolean(L, fixture->IsSensor());
        return 1;
    }

    static int Fixture_SetSensor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        fixture->SetSensor(lua_toboolean(L, 3) != 0);
        return 0;
    }

    static int Fixture_GetDensity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        lua_pushnumber(L, fixture->GetDensity());
        return 1;
    }

    static int Fixture_SetDensity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        fixture->SetDensity(luaL_checknumber(L, 3));
        if (lua_gettop(L) >= 4 && !lua_isnil(L, 4) && lua_toboolean(L, 4))
        {
            fixture->GetBody()->ResetMassData();
        }
        return 0;
    }

    static int Fixture_GetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        lua_pushnumber(L, fixture->GetFriction());
        return 1;
    }

    static int Fixture_SetFriction(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        fixture->SetFriction(luaL_checknumber(L, 3));
        return 0;
    }

    static int Fixture_GetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        lua_pushnumber(L, fixture->GetRestitution());
        return 1;
    }

    static int Fixture_SetRestitution(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        fixture->SetRestitution(luaL_checknumber(L, 3));
        return 0;
    }

    static int Fixture_GetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        int child_index = CheckChildIndex(L, fixture, 3);
        PushFilterData(L, fixture->GetFilterData(child_index));
        return 1;
    }

    static int Fixture_SetFilterData(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        int child_index = CheckChildIndex(L, fixture, 3);
        b2Filter filter = CheckFilterData(L, 4);
        fixture->SetFilterData(filter, child_index);
        return 0;
    }

    static int Fixture_Refilter(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        fixture->Refilter(lua_toboolean(L, 3) != 0);
        return 0;
    }

    static int Fixture_TestPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        b2Vec2 point = CheckVec2(L, 3, GetPhysicsScale());
        lua_pushboolean(L, fixture->TestPoint(point));
        return 1;
    }

    static int Fixture_GetAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        b2Body* body = CheckBody(L, 1);
        int fixture_index = luaL_checkinteger(L, 2);
        b2Fixture* fixture = GetFixtureByIndex(body, fixture_index);
        if (!fixture)
        {
            return luaL_error(L, "fixture_index %d out of range.", fixture_index);
        }
        int child_index = CheckChildIndex(L, fixture, 3);
        const b2AABB& aabb = fixture->GetAABB(child_index);

        lua_newtable(L);
        dmScript::PushVector3(L, FromB2(aabb.lowerBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "lower");

        dmScript::PushVector3(L, FromB2(aabb.upperBound, GetInvPhysicsScale()));
        lua_setfield(L, -2, "upper");
        return 1;
    }

    static const luaL_reg Fixture_functions[] =
    {
        {"get_type", Fixture_GetType},
        {"is_sensor", Fixture_IsSensor},
        {"set_sensor", Fixture_SetSensor},
        {"get_density", Fixture_GetDensity},
        {"set_density", Fixture_SetDensity},
        {"get_friction", Fixture_GetFriction},
        {"set_friction", Fixture_SetFriction},
        {"get_restitution", Fixture_GetRestitution},
        {"set_restitution", Fixture_SetRestitution},
        {"get_filter_data", Fixture_GetFilterData},
        {"set_filter_data", Fixture_SetFilterData},
        {"refilter", Fixture_Refilter},
        {"test_point", Fixture_TestPoint},
        {"get_aabb", Fixture_GetAABB},
        {0,0}
    };

    void ScriptBox2DInitializeFixture(struct lua_State* L)
    {
        lua_newtable(L);
        luaL_register(L, 0, Fixture_functions);
        lua_setfield(L, -2, "fixture");
    }
}

/*# Box2D b2Fixture documentation
 *
 * Functions for interacting with fixtures attached to Box2D bodies.
 * Fixtures are addressed functionally by `(body, fixture_index)` rather than persistent Lua handles.
 *
 * @document
 * @name b2d.fixture
 * @namespace b2d.fixture
 * @language Lua
 */

/*# Get the fixture type.
 * @name b2d.fixture.get_type
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @return type [type: number]
 */

/*# Check if a fixture is a sensor.
 * @name b2d.fixture.is_sensor
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @return enabled [type: boolean]
 */

/*# Set sensor mode for a fixture.
 * @name b2d.fixture.set_sensor
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param enabled [type: boolean]
 */

/*# Get fixture density.
 * @name b2d.fixture.get_density
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @return density [type: number] density in kg/m^2
 */

/*# Set fixture density.
 * @name b2d.fixture.set_density
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param density [type: number] density in kg/m^2
 * @param update_mass [type: boolean] if true, reset body mass data after the change
 */

/*# Get fixture friction.
 * @name b2d.fixture.get_friction
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @return friction [type: number]
 */

/*# Set fixture friction.
 * @name b2d.fixture.set_friction
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param friction [type: number]
 */

/*# Get fixture restitution.
 * @name b2d.fixture.get_restitution
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @return restitution [type: number]
 */

/*# Set fixture restitution.
 * @name b2d.fixture.set_restitution
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param restitution [type: number]
 */

/*# Get fixture filter data for a child shape.
 * @name b2d.fixture.get_filter_data
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param child_index [type: number] 1-based child shape index
 * @return filter [type: table] table with `category_bits`, `mask_bits`, and `group_index`
 */

/*# Set fixture filter data for a child shape.
 * @name b2d.fixture.set_filter_data
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param child_index [type: number] 1-based child shape index
 * @param filter [type: table] table with `category_bits`, `mask_bits`, and `group_index`
 */

/*# Refilter a fixture.
 * @name b2d.fixture.refilter
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param touch_proxies [type: boolean] if true, touch broad-phase proxies
 */

/*# Test a point against a fixture.
 * @name b2d.fixture.test_point
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param point [type: vector3] point in world coordinates
 * @return hit [type: boolean]
 */

/*# Get fixture AABB for a child shape.
 * @name b2d.fixture.get_aabb
 * @param body [type: b2Body] body
 * @param fixture_index [type: number] 1-based fixture index from `b2d.body.get_fixtures`
 * @param child_index [type: number] 1-based child shape index
 * @return aabb [type: table] table with `lower` and `upper`
 */
