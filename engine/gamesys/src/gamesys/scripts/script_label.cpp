
#include <dlib/log.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <render/render.h>
#include <script/script.h>


#include "script_label.h"
#include "../components/comp_label.h"
#include "gamesys_ddf.h"
#include "label_ddf.h"

namespace dmGameSystem
{

/*# Label API documentation
 *
 * Functions to manipulate a label component.
 *
 * @document
 * @name Label
 * @namespace label
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
 * Returns the size of the label. The size will constrain the text if line break is enabled
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

static void FreeLabelString(dmMessage::Message* message)
{
    dmGameSystemDDF::SetText* textmsg = (dmGameSystemDDF::SetText*) message->m_Data;
    free( (void*)textmsg->m_Text );
}

/*# set the text for a label
 *
 * Sets the text of a label component
 *
 * @name label.set_text
 * @param url [type:string|hash|url] the label that should have a constant set
 * @param text [type:string] the text
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

    dmGameObject::HInstance instance = CheckGoInstance(L);

    const char* text = luaL_checkstring(L, 2);
    if (!text)
    {
        return DM_LUA_ERROR("Expected string as second argument");
    }

    dmGameSystemDDF::SetText msg;
    msg.m_Text = strdup(text);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    if (dmMessage::RESULT_OK != dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetText::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetText::m_DDFDescriptor, &msg, sizeof(msg), FreeLabelString) )
    {
        free((void*)msg.m_Text);
        return DM_LUA_ERROR("Failed to send label string as message!");
    }
    return 0;
}


/*# gets the text metrics for a label
 *
 * Gets the text metrics from a label component
 *
 * @name label.get_text_metrics
 * @param url [type:string|hash|url] the label to get the (unscaled) metrics from
 * @return metrics [type:table] a table with the following fields:
 *
 * - width
 * - height
 * - max_ascent
 * - max_descent
 *
 * ```lua
 * function init(self)
 *     local metrics = label.get_text_metrics("#label")
 *     pprint(metrics)
 * end
 * ```
 */
static int GetTextMetrics(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmGameObject::HInstance instance = CheckGoInstance(L);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    dmGameSystem::LabelComponent* component = (dmGameSystem::LabelComponent*)dmGameObject::GetComponentFromURL(receiver, 0);
    if (!component) {
        return DM_LUA_ERROR("Could not find instance %s:%s#%s", dmHashReverseSafe64(receiver.m_Socket), dmHashReverseSafe64(receiver.m_Path), dmHashReverseSafe64(receiver.m_Fragment));
    }

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

static const luaL_reg Module_methods[] =
{
    {"set_text", SetText},
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
