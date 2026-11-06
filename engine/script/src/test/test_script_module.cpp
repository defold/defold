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

#include "script.h"
#include "script_private.h"
#include "test_script.h"

#include <script/lua_source_ddf.h>

#include <testmain/testmain.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <stdlib.h>
#include <string.h>

class ScriptModuleTest : public dmScriptTest::ScriptTest
{
    public:
    void SetUp() override
    {
        dmScriptTest::ScriptTest::SetUp();
        m_DestroyedResources = 0;
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(m_ResourceFactory, "module", this, 0, CreateModuleResource, 0, DestroyModuleResource, 0));
    }

    void TearDown() override
    {
        dmScript::ClearModules(m_Context);
        dmScriptTest::ScriptTest::TearDown();
    }

    static dmResource::Result CreateModuleResource(const dmResource::ResourceCreateParams* params)
    {
        ResourceDescriptorSetResource(params->m_Resource, strdup((const char*)params->m_Buffer));
        return dmResource::RESULT_OK;
    }

    static dmResource::Result DestroyModuleResource(const dmResource::ResourceDestroyParams* params)
    {
        ScriptModuleTest* test = (ScriptModuleTest*)params->m_Context;
        free(ResourceDescriptorGetResource(params->m_Resource));
        ++test->m_DestroyedResources;
        return dmResource::RESULT_OK;
    }

    uint32_t m_DestroyedResources;
};

// NOTE: we don't generate actual bytecode for this test-data, so
// just pass in regular lua source instead.
static dmLuaDDF::LuaSource* LuaSourceFromText(const char *text)
{
    static dmLuaDDF::LuaSource tmp;
    memset(&tmp, 0x00, sizeof(tmp));
    tmp.m_Script.m_Data = (uint8_t*)text;
    tmp.m_Script.m_Count = strlen(text);
    tmp.m_Bytecode.m_Data = (uint8_t*)text;
    tmp.m_Bytecode.m_Count = strlen(text);
    tmp.m_Bytecode64.m_Data = (uint8_t*)text;
    tmp.m_Bytecode64.m_Count = strlen(text);
    tmp.m_Filename = "dummy";
    return &tmp;
}

TEST_F(ScriptModuleTest, TestModule)
{
    int top = lua_gettop(L);
    const char* script = "module(..., package.seeall)\n function f1()\n return 123\n end\n";
    const char* script_file_name = "x.test_mod";
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, script_file_name));
    dmScript::Result ret = dmScript::AddModule(m_Context, LuaSourceFromText(script), script_file_name, 0, dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, script_file_name));
    ASSERT_TRUE(RunFile(L, "test_module.luac"));
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptModuleTest, TestReload)
{
    int top = lua_gettop(L);
    const char* script = "module(..., package.seeall)\n function f1()\n return 123\n end\n";
    const char* script_reload = "module(..., package.seeall)\n reloaded = 1010\n function f1()\n return 456\n end\n";
    const char* script_file_name = "x.test_mod";
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, script_file_name));
    dmScript::Result ret = dmScript::AddModule(m_Context, LuaSourceFromText(script), script_file_name, 0, dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, script_file_name));
    ASSERT_TRUE(RunFile(L, "test_module.luac"));

    ret = dmScript::ReloadModule(m_Context, LuaSourceFromText(script_reload), dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);
    lua_getfield(L, LUA_GLOBALSINDEX, "x");
    lua_getfield(L, -1, "test_mod");
    lua_getfield(L, -1, "reloaded");
    int reloaded = luaL_checkinteger(L, -1);
    ASSERT_EQ(1010, reloaded);
    lua_pop(L, 3);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptModuleTest, TestReloadReturn)
{
    int top = lua_gettop(L);
    const char* script = "local M = {}\nreturn M\n";
    const char* script_file_name = "x.test_mod";
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, script_file_name));
    dmScript::Result ret = dmScript::AddModule(m_Context, LuaSourceFromText(script), script_file_name, 0, dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, script_file_name));

    ret = dmScript::ReloadModule(m_Context, LuaSourceFromText(script), dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptModuleTest, TestReloadFail)
{
    int top = lua_gettop(L);
    const char* script = "module(..., package.seeall)\n reloaded = 1010\n function f1()\n return 123\n end\n";
    const char* script_reload = "module(..., package.seeall)\n reloaded = -1\n function f1()\n return 123\n en\n"; // NOTE: en instead of end
    const char* script_file_name = "x.test_mod";
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, script_file_name));
    dmScript::Result ret = dmScript::AddModule(m_Context, LuaSourceFromText(script), script_file_name, 0, dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_OK, ret);
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, script_file_name));
    ASSERT_TRUE(RunFile(L, "test_module.luac"));

    ret = dmScript::ReloadModule(m_Context, LuaSourceFromText(script_reload), dmHashString64(script_file_name));
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, ret);
    lua_getfield(L, LUA_GLOBALSINDEX, "x");
    lua_getfield(L, -1, "test_mod");
    lua_getfield(L, -1, "reloaded");
    int reloaded = luaL_checkinteger(L, -1);
    ASSERT_EQ(1010, reloaded);
    lua_pop(L, 3);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptModuleTest, TestModuleMissing)
{
    int top = lua_gettop(L);
    ASSERT_FALSE(RunFile(L, "test_module_missing.luac"));
    ASSERT_EQ(top+1, lua_gettop(L));
    lua_pop(L, lua_gettop(L)-top);
}

