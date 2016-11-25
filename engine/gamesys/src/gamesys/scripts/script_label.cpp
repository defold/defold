
#include <dlib/log.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <render/render.h>
#include <script/script.h>


#include "script_label.h"
#include "gamesys_ddf.h"
#include "label_ddf.h"

namespace dmGameSystem
{

/*# Label API documentation
 *
 * Functions to manipulate a label component.
 *
 * @name Label
 * @namespace label
 */

/*# label color (vector4)
 *
 * The color of the label. The type of the property is vector4.
 *
 * @name color
 * @property
 *
 * @examples
 * <pre>
 * function init(self)
 *  -- Get the current color's y component
 *  local red_component = go.get("#label", "color.y")
 *  -- Animate the color
 *  go.animate("#label", "color", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * </pre>
 */

/*# label outline (vector4)
 *
 * The outline color of the label. The type of the property is vector4.
 *
 * @name outline
 * @property
 *
 * @examples
 * <pre>
 * function init(self)
 *  -- Get the current outline color
 *  local outline = go.get("#label", "outline")
 *  -- Animate the property
 *  go.animate("#label", "outline", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * </pre>
 */

/*# label shadow (vector4)
 *
 * The shadow color of the label. The type of the property is vector4.
 *
 * @name shadow
 * @property
 *
 * @examples
 * <pre>
 * function init(self)
 *  -- Get the current shadow color
 *  local shadow = go.get("#label", "shadow")
 *  -- Animate the property
 *  go.animate("#label", "shadow", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(0,1,0,1), go.EASING_INOUTSINE, 1)
 * end
 * </pre>
 */

/*# label scale (vector3|number)
 *
 * The scale of the label. The type of the property is vector3.
 *
 * @name scale
 * @property
 *
 * @examples
 * <p>
 * How to scale a label independently along the X and Y axis:
 * </p>
 * <pre>
 * function init(self)
 *  -- Double the y-axis scaling on component "label"
 *  local yscale = go.get("#label", "scale.y")
 *  go.set("#label", "scale.y", yscale * 2)
 *  -- Set the new scale altogether
 *  go.set("#label", "scale", vmath.vector3(2,2,2))
 *  -- Animate the scale
 *  go.animate("#label", "scale", go.PLAYBACK_LOOP_PINGPONG, vmath.vector3(2,2,2), go.EASING_INOUTSINE, 1)
 * end
 * </pre>
 */

/*# label size (vector3)
 *
 * Returns the size of the label. THe size will constrain the text if line break is enabled
 * The type of the property is vector3.
 *
 * @name size
 * @property
 *
 * @examples
 * <p>
 * How to query a label's size, either as a vector or selecting a specific dimension:
 * </p>
 * <pre>
 * function init(self)
 *  -- get size from component "label"
 *  local size = go.get("#label", "size")
 *  local sizex = go.get("#label", "size.x")
 *  -- do something useful
 *  assert(size.x == sizex)
 * end
 * </pre>
 */

static void FreeLabelString(dmMessage::Message* message)
{
    dmGameSystemDDF::SetText* textmsg = (dmGameSystemDDF::SetText*) message->m_Data;
    free( (void*)textmsg->m_Text );
}

/*# set the text for a label
 * Sets the text of a label component
 *
 * @name label.set_text
 * @param url the label that should have a constant set (url)
 * @param text the text (string|hash)
 * @examples
 * <pre>
 * function init(self)
 *     label.set_text("#label", "Hello World!")
 * end
 * </pre>
 */
static int SetText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmGameObject::HInstance instance = CheckGoInstance(L);

    const char* text = luaL_checkstring(L, 2);
    if (!text)
    {
        luaL_error(L, "Expected string as first argument");
        return 0;
    }

    dmGameSystemDDF::SetText msg;
    msg.m_Text = strdup(text);

    dmMessage::URL receiver;
    dmMessage::URL sender;
    dmScript::ResolveURL(L, 1, &receiver, &sender);

    if (dmMessage::RESULT_OK != dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetText::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetText::m_DDFDescriptor, &msg, sizeof(msg), FreeLabelString) )
    {
        free((void*)msg.m_Text);
        luaL_error(L, "Failed to send label string as message!");
    }
    return 0;
}


static const luaL_reg Module_methods[] =
{
    {"set_text", SetText},
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
