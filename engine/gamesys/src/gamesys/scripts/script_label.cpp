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


#include <dlib/log.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <render/font/fontmap.h>
#include <render/render.h>
#include <script/script.h>
#include <script/text_curve.hpp>
#include <dmsdk/gamesys/script.h>

#include "script_label.h"
#include "../components/comp_label.h"
#include <gamesys/gamesys_ddf.h>
#include <gamesys/label_ddf.h>

namespace dmGameSystem
{
static const dmhash_t TAG_SPRITE = dmHashString64("sprite");

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

/*# [type:number] label arc radius
 *
 * Signed radius of the first text baseline in local units, default 100.
 * The arc is centered on the component origin. Later lines retain their layout baseline spacing.
 *
 * @name arc_radius
 * @property
 * @examples
 * ```lua
 * go.set("#label", "arc_radius", 200)
 * local radius = go.get("#label", "arc_radius")
 * ```
 */

/*# [type:number] label arc angle
 *
 * Angular span of the text block, between 0 and 360 degrees, inclusive.
 * 360 forms a full circle. Defaults to 0, which selects ordinary textbox layout. Arc layout ignores size. When
 * line_break is enabled, text wraps at the arc length (absolute radius times the absolute
 * angle in radians). Explicit newlines and glyph-cluster size are preserved.
 *
 * @name arc_angle
 * @property
 * @examples
 * ```lua
 * go.set("#label", "arc_angle", 360)
 * local angle = go.get("#label", "arc_angle")
 * ```
 */

/*# [type:boolean] label line break
 *
 * The line break of the label.
 * In arc layout, automatic wrapping uses the arc length instead of size.x.
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

/*# [type:string] label text
 *
 * The text of the label.
 *
 * @name text
 * @property
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *     go.set("#label", "text", "Hello World!")
 *     local text = go.get("#label", "text")
 * end
 * ```
 */

/*# set the text for a label
 *
 * Sets the text of a label component
 *
 * [icon:attention] This function is deprecated. Use `go.set("#label", "text", value)` instead.
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
 *     go.set("#label", "text", "Hello World!")
 * end
 * ```
 */
static int SetText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmLogOnceWarning(dmScript::DEPRECATION_FUNCTION_FMT, "label", "set_text", "go", "set");

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

/*# gets the text for a label
 *
 * Gets the text from a label component
 *
 * [icon:attention] This function is deprecated. Use `go.get("#label", "text")` instead.
 *
 * @name label.get_text
 * @param url [type:string|hash|url] the label to get the text from
 * @return text [type:string] the label text
 *
 * @examples
 *
 * ```lua
 * function init(self)
 *     local text = go.get("#label", "text")
 *     print(text)
 * end
 * ```
 */
static int GetText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmLogOnceWarning(dmScript::DEPRECATION_FUNCTION_FMT, "label", "get_text", "go", "get");

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

/*# sets a text curve
 * Maps each glyph cluster onto an ordered cubic Bezier chain in local space.
 * All text lines use the same curve, offset by their baseline distance along up.
 * Each knot has position, optional in_handle/out_handle offsets, roll_degrees
 * (default zero), and positive uniform scale (default one).
 * Only the first knot accepts up, the initial direction to transport along the
 * curve. It defaults to {0, 1, 0}; its length does not affect scale. Vectors can
 * be vector3 values or {x, y, z} tables. Handles default to zero offsets.
 * A curve overrides arc and size. Line Break wraps at the curve's length.
 * Open endpoints extrapolate their tangents. Passing nil restores arc/textbox mapping.
 * @name label.set_curve
 * @param url [type:string|hash|url] label component
 * @param knots [type:{ position:vector3|number[], in_handle?:vector3|number[], out_handle?:vector3|number[], up?:vector3|number[], roll_degrees?:number, scale?:number }[]|nil] ordered array of at least two knots, or nil to clear
 * @param [options] [type:{ closed?:boolean }] optional table with closed (boolean, default false)
 * @examples
 * ```lua
 * label.set_curve("#label", {
 *     {position = {0, 0, 0}, out_handle = {80, 80, 0}, up = {0, 1, 0}},
 *     {position = {240, 0, 0}, in_handle = {-80, -80, 0}, roll_degrees = 30, scale = 1.2},
 * })
 * ```
 */
static int SetCurve(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);
    CheckGoInstance(L);
    dmGameSystem::LabelComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, LABEL_EXT, 0, (dmGameObject::HComponent*)&component, 0);
    HFontTextCurve curve = dmScript::CheckTextCurve(L, 2, 3);
    dmGameSystem::CompLabelSetCurve(component, curve);
    FontTextCurveRelease(curve);
    return 0;
}

static const char* GetLayoutObjectTagName(dmhash_t tag)
{
    return tag == TAG_SPRITE ? "sprite" : "link";
}