TEST_F(ScriptModuleTest, TestReloadNotLoaded)
{
    int top = lua_gettop(L);
    dmScript::Result ret = dmScript::ReloadModule(m_Context, LuaSourceFromText(""), dmHashString64("not_loaded"));
    ASSERT_EQ(dmScript::RESULT_MODULE_NOT_LOADED, ret);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptModuleTest, TestModulePathSurvivesModuleTableGrowth)
{
    const char* script = "return {}\n";
    const dmhash_t first_path_hash = dmHashString64("/first_module.luac");
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(script), "first_module", 0, first_path_hash));

    char module_name[32];
    for (uint32_t i = 1; i < 257; ++i)
    {
        dmSnPrintf(module_name, sizeof(module_name), "module_%u", i);
        ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(script), module_name, 0, dmHashString64(module_name)));
    }

    ASSERT_EQ(dmScript::RESULT_OK, dmScript::ReloadModule(m_Context, LuaSourceFromText(script), first_path_hash));
}

TEST_F(ScriptModuleTest, TestModuleTablesGrowIndependently)
{
    for (uint32_t i = 0; i < m_Context->m_PathToModule.Capacity(); ++i)
    {
        m_Context->m_PathToModule.Put(i, i);
    }

    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText("return {}\n"), "first_module", 0, 256));
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, (dmhash_t) 256));
}

TEST_F(ScriptModuleTest, TestModuleNameHashCollision)
{
    const char* script = "return {}\n";
    const char* first_module_name = "issue-12785.collision_target_aaaaaaaaaaa00043b5a";
    const char* second_module_name = "issue-12785.col_000000000000000000000000lnAXgm1b";
    ASSERT_EQ(dmHashString64(first_module_name), dmHashString64(second_module_name));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(script), first_module_name, 0, 1));

    ASSERT_EQ(dmScript::RESULT_MODULE_NAME_HASH_COLLISION, dmScript::AddModule(m_Context, LuaSourceFromText(script), second_module_name, 0, 2));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, (dmhash_t) 2));
}

