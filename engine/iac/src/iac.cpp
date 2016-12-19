#include <stdlib.h>
#include <assert.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "iac.h"

// Extension internal platform specific functions
extern int IAC_PlatformSetListener(lua_State* L);

/*# Inter-app communication API documentation
 *
 * Functions and constants for doing inter-app communication.
 *
 * [icon:ios] [icon:android] These API:s only exist on mobile platforms.
 *
 * @document
 * @name Inter-app communication
 * @namespace iac
 */

/*# set iac listener
 *
 * Sets the listener function for inter-app communication events.
 *
 * @name iac.set_listener
 * @param listener [type:function(self, payload, type)] listener callback function
 *
 * self
 * :        [type:object] The current object.
 *
 * payload
 * :        [type:table] The iac payload.
 *
 * type
 * :        [type:constant] The type of iac, an iac.TYPE_<TYPE> enumeration.
 *
 * @examples
 *
 * Set the iac listener.
 *
 * ```lua
 * local function iac_listener(self, payload, type)
 *      if type == iac.TYPE_INVOCATION then
 *          -- This was an invocation
 *          print(payload.origin) -- origin may be empty string if it could not be resolved
 *          print(payload.url)
 *      end
 * end
 *
 * local init(self)
 *      ...
 *      iac.set_listener(iac_listener)
 * end
 * ```
 */
int IAC_SetListener(lua_State* L)
{
    return IAC_PlatformSetListener(L);
}



/*# iac type
 *
 * @name iac.TYPE_INVOCATION
 * @variable
 */

static const luaL_reg IAC_methods[] =
{
    {"set_listener", IAC_SetListener},
    {0, 0}
};


namespace dmIAC
{

    dmExtension::Result Initialize(dmExtension::Params* params)
    {
        lua_State*L = params->m_L;
        int top = lua_gettop(L);
        luaL_register(L, "iac", IAC_methods);

#define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) val); \
            lua_setfield(L, -2, #name);

        SETCONSTANT(TYPE_INVOCATION, DM_IAC_EXTENSION_TYPE_INVOCATION);

#undef SETCONSTANT

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return dmExtension::RESULT_OK;
    }


    dmExtension::Result Finalize(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }

}