/*# Rich-text layout object
 *
 * @struct
 * @name label.layout_object
 * @member type [type:string] object type, currently `link` or `sprite`
 * @member id [type:hash] the object's `id` attribute, or its generated layout object id
 * @member text_offset [type:integer] zero-based UTF-32 offset in the visible text
 * @member text_length [type:integer] visible UTF-32 text length covered by the object
 * @member x [type:number] local lower-left x-coordinate after arc/curve mapping
 * @member y [type:number] local lower-left y-coordinate after arc/curve mapping
 * @member z [type:number] local lower-left z-coordinate after curve mapping; zero for flat text and arcs
 * @member width [type:number] resolved object width
 * @member height [type:number] resolved object height
 * @member attributes [type:table<string, string>] markup attributes keyed by name
 */

/*# gets the markup objects for a label
 *
 * Returns the sprites and links found in the label's current layout.
 * Each entry contains `type`, `id`, the zero-based UTF-32 `text_offset`,
 * `text_length`, resolved `x`, `y`, `z`, `width`
 * and `height`, and an `attributes` table. The position is the lower-left
 * object corner after arc/curve mapping, before applying component scale and rotation.
 * Flat text coordinates remain relative to the label's pivot. Width and height are layout dimensions.
 * Inline resource rendering is not part of this MVP; sprites use their explicit
 * dimensions or a one-em square fallback.
 *
 * @name label.get_layout_objects
 * @param url [type:string|hash|url] the label to inspect
 * @return objects [type:label.layout_object[]] layout objects in source order
 * @examples
 *
 * ```lua
 * local objects = label.get_layout_objects("#label")
 * for _, object in ipairs(objects) do
 *     if object.type == "link" then
 *         print(object.attributes.src, object.text_offset, object.text_length)
 *     elseif object.type == "sprite" then
 *         print(object.attributes.src, object.x, object.y, object.width, object.height)
 *     end
 * end
 * ```
 */
static int GetLayoutObjects(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    CheckGoInstance(L);
    dmGameSystem::LabelComponent* component = 0;
    dmScript::GetComponentFromLua(L, 1, LABEL_EXT, 0, (dmGameObject::HComponent*)&component, 0);

    HTextLayout                      layout = dmGameSystem::CompLabelGetTextLayout(component);
    const uint32_t                   object_count = layout ? TextLayoutGetObjectCount(layout) : 0;
    const TextLayoutObject*          objects = layout ? TextLayoutGetObjects(layout) : 0;
    const TextLayoutObjectAttribute* attributes = layout ? TextLayoutGetObjectAttributes(layout) : 0;
    const char*                      source = layout ? TextLayoutGetObjectSource(layout) : "";
    lua_createtable(L, object_count, 0);

    for (uint32_t i = 0; i < object_count; ++i)
    {
        const TextLayoutObject& object = objects[i];
        lua_createtable(L, 0, 10);
        lua_pushstring(L, GetLayoutObjectTagName(object.m_Tag));
        lua_setfield(L, -2, "type");
        dmScript::PushHash(L, object.m_Id);
        lua_setfield(L, -2, "id");
        lua_pushnumber(L, object.m_TextOffset);
        lua_setfield(L, -2, "text_offset");
        lua_pushnumber(L, object.m_TextLength);
        lua_setfield(L, -2, "text_length");
        lua_pushnumber(L, object.m_Width);
        lua_setfield(L, -2, "width");
        lua_pushnumber(L, object.m_Height);
        lua_setfield(L, -2, "height");
        dmVMath::Point3 position(0.0f, 0.0f, 0.0f);
        dmGameSystem::CompLabelGetLayoutObjectPosition(component, layout, &object, &position);
        lua_pushnumber(L, position.getX());
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, position.getY());
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, position.getZ());
        lua_setfield(L, -2, "z");
        lua_createtable(L, 0, object.m_AttributeCount);

        for (uint32_t j = 0; j < object.m_AttributeCount; ++j)
        {
            const TextLayoutObjectAttribute& attribute = attributes[object.m_AttributeIndex + j];

            if (attribute.m_NameLength)
            {
                lua_pushlstring(L, source + attribute.m_NameOffset, attribute.m_NameLength);
            }
            else
            {
                lua_pushstring(L, "value");
            }

            lua_pushlstring(L, source + attribute.m_ValueOffset, attribute.m_ValueLength);
            lua_settable(L, -3);
        }

        lua_setfield(L, -2, "attributes");
        lua_rawseti(L, -2, i + 1);
    }

    return 1;
}

static const luaL_reg Module_methods[] = {
    { "set_text", SetText },
    { "set_curve", SetCurve },
    { "get_text", GetText },
    { "get_layout_objects", GetLayoutObjects },
    { 0, 0 }
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

} // namespace dmGameSystem