// Direct-source modules copy caller input while retaining their resource for deferred and repeated require.
TEST_F(ScriptModuleTest, TestResourceModuleOwnsCode)
{
    char code[] = "return { value = 123 }";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/shared.module", code, sizeof(code), (void**)&resource));
    dmResource::IncRef(m_ResourceFactory, resource);
    char* source = strdup(code);
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(source), "shared_module", resource, dmHashString64("/shared.module")));
    dmResource::Release(m_ResourceFactory, resource);

    ASSERT_EQ(0u, m_DestroyedResources);
    memcpy(strstr(source, "123"), "456", 3);
    ASSERT_TRUE(RunString(L, "assert(require('shared_module').value == 123)"));
    free(source);
    ASSERT_TRUE(RunString(L, "package.loaded.shared_module = nil; assert(require('shared_module').value == 123)"));

    dmScript::ClearModules(m_Context);
    ASSERT_EQ(1u, m_DestroyedResources);
}

// Modules without resources retain their own code after both add and reload callers free their input.
TEST_F(ScriptModuleTest, TestModuleWithoutResourceOwnsCode)
{
    const dmhash_t path_hash = dmHashString64("/owned.module");
    char* code = strdup("return { value = 123 }");
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(code), "owned_module", 0, path_hash));
    free(code);
    ASSERT_TRUE(RunString(L, "assert(require('owned_module').value == 123)"));

    code = strdup("return { value = 456, reloaded = true }");
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::ReloadModule(m_Context, LuaSourceFromText(code), path_hash));
    free(code);
    ASSERT_TRUE(RunString(L, "package.loaded.owned_module = nil; assert(require('owned_module').value == 456)"));
}

// Reload copies temporary input even when a resource reference is present.
TEST_F(ScriptModuleTest, TestResourceModuleReloadOwnsCode)
{
    char code[] = "return { value = 123 }";
    const dmhash_t path_hash = dmHashString64("/reload.module");
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/reload.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(resource), "reload_module", resource, path_hash));
    ASSERT_TRUE(RunString(L, "assert(require('reload_module').value == 123)"));

    char* source = strdup("return { value = 456, reloaded = true }");
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::ReloadModule(m_Context, LuaSourceFromText(source), path_hash));
    memcpy(strstr(source, "456"), "789", 3);
    ASSERT_TRUE(RunString(L, "package.loaded.reload_module = nil; assert(require('reload_module').value == 456)"));
    free(source);
    ASSERT_TRUE(RunString(L, "package.loaded.reload_module = nil; assert(require('reload_module').value == 456)"));

    dmScript::ClearModules(m_Context);
    ASSERT_EQ(1u, m_DestroyedResources);
}

// Failed reload preserves the existing Lua table after the caller frees its input.
TEST_F(ScriptModuleTest, TestResourceModuleReloadFailure)
{
    char code[] = "return { value = 123 }";
    const dmhash_t path_hash = dmHashString64("/invalid.module");
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/invalid.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(resource), "invalid_module", resource, path_hash));
    ASSERT_TRUE(RunString(L, "assert(require('invalid_module').value == 123)"));

    char* source = strdup("return {");
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, dmScript::ReloadModule(m_Context, LuaSourceFromText(source), path_hash));
    free(source);
    ASSERT_TRUE(RunString(L, "assert(require('invalid_module').value == 123)"));

    dmScript::ClearModules(m_Context);
    ASSERT_EQ(1u, m_DestroyedResources);
}

// Independent contexts retain their own module state and a reference to the same resource.
TEST_F(ScriptModuleTest, TestResourceModuleIndependentContexts)
{
    char code[] = "return { value = 123 }";
    const dmhash_t path_hash = dmHashString64("/contexts.module");
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/contexts.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText(resource), "shared_module", resource, path_hash));

    dmScript::ContextParams params = {};
    params.m_ConfigFile = m_ConfigFile;
    params.m_Factory = m_ResourceFactory;
    dmScript::HContext other_context = dmScript::NewContext(params);
    dmScript::Initialize(other_context);
    lua_State* other_state = dmScript::GetLuaState(other_context);
    dmResource::IncRef(m_ResourceFactory, resource);
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(other_context, LuaSourceFromText(resource), "shared_module", resource, path_hash));
    ASSERT_TRUE(RunString(L, "require('shared_module').value = 456"));
    ASSERT_TRUE(RunString(other_state, "assert(require('shared_module').value == 123)"));

    dmScript::Finalize(other_context);
    dmScript::DeleteContext(other_context);
    ASSERT_EQ(0u, m_DestroyedResources);
    ASSERT_TRUE(RunString(L, "assert(require('shared_module').value == 456)"));
    dmScript::ClearModules(m_Context);
    ASSERT_EQ(1u, m_DestroyedResources);
}

