#include <dmsdk/extension/extension.h>
#include <assert.h>

namespace dmIAPStub
{

static int IAP_ThrowError(lua_State* L)
{
    return luaL_error(L, "iap has been removed from core, please read /builtins/docs/iap.md for more information.");
}

static const luaL_reg IAP_methods[] =
{
    {"list", IAP_ThrowError},
    {"buy", IAP_ThrowError},
    {"finish", IAP_ThrowError},
    {"restore", IAP_ThrowError},
    {"set_listener", IAP_ThrowError},
    {"get_provider_id", IAP_ThrowError},
    {0, 0}
};

static dmExtension::Result IAP_Initialize(dmExtension::Params* params)
{
    lua_State*L = params->m_L;
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_register(L, "iap", IAP_methods);
    }
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
	return dmExtension::RESULT_OK;
}

static dmExtension::Result IAP_Finalize(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IAPExt, "IAP", 0, 0, IAP_Initialize, 0, 0, IAP_Finalize)

} // namespace
