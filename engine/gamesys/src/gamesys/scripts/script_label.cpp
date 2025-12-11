// Copyright 2020-2025 The Defold Foundation
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


#include <dlib/log.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <render/font/fontmap.h>
#include <render/render.h>
#include <script/script.h>
#include <dmsdk/gamesys/script.h>

#include "script_label.h"
#include "../components/comp_label.h"
#include <gamesys/gamesys_ddf.h>
#include <gamesys/label_ddf.h>

namespace dmGameSystem
{
/*# Label API documentation
 *
 * Functions to manipulate a label component.
 *
 * @document
 * @name Label
 * @namespace label
 * @language Lua
 */

/*# [type:vector4] label color
 *
 * The color of the label. The type of the property is vector4.
 *
 * @name color
 * @property
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *    -- Get the current color's y component
 *    local red_component = go.get("#label", "color.y")
 *    -- Animate the color
 *    go.animate("#label", "color", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * ```
 */

/*# [type:vector4] label outline
 *
 * The outline color of the label. The type of the property is vector4.
 *
 * @name outline
 * @property
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *    -- Get the current outline color
 *    local outline = go.get("#label", "outline")
 *    -- Animate the property
 *    go.animate("#label", "outline", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * ```
 */

/*# [type:vector4] label shadow
 *
 * The shadow color of the label. The type of the property is vector4.
 *
 * @name shadow
 * @property
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *  -- Get the current shadow color
 *  local shadow = go.get("#label", "shadow")
 *  -- Animate the property
 *  go.animate("#label", "shadow", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * ```
 */

/*# [type:number|vector3] label scale
 *
 * The scale of the label. The type of the property is number (uniform)
 * or vector3 (non uniform).
 *
 * @name scale
 * @property
 *
 * @examples
 *
 * How to scale a label independently along the X and Y axis:
 *
 * ```lua
 * function init(self)
 *    -- Double the y-axis scaling on component "label"
 *    local yscale = go.get("#label", "scale.y")
 *    go.set("#label", "scale.y", yscale * 2)
 *    -- Set the new scale altogether
 *    go.set("#label", "scale", vmath.vector3(2,2,2))
 *    -- Animate the scale
 *    go.animate("#label", "scale", go.PLAYBACK_LOOP_PINGPONG, vmath.vector3(2,2,2), go.EASING_INOUTSINE, 1)
 * end
 * ```
 */

/*# [type:vector3] label size
 *
 * Returns the size of the label. The size will constrain the text if line break is enabled.
 * The type of the property is vector3.
 *
 * @name size
 * @property
 *
 * @examples
 *
 * How to query a label's size, either as a vector or selecting a specific dimension:
 *
 * ```lua
 * function init(self)
 *  -- get size from component "label"
 *  local size = go.get("#label", "size")
 *  local sizex = go.get("#label", "size.x")
 *  -- do something useful
 *  assert(size.x == sizex)
 * end
 * ```
 */

/*# [type:hash] label material
 *
 * The material used when rendering the label. The type of the property is hash.
 *
 * @name material
 * @property
 *
 * @examples
 *
 * How to set material using a script property (see [ref:resource.material])
 *
 * ```lua
 * go.property("my_material", resource.material("/material.material"))
 *
 * function init(self)
 *   go.set("#label", "material", self.my_material)
 * end
 * ```
 */

/*# [type:hash] label font
 *
 * The font used when rendering the label. The type of the property is hash.
 *
 * @name font
 * @property
 *
 * @examples
 *
 * How to set font using a script property (see [ref:resource.font])
 *
 * ```lua
 * go.property("my_font", resource.font("/font.font"))
 *
 * function init(self)
 *   go.set("#label", "font", self.my_font)
 * end
 * ```
 */

/*# [type:number] label leading
 *
 * The leading of the label. This value is used to scale the line spacing of text.
 * The type of the property is number.
 *
 * @name leading
 * @property
 *
 * @examples
 *
 * How to query a label's leading:
 *
 * ```lua
 * function init(self)
 *  -- get leading from component "label"
 *  local leading = go.get("#label", "leading")
 *  -- do something useful
 *  leading = leading * 1.2
 *  go.set("#label", "leading", leading)
 * end
 * ```
 */

/*# [type:number] label tracking
 *
 * The tracking of the label.
 * This value is used to adjust the vertical spacing of characters in the text.
 * The type of the property is number.
 *
 * @name tracking
 * @property
 *
 * @examples
 *
 * How to query a label's tracking:
 *
 * ```lua
 * function init(self)
 *  -- get tracking from component "label"
 *  local tracking = go.get("#label", "tracking")
 *  -- do something useful
 *  tracking = tracking * 1.2
 *  go.set("#label", "tracking", tracking)
 * end
 * ```
 */

/*# [type:boolean] label line break
 *
 * The line break of the label.
 * This value is used to adjust the vertical spacing of characters in the text.
 * The type of the property is boolean.
 *
 * @name line_break
 * @property
 *
 * @examples
 *
 * How to query a label's line break:
 *
 * ```lua
 * function init(self)
 *  -- get line_break from component "label"
 *  local line_break = go.get("#label", "line_break")
 *  -- do something useful
 *  go.set("#label", "line_break", false)
 * end
 * ```
 */

// As seen in gamesys_private.h (which makes it a _lot_ harder to search for)
static const char* LABEL_EXT = "labelc";

/*# set the text for a label
 *
 * Sets the text of a label component
 *
 * [icon:attention] This method uses the message passing that means the value will be set after `dispatch messages` step.
 * More information is available in the <a href="/manuals/application-lifecycle">Application Lifecycle manual</a>.
 *
 * @name label.set_text
 * @param url [type:string|hash|url] the label that should have a constant set
 * @param text [type:string|number] the text
 * @examples
 *
 * ```lua
 * function init(self)
 *     label.set_text("#label", "Hello World!")
 * end
 * ```
 */
static int SetText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