static int PrepareModule(dmScript::HContext context, lua_State* L, void* resource)
{
    lua_getglobal(L, "prepared_module_loads");
    int count = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, count + 1);
    lua_setglobal(L, "prepared_module_loads");
    return dmScript::LuaLoad(L, LuaSourceFromText((const char*)resource));
}

// Optional modules retain syntax errors until require, where Lua code can catch them.
TEST_F(ScriptModuleTest, PreparedModuleDefersSyntaxError)
{
    char code[] = "local =";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/syntax.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "syntax_module", resource, dmHashString64("/syntax.module"), PrepareModule));
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, "syntax_module"));
    ASSERT_TRUE(RunString(L,
                          "assert(prepared_module_loads == 1); "
                          "local ok, err = pcall(require, 'syntax_module'); assert(not ok); assert(type(err) == 'string'); "
                          "assert(package.loaded.syntax_module == nil); "
                          "local again, next_err = pcall(require, 'syntax_module'); assert(not again); assert(next_err == err)"));
}

// Cached chunks must release module environments after require and reload, while returned functions keep their own environment.
TEST_F(ScriptModuleTest, PreparedModuleReleasesEnvironment)
{
    char code[] = "local M = { value = 123 }; prepared_environments[1] = M; setfenv(1, M); function get_value() return value end; return M";
    char* resource = 0;
    const dmhash_t path_hash = dmHashString64("/environment.module");
    ASSERT_TRUE(RunString(L, "prepared_environments = setmetatable({}, { __mode = 'v' })"));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/environment.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "environment_module", resource, path_hash, PrepareModule));
    ASSERT_TRUE(RunString(L,
                          "do local M = require('environment_module'); assert(M.get_value() == 123) end; "
                          "package.loaded.environment_module = nil; collectgarbage('collect'); collectgarbage('collect'); "
                          "assert(prepared_environments[1] == nil)"));
    ASSERT_TRUE(RunString(L,
                          "local co = coroutine.create(function() "
                          "local env = setmetatable({}, { __index = _G }); prepared_environments[2] = env; setfenv(0, env); "
                          "assert(require('environment_module').get_value() == 123) end); "
                          "local ok, err = coroutine.resume(co); assert(ok, err); co = nil; "
                          "package.loaded.environment_module = nil; collectgarbage('collect'); collectgarbage('collect'); "
                          "assert(prepared_environments[1] == nil); assert(prepared_environments[2] == nil)"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::ReloadModule(m_Context, LuaSourceFromText(code), path_hash));
    ASSERT_TRUE(RunString(L, "collectgarbage('collect'); collectgarbage('collect'); assert(prepared_environments[1] == nil)"));
}

// Error paths release changed environments and preserve the error object passed to pcall.
TEST_F(ScriptModuleTest, PreparedModuleErrorReleasesEnvironment)
{
    char code[] = "local M = {}; prepared_environments[1] = M; local fail = error; setfenv(1, M); fail(M)";
    char* resource = 0;
    const dmhash_t path_hash = dmHashString64("/error_environment.module");
    ASSERT_TRUE(RunString(L, "prepared_environments = setmetatable({}, { __mode = 'v' })"));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/error_environment.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "error_environment_module", resource, path_hash, PrepareModule));
    ASSERT_TRUE(RunString(L,
                          "do local ok, err = pcall(require, 'error_environment_module'); assert(not ok); assert(err == prepared_environments[1]) end; "
                          "package.loaded.error_environment_module = nil; collectgarbage('collect'); collectgarbage('collect'); "
                          "assert(prepared_environments[1] == nil)"));
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, dmScript::ReloadModule(m_Context, LuaSourceFromText(code), path_hash));
    ASSERT_TRUE(RunString(L, "collectgarbage('collect'); collectgarbage('collect'); assert(prepared_environments[1] == nil)"));
}

