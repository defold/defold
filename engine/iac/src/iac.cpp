#include <stdlib.h>
#include <assert.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "iac.h"

// Extension internal platform specific functions
extern int IAC_PlatformSetListener(lua_State* L);


/*# set iac listener
 *
 * The listener callback has the following signature: function(self, payload, type) where payload is a table
 * with the iac payload and type is an iac.TYPE_<TYPE> enumeration.
 *
 * @name iac.set_listener
 * @param listener listener callback function (function)
 *
 * @examples
 * <p>
 * Set the iac listener.
 * </p>
 * <pre>
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
 *
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
