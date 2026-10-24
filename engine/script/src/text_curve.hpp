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

#ifndef DM_SCRIPT_TEXT_CURVE_HPP
#define DM_SCRIPT_TEXT_CURVE_HPP

#include "script.h"
#include <font/text_curve.hpp>

namespace dmScript
{
    // Shared label/GUI argument reader. Indices must be positive. Returns one
    // owned reference, or null for nil. Raises a Lua error on invalid input
    // without retaining Lua data or leaking the temporary knot buffer.
    HFontTextCurve CheckTextCurve(lua_State* L, int knots_index, int options_index);
} // namespace dmScript

#endif // DM_SCRIPT_TEXT_CURVE_HPP