// Module failures preserve their Lua call frames for the caller's error handler.
TEST_F(ScriptModuleTest, PreparedModulePreservesErrorFrames)
{
    char code[] = "local function inner_failure() error('module failed') end; inner_failure()";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/traceback.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "traceback_module", resource, dmHashString64("/traceback.module"), PrepareModule));
    ASSERT_TRUE(RunString(L,
                          "local seen = false; local ok, err = xpcall(function() require('traceback_module') end, function(err) "
                          "for i = 1, 20 do local info = debug.getinfo(i, 'S'); if info and info.source == 'dummy' then seen = true end end; "
                          "return err end); assert(not ok); assert(type(err) == 'string'); assert(seen, 'module error frames missing')"));
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, dmScript::ReloadModule(m_Context, LuaSourceFromText(code), dmHashString64("/traceback.module")));
    ASSERT_NE((const char*)0, strstr(GetLog(), "inner_failure"));
}

struct ModuleAllocationCount
{
    lua_Alloc m_Allocate;
    void* m_Context;
    uint32_t m_Count;
};

static void* CountModuleAllocations(void* context, void* pointer, size_t previous, size_t size)
{
    ModuleAllocationCount* count = (ModuleAllocationCount*)context;
    if (size > 0)
        ++count->m_Count;
    return count->m_Allocate(count->m_Context, pointer, previous, size);
}

static void* LimitModuleAllocations(void* context, void* pointer, size_t previous, size_t size)
{
    ModuleAllocationCount* count = (ModuleAllocationCount*)context;
    if (size > 1024)
    {
        ++count->m_Count;
        return 0;
    }
    return count->m_Allocate(count->m_Context, pointer, previous, size);
}

static int PreparedModuleFromGlobal(dmScript::HContext context, lua_State* L, void* resource)
{
    lua_getglobal(L, "precompiled_module");
    return 0;
}

// Lua allocation failures during preparation return a resource-loading error and preserve the caller's resource ownership.
TEST_F(ScriptModuleTest, PreparedModuleAllocationFailureRollsBack)
{
    ASSERT_TRUE(RunString(L,
                          "local bytes = {}; local seed = 1; for i = 1, 4096 do seed = (seed * 16807) % 2147483647; bytes[i] = string.char(seed % 256) end; "
                          "precompiled_module = assert(loadstring('return ' .. string.format('%q', table.concat(bytes))))"));
    char code[] = "return {}";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/allocation_failure.module", code, sizeof(code), (void**)&resource));
    int top = lua_gettop(L);
    ModuleAllocationCount count = {};
    count.m_Allocate = lua_getallocf(L, &count.m_Context);
    lua_setallocf(L, LimitModuleAllocations, &count);
    dmScript::Result result = dmScript::AddModule(m_Context, 0, "allocation_failure_module", resource, dmHashString64("/allocation_failure.module"), PreparedModuleFromGlobal);
    lua_setallocf(L, count.m_Allocate, count.m_Context);
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, result);
    ASSERT_GT(count.m_Count, 0u);
    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, "allocation_failure_module"));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, dmHashString64("/allocation_failure.module")));
    ASSERT_EQ(0u, m_DestroyedResources);
    dmResource::Release(m_ResourceFactory, resource);
    ASSERT_EQ(1u, m_DestroyedResources);
}

