// Extension lib defines
#define LIB_NAME "NativeExtensionsRule"
#define MODULE_NAME "NativeModule"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdio.h>

static int HelloNative(lua_State* L) {
  printf("Hello World from C!\n");
  return 0;
}

static const luaL_reg module_methods[] =
  {{"helloNative", HelloNative},
   {0, 0}};

static void LuaInit(lua_State* L) {
  int top = lua_gettop(L);

  luaL_register(L, MODULE_NAME, module_methods);

  lua_pop(L, 1);
  assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams* params) {
  return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeMyExtension(dmExtension::Params* params) {
  LuaInit(params->m_L);
  printf("Registered %s Extension\n", MODULE_NAME);
  return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams* params) {
  return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeMyExtension(dmExtension::Params* params) {
  return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(NativeExtensionsRule, LIB_NAME, AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, 0, 0, FinalizeMyExtension);