    size_t text_len = 0;
    const char* text = luaL_checklstring(L, 2, &text_len);
    if (!text)
    {
        return DM_LUA_ERROR("Expected string as second argument");
    }

    uint32_t data_size = sizeof(dmGameSystemDDF::SetText) + text_len + 1;
    if (data_size > dmMessage::DM_MESSAGE_MAX_DATA_SIZE)
    {
        return DM_LUA_ERROR("The label string is too long: %u (max is message size %u)", data_size, dmMessage::DM_MESSAGE_MAX_DATA_SIZE);
    }
    uint8_t data[dmMessage::DM_MESSAGE_MAX_DATA_SIZE];

    dmGameSystemDDF::SetText* message = (dmGameSystemDDF::SetText*)data;
    message->m_Text = (const char*)sizeof(dmGameSystemDDF::SetText);
    memcpy((void*)(data + sizeof(dmGameSystemDDF::SetText)), text, text_len + 1);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::GetURL(L, &sender);
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    if (dmMessage::RESULT_OK != dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetText::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::SetText::m_DDFDescriptor, data, data_size, 0) )
    {
        return DM_LUA_ERROR("Failed to send label string as message!");
    }
    return 0;
}


/** DEPRECATED
 */
static int GetTextMetrics(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    CheckGoInstance(L);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    dmGameSystem::LabelComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, LABEL_EXT, 0, (dmGameObject::HComponent*)&component, 0);

    assert(component != 0);

    dmRender::TextMetrics metrics;
    dmGameSystem::CompLabelGetTextMetrics(component, metrics);

    lua_createtable(L, 0, 4);
    lua_pushliteral(L, "width");
    lua_pushnumber(L, metrics.m_Width);
    lua_rawset(L, -3);
    lua_pushliteral(L, "height");
    lua_pushnumber(L, metrics.m_Height);
    lua_rawset(L, -3);
    lua_pushliteral(L, "max_ascent");
    lua_pushnumber(L, metrics.m_MaxAscent);
    lua_rawset(L, -3);
    lua_pushliteral(L, "max_descent");
    lua_pushnumber(L, metrics.m_MaxDescent);
    lua_rawset(L, -3);

    return 1;
}

/*# gets the text for a label
 *
 * Gets the text from a label component
 *
 * @name label.get_text
 * @param url [type:string|hash|url] the label to get the text from
 * @return metrics [type:string] the label text
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *     local text = label.get_text("#label")
 *     print(text)
 * end
 * ```
 */
static int GetText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    CheckGoInstance(L);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    dmGameSystem::LabelComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, LABEL_EXT, 0, (dmGameObject::HComponent*)&component, 0);

    const char* value = dmGameSystem::CompLabelGetText(component);
    lua_pushstring(L, value);

    return 1;
}

static const luaL_reg Module_methods[] =
{
    {"set_text", SetText},
    {"get_text", GetText},
    {"get_text_metrics", GetTextMetrics},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    luaL_register(L, "label", Module_methods);
    lua_pop(L, 1);
}

void ScriptLabelRegister(const ScriptLibContext& context)
{
    LuaInit(context.m_LuaState);
}

void ScriptLabelFinalize(const ScriptLibContext& context)
{
}

}