// The first module lookup returns a Lua function already created during resource loading, without allocating VM nodes.
TEST_F(ScriptModuleTest, PreparedModuleFirstLookupDoesNotAllocate)
{
    char code[] = "return {}";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/allocation.module", code, sizeof(code), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "allocation_module", resource, dmHashString64("/allocation.module"), PrepareModule));
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaders");
    lua_rawgeti(L, -1, 1);
    lua_remove(L, -2);
    lua_remove(L, -2);
    lua_pushliteral(L, "allocation_module");
    ASSERT_TRUE(lua_checkstack(L, 32));

    ModuleAllocationCount count = {};
    count.m_Allocate = lua_getallocf(L, &count.m_Context);
    lua_gc(L, LUA_GCSTOP, 0);
    lua_setallocf(L, CountModuleAllocations, &count);
    int status = lua_pcall(L, 1, 1, 0);
    lua_setallocf(L, count.m_Allocate, count.m_Context);
    lua_gc(L, LUA_GCRESTART, 0);
    ASSERT_EQ(0, status);
    ASSERT_EQ(0u, count.m_Count);
    ASSERT_TRUE(lua_isfunction(L, -1));
    ASSERT_FALSE(lua_iscfunction(L, -1));
    lua_pop(L, 1);
}

static int PrepareDumpedModule(dmScript::HContext context, lua_State* L, void* resource)
{
    lua_getglobal(L, "prepared_bytecode");
    size_t size;
    const char* code = lua_tolstring(L, -1, &size);
    dmLuaDDF::LuaSource source = {};
    source.m_Bytecode.m_Data = (uint8_t*)code;
    source.m_Bytecode.m_Count = size;
    source.m_Filename = "@prepared_bytecode.lua";
    int status = dmScript::LuaLoad(L, &source);
    lua_remove(L, -2);
    return status;
}

// Binary modules preserve chunk varargs, global arg lookup, nested upvalues, debug names and LuaJIT cdata constants.
TEST_F(ScriptModuleTest, PreparedBytecodeModulePreservesSemantics)
{
    ASSERT_TRUE(RunString(L,
                          "module_arguments = {}; arg = module_arguments; "
                          "prepared_bytecode = string.dump(assert(loadstring([[local name = ...; assert(arg == module_arguments); "
                          "local offset = 7; local function value(x) return x + offset end; "
                          "return { name = name, value = value }]], '@prepared_bytecode.lua')))"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "bytecode_module", 0, dmHashString64("/bytecode.module"), PrepareDumpedModule));
    ASSERT_TRUE(RunString(L,
                          "prepared_bytecode = nil; local M = require('bytecode_module'); "
                          "assert(M.name == 'bytecode_module'); assert(M.value(3) == 10); "
                          "assert(debug.getinfo(M.value, 'S').source == '@prepared_bytecode.lua'); "
                          "package.loaded.bytecode_module = nil; assert(require('bytecode_module').value(5) == 12); "
                          "local source = jit and 'return { value = 123ULL }' or 'return { value = 123 }'; "
                          "prepared_bytecode = string.dump(assert(loadstring(source)))"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "cdata_module", 0, dmHashString64("/cdata.module"), PrepareDumpedModule));
    ASSERT_TRUE(RunString(L,
                          "prepared_bytecode = nil; local M = require('cdata_module'); "
                          "if jit then assert(tostring(M.value) == '123ULL') else assert(M.value == 123) end; "
                          "local captured = 7; prepared_bytecode = string.dump(function(...) return { name = ..., value = captured or 123 } end)"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "upvalue_module", 0, dmHashString64("/upvalue.module"), PrepareDumpedModule));
    ASSERT_TRUE(RunString(L,
                          "prepared_bytecode = nil; local M = require('upvalue_module'); assert(M.name == 'upvalue_module'); assert(M.value == 123); "
                          "package.loaded.upvalue_module = nil; assert(require('upvalue_module').value == 123); "
                          "prepared_bytecode = string.dump(assert(loadstring('')))"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "empty_module", 0, dmHashString64("/empty.module"), PrepareDumpedModule));
    ASSERT_TRUE(RunString(L,
                          "prepared_bytecode = nil; assert(require('empty_module') == true); package.loaded.empty_module = nil; assert(require('empty_module') == true); "
                          "prepared_bytecode = string.dump(assert(loadstring('return { value = 456 }')), true)"));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "stripped_module", 0, dmHashString64("/stripped.module"), PrepareDumpedModule));
    ASSERT_TRUE(RunString(L, "prepared_bytecode = nil; assert(require('stripped_module').value == 456); package.loaded.stripped_module = nil; assert(require('stripped_module').value == 456)"));
}

