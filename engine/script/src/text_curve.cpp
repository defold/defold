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

#include "text_curve.hpp"
#include <math.h>

namespace dmScript
{
    static bool ReadVector(lua_State* L, const char* name, dmVMath::Vector3* value, bool required, bool* present)
    {
        lua_getfield(L, -1, name);
        *present = !lua_isnil(L, -1);
        bool valid = !required;
        *value = dmVMath::Vector3(0.0f);
        if (*present)
        {
            dmVMath::Vector3* vector = dmScript::ToVector3(L, -1);
            if (vector)
            {
                *value = *vector;
                valid = true;
            }
            else if (lua_istable(L, -1))
            {
                valid = true;
                for (int i = 0; i < 3; ++i)
                {
                    lua_rawgeti(L, -1, i + 1);
                    valid = valid && lua_isnumber(L, -1);
                    value->setElem(i, (float)lua_tonumber(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
                valid = false;
        }
        lua_pop(L, 1);
        return valid && isfinite(value->getX()) && isfinite(value->getY()) && isfinite(value->getZ());
    }

    static bool ReadNumber(lua_State* L, const char* name, float default_value, float* value)
    {
        lua_getfield(L, -1, name);
        const bool valid = lua_isnil(L, -1) || lua_isnumber(L, -1);
        *value = lua_isnil(L, -1) ? default_value : (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
        return valid && isfinite(*value);
    }

    static bool ReadKnot(lua_State* L, FontTextCurveKnot* knot)
    {
        if (!lua_istable(L, -1))
            return false;
        bool present;
        if (!ReadVector(L, "position", &knot->m_Position, true, &present) ||
            !ReadVector(L, "in_handle", &knot->m_InHandle, false, &present) ||
            !ReadVector(L, "out_handle", &knot->m_OutHandle, false, &present))
            return false;
        return ReadNumber(L, "roll_degrees", 0.0f, &knot->m_RollDegrees) &&
        ReadNumber(L, "scale", 1.0f, &knot->m_Scale) && knot->m_Scale > 0.0f;
    }

    HFontTextCurve CheckTextCurve(lua_State* L, int knots_index, int options_index)
    {
        if (lua_isnoneornil(L, knots_index))
            return 0;
        luaL_checktype(L, knots_index, LUA_TTABLE);
        const size_t count = lua_objlen(L, knots_index);
        if (count < 2 || count > UINT32_MAX / 64 - 1)
            luaL_error(L, "A text curve requires at least two knots");
        bool closed = false;
        if (!lua_isnoneornil(L, options_index))
        {
            luaL_checktype(L, options_index, LUA_TTABLE);
            lua_getfield(L, options_index, "closed");
            if (!lua_isnil(L, -1) && !lua_isboolean(L, -1))
                luaL_error(L, "curve option 'closed' must be a boolean");
            closed = lua_toboolean(L, -1);
            lua_pop(L, 1);
        }
        // Lua owns the temporary buffer, including when a field getter errors.
        FontTextCurveKnot* knots = (FontTextCurveKnot*)lua_newuserdata(L, sizeof(FontTextCurveKnot) * count);
        dmVMath::Vector3   up;
        for (uint32_t i = 0; i < count; ++i)
        {
            lua_rawgeti(L, knots_index, i + 1);
            if (!ReadKnot(L, &knots[i]))
                luaL_error(L, "Invalid text curve knot %u: expected finite position, handles, roll_degrees and positive scale", i + 1);
            if (i == 0)
            {
                bool present;
                if (!ReadVector(L, "up", &up, false, &present))
                    luaL_error(L, "The first curve knot's up must be a finite, nonzero vector");
                if (!present)
                    up = dmVMath::Vector3(0, 1, 0);
            }
            else
            {
                lua_getfield(L, -1, "up");
                if (!lua_isnil(L, -1))
                    luaL_error(L, "up is only supported on the first curve knot; use roll_degrees on later knots");
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        HFontTextCurve curve = FontTextCurveCreate(knots, (uint32_t)count, closed, up);
        lua_pop(L, 1);
        if (!curve)
            luaL_error(L, "Text curve must have nonzero length and a finite, nonzero initial up vector");
        return curve;
    }
} // namespace dmScript