// Preparation creates reusable code during registration, leaves its body deferred, and resets its environment on repeat require.
TEST_F(ScriptModuleTest, PreparedModuleRunsWithoutLoadingAgain)
{
    char code[] = "_G.prepared_module_runs = (_G.prepared_module_runs or 0) + 1; local result = { value = 123 }; setfenv(1, {}); return result";
    char* resource = 0;
    const dmhash_t path_hash = dmHashString64("/prepared.module");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/prepared.module", code, strlen(code) + 1, (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "prepared_module", resource, path_hash, PrepareModule));
    ASSERT_TRUE(RunString(L, "assert(prepared_module_loads == 1); assert(prepared_module_runs == nil)"));

    resource[0] = '!';
    ASSERT_TRUE(RunString(L, "assert(require('prepared_module').value == 123); assert(prepared_module_runs == 1)"));
    ASSERT_TRUE(RunString(L,
                          "package.loaded.prepared_module = nil; "
                          "local co = coroutine.create(function() return require('prepared_module').value end); "
                          "local ok, value = coroutine.resume(co); assert(ok, value); assert(value == 123); "
                          "assert(prepared_module_runs == 2); assert(prepared_module_loads == 1)"));

    char replacement[] = "return { value = 456 }";
    char* next = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/replacement.module", replacement, strlen(replacement) + 1, (void**)&next));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, 0, "prepared_module", next, path_hash, PrepareModule));
    ASSERT_EQ(1u, m_DestroyedResources);
    ASSERT_TRUE(RunString(L, "package.loaded.prepared_module = nil; assert(require('prepared_module').value == 456); assert(prepared_module_loads == 2)"));
    dmScript::ClearModules(m_Context);
    ASSERT_EQ(2u, m_DestroyedResources);
}

static int FailModulePreparation(dmScript::HContext context, lua_State* L, void* resource)
{
    lua_pushliteral(L, "Module resource is unavailable");
    return LUA_ERRFILE;
}

// Failed resource reads roll back new path markers and leave an existing path alias and the caller's resource ownership intact.
TEST_F(ScriptModuleTest, PreparedModuleFailureRollsBackRegistration)
{
    const dmhash_t path_hash = dmHashString64("/existing.module");
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(m_Context, LuaSourceFromText("return { value = 123 }"), "existing_module", 0, path_hash));
    char invalid[] = "local =";
    char* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_ResourceFactory, "/invalid_prepared.module", invalid, strlen(invalid) + 1, (void**)&resource));
    int top = lua_gettop(L);
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, dmScript::AddModule(m_Context, 0, "failed_alias", resource, path_hash, FailModulePreparation));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, "failed_alias"));
    ASSERT_TRUE(dmScript::ModuleLoaded(m_Context, path_hash));
    ASSERT_TRUE(RunString(L, "assert(require('existing_module').value == 123)"));

    const dmhash_t new_path = dmHashString64("/new_prepared.module");
    ASSERT_EQ(dmScript::RESULT_LUA_ERROR, dmScript::AddModule(m_Context, 0, "failed_new", resource, new_path, FailModulePreparation));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, "failed_new"));
    ASSERT_FALSE(dmScript::ModuleLoaded(m_Context, new_path));
    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_EQ(0u, m_DestroyedResources);
    dmResource::Release(m_ResourceFactory, resource);
    ASSERT_EQ(1u, m_DestroyedResources);
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
