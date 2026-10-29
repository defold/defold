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

#include <jc_test/jc_test.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/time.h>
#include <resource/resource.h>
#include <dmsdk/gameobject/res_lua.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../gameobject_script.h"
#include "../gameobject_script_util.h"
#include "../script.h"
#include "gameobject/test/script/test_gameobject_script_ddf.h"
#include <gameobject/gameobject_ddf.h>
#include <gameobject/lua_ddf.h>

using namespace dmVMath;

class ScriptTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/src/gameobject/test/script";

        char path[DMPATH_MAX_PATH];
        m_Factory = dmResource::NewFactory(&params, dmTestUtil::MakeHostPath(path, sizeof(path), m_Path));
        ASSERT_NE((dmResource::HFactory)0, m_Factory);

        dmScript::ContextParams script_context_params = {};
        script_context_params.m_Factory = m_Factory;
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);

        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_ModuleContext.m_ScriptContexts.SetCapacity(1);
        m_ModuleContext.m_ScriptContexts.Push(m_ScriptContext);
        m_Contexts.SetCapacity(7,16);
        m_Contexts.Put(dmHashString64("goc"), m_Register);
        m_Contexts.Put(dmHashString64("collectionc"), m_Register);
        m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
        m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
        dmResource::RegisterTypes(m_Factory, &m_Contexts);

        dmGameObject::ComponentTypeCreateCtx component_create_ctx = {};
        component_create_ctx.m_Script = m_ScriptContext;
        component_create_ctx.m_Register = m_Register;
        component_create_ctx.m_Factory = m_Factory;
        dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        dmGameObject::SortComponentTypes(m_Register);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);
        dmMessage::Result result = dmMessage::NewSocket("@system", &m_Socket);
        assert(result == dmMessage::RESULT_OK);
    }

    void TearDown() override
    {
        dmMessage::DeleteSocket(m_Socket);
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmMessage::HSocket m_Socket;
    dmScript::HContext m_ScriptContext;
    const char* m_Path;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

static void SetCachedWorldTransform(dmGameObject::HCollection collection, dmGameObject::HInstance hinstance, const Matrix4& transform)
{
    dmGameObject::Collection* collection_ptr = dmGameObject::GetCollectionFromHandle(collection);
    dmGameObject::Instance* instance = dmGameObject::GetInstanceFromHandle(collection_ptr, hinstance);
    collection_ptr->m_WorldTransforms[instance->m_Index] = transform;
}

struct TestScript01Context
{
    dmGameObject::HRegister m_Register;
    bool m_Result;
};

static dmGameObject::CompScriptWorld* GetScriptWorld(dmGameObject::HCollection collection)
{
    uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(collection, dmHashString64("scriptc"));
    return component_type_index == 0xffffffff
            ? 0
            : (dmGameObject::CompScriptWorld*)dmGameObject::GetWorld(collection, component_type_index);
}

TEST_F(ScriptTest, WorldStorageUsesConfiguredCapacity)
{
    dmGameObject::HCollection large_collection = dmGameObject::NewCollection("large_script_storage", m_Factory, m_Register, 100000, 0x0);
    ASSERT_NE(0, large_collection);
    dmGameObject::CompScriptWorld* script_world = GetScriptWorld(large_collection);
    ASSERT_NE((dmGameObject::CompScriptWorld*)0, script_world);
    ASSERT_EQ(100000U, script_world->m_Instances.Capacity());

    dmGameObject::DeleteCollection(large_collection);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Register));

    script_world = GetScriptWorld(m_Collection);
    ASSERT_NE((dmGameObject::CompScriptWorld*)0, script_world);
    ASSERT_EQ(1024U, script_world->m_Instances.Capacity());
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE(0, go);
    ASSERT_EQ(1U, script_world->m_Instances.Size());
    ASSERT_EQ(1024U, script_world->m_Instances.Capacity());
}

void TestScript01SystemDispatch(dmMessage::Message* message, void* user_ptr)
{
    TestGameObjectDDF::Factory* f = (TestGameObjectDDF::Factory*) message->m_Data;
    // NOTE: We relocate the string here (from offset to pointer)
    f->m_Prototype = (const char*) ((uintptr_t) f->m_Prototype + (uintptr_t) f);
    TestScript01Context* context = (TestScript01Context*)user_ptr;
    bool* dispatch_result = &context->m_Result;

    TestGameObjectDDF::FactoryResult result;
    result.m_Status = 1010;
    dmDDF::Descriptor* descriptor = TestGameObjectDDF::FactoryResult::m_DDFDescriptor;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&message->m_Receiver, &message->m_Sender, dmHashString64(descriptor->m_Name), 0, (uintptr_t)descriptor, &result, sizeof(TestGameObjectDDF::FactoryResult), 0));

    *dispatch_result = f->m_Pos.getX() == 1.0 && f->m_Pos.getY() == 2.0 && f->m_Pos.getZ() == 3.0 && strcmp("test", f->m_Prototype) == 0;
}

void TestScript01CollectionDispatch(dmMessage::Message *message_object, void* user_ptr)
{

}

TEST_F(ScriptTest, TestScript01)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE(0, go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "my_object01"));

    TestGameObjectDDF::GlobalData global_data;
    global_data.m_UintValue = 12345;
    global_data.m_IntValue = -123;
    global_data.m_StringValue = "string_value";
    global_data.m_VecValue.setX(1.0f);
    global_data.m_VecValue.setY(2.0f);
    global_data.m_VecValue.setZ(3.0f);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmMessage::HSocket collection_socket = dmGameObject::GetMessageSocket(m_Collection);
    TestScript01Context context;
    context.m_Register = m_Register;
    context.m_Result = false;
    dmMessage::Dispatch(m_Socket, TestScript01SystemDispatch, &context);

    ASSERT_TRUE(context.m_Result);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    // Final dispatch to deallocate message data
    dmMessage::Dispatch(m_Socket, TestScript01SystemDispatch, &context);
    dmMessage::Dispatch(collection_socket, TestScript01CollectionDispatch, &context);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &action, 1));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

// Hashed transform targets must update the named object without allocating
// temporary URLs; missing targets and slot reuse must still resolve by identifier.
TEST_F(ScriptTest, HashTransformTargetsAvoidTemporaryURLs)
{
    dmGameObject::HInstance caller = dmGameObject::New(m_Collection, "/null.goc");
    dmGameObject::HInstance target = dmGameObject::New(m_Collection, 0);
    ASSERT_NE(0, caller);
    ASSERT_NE(0, target);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, caller, "caller"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, target, "target"));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    dmGameObject::CompScriptWorld* world = GetScriptWorld(m_Collection);
    lua_rawgeti(L, LUA_REGISTRYINDEX, world->m_Instances[0]->m_InstanceReference);
    dmScript::SetInstance(L);
    const char* code =
        "if jit then jit.off() end\n"
        "local id = hash('target')\n"
        "local p, s, r = vmath.vector3(1, 2, 3), vmath.vector3(2, 3, 4), vmath.quat()\n"
        "go.set_position(p, id); go.set_scale(s, id); go.set_rotation(r, id)\n"
        "collectgarbage('collect'); collectgarbage('stop')\n"
        "local before = collectgarbage('count')\n"
        "for i = 1, 500 do\n"
        "    go.set_position(p, id); go.set_scale(s, id); go.set_rotation(r, id)\n"
        "end\n"
        "local allocated = collectgarbage('count') - before\n"
        "collectgarbage('restart')\n"
        "assert(allocated < 1, 'hashed setters allocated temporary data: ' .. allocated .. ' KiB')\n"
        "assert(go.get_position(id) == p and go.get_scale(id) == s and go.get_rotation(id) == r)\n"
        "assert(go.get_position() == vmath.vector3())\n";
    int result = luaL_dostring(L, code);
    if (result)
        dmLogError("%s", lua_tostring(L, -1));
    ASSERT_EQ(0, result);
    dmGameObject::Delete(m_Collection, target, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(0, luaL_dostring(L, "assert(not pcall(go.set_position, vmath.vector3(), hash('target')))"));

    target = dmGameObject::New(m_Collection, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, target, "target"));
    ASSERT_EQ(0, luaL_dostring(L, "go.set_position(vmath.vector3(4, 5, 6), hash('target'))"));
    ASSERT_NEAR(4.0f, dmGameObject::GetPosition(target).getX(), 0.000001f);
    ASSERT_NEAR(5.0f, dmGameObject::GetPosition(target).getY(), 0.000001f);
    ASSERT_NEAR(6.0f, dmGameObject::GetPosition(target).getZ(), 0.000001f);
}

TEST_F(ScriptTest, TestFailingScript02)
{
    // Test init failure

    // Avoid logging expected errors. Better solution?
    dmLogSetLevel(LOG_SEVERITY_FATAL);
    dmGameObject::New(m_Collection, "/go2.goc");
    bool result = dmGameObject::Init(m_Collection);
    dmLogSetLevel(LOG_SEVERITY_WARNING);
    EXPECT_FALSE(result);
    result = dmGameObject::Final(m_Collection);
    EXPECT_FALSE(result);
}

TEST_F(ScriptTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go3.goc");
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Avoid logging expected errors. Better solution?
    dmLogSetLevel(LOG_SEVERITY_FATAL);
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmLogSetLevel(LOG_SEVERITY_WARNING);
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestFailingScript04)
{
    // Test update failure, lua update-identifier used for something else than function callback
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go4.goc");
    ASSERT_EQ(0, go);
}

TEST_F(ScriptTest, TestFailingScript05)
{
    // Test posting to missing component id
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go5.goc");
    ASSERT_NE(0, go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "go5"));
    ASSERT_FALSE(dmGameObject::Init(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

static void CreateScriptFile(const char* file_name, const char* contents)
{
    dmLuaDDF::LuaModule lua_module;
    memset(&lua_module, 0, sizeof(lua_module));
    lua_module.m_Source.m_Script.m_Data = (uint8_t*) contents;
    lua_module.m_Source.m_Script.m_Count = strlen(contents);
    lua_module.m_Source.m_Filename = file_name;
    dmDDF::Result r = dmDDF::SaveMessageToFile(&lua_module, dmLuaDDF::LuaModule::m_DDFDescriptor, file_name);
    assert(r == dmDDF::RESULT_OK);
}

TEST_F(ScriptTest, UpdateWithoutTransformChangeDoesNotRefreshWorldTransform)
{
    const char* go_resource_name = "/update_transform_cache_no_change.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_resource_name);
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_FALSE(dmGameObject::GetCollectionFromHandle(m_Collection)->m_DirtyTransforms);

    SetCachedWorldTransform(m_Collection, go, Matrix4::translation(Vector3(42, 43, 44)));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    Point3 world_position = dmGameObject::GetWorldPosition(go);
    ASSERT_EQ(42, world_position.getX());
    ASSERT_EQ(43, world_position.getY());
    ASSERT_EQ(44, world_position.getZ());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, UpdateWithTransformChangeRefreshesWorldTransform)
{
    const char* go_resource_name = "/update_transform_cache_change.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_resource_name);
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    SetCachedWorldTransform(m_Collection, go, Matrix4::translation(Vector3(42, 43, 44)));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    Point3 world_position = dmGameObject::GetWorldPosition(go);
    ASSERT_EQ(1, world_position.getX());
    ASSERT_EQ(2, world_position.getY());
    ASSERT_EQ(3, world_position.getZ());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, MessageWithoutTransformChangeDoesNotRefreshWorldTransform)
{
    const char* go_resource_name = "/message_transform_cache_no_change.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_resource_name);
    ASSERT_NE((dmGameObject::HInstance) 0, go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "message_go"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_FALSE(dmGameObject::GetCollectionFromHandle(m_Collection)->m_DirtyTransforms);

    SetCachedWorldTransform(m_Collection, go, Matrix4::translation(Vector3(42, 43, 44)));

    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("script");
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, dmHashString64("message"), 0, 0, 0x0, 0, 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    Point3 world_position = dmGameObject::GetWorldPosition(go);
    ASSERT_EQ(42, world_position.getX());
    ASSERT_EQ(43, world_position.getY());
    ASSERT_EQ(44, world_position.getZ());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, MessageWithTransformChangeRefreshesWorldTransform)
{
    const char* go_resource_name = "/message_transform_cache_change.goc";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_resource_name);
    ASSERT_NE((dmGameObject::HInstance) 0, go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "message_transform_go"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    SetCachedWorldTransform(m_Collection, go, Matrix4::translation(Vector3(42, 43, 44)));

    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("script");
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, dmHashString64("message"), 0, 0, 0x0, 0, 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    Point3 world_position = dmGameObject::GetWorldPosition(go);
    ASSERT_EQ(1, world_position.getX());
    ASSERT_EQ(2, world_position.getY());
    ASSERT_EQ(3, world_position.getZ());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestReload)
{
    const char* script_resource_name = "/__test__.scriptc";
    char script_file_name[512];
    dmTestUtil::MakeHostPathf(script_file_name, sizeof(script_file_name), "%s%s", m_Path, script_resource_name);

    const char* go_resource_name = "/__go__.goc";
    char go_file_name[512];
    dmTestUtil::MakeHostPathf(go_file_name, sizeof(go_file_name), "%s%s", m_Path, go_resource_name);

    dmGameObjectDDF::PrototypeDesc prototype;
    memset(&prototype, 0, sizeof(prototype));
    prototype.m_Components.m_Count = 1;
    dmGameObjectDDF::ComponentDesc component_desc;
    memset(&component_desc, 0, sizeof(component_desc));
    component_desc.m_Id = "script";
    component_desc.m_Component = script_resource_name;
    prototype.m_Components.m_Data = &component_desc;

    // NOTE: +1 to remove /
    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, go_file_name);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name,
               "function update(self)\n"
               "    go.set_position(vmath.vector3(1,2,3))\n"
               "end\n");

    dmGameObject::HInstance go;
    go = dmGameObject::New(m_Collection, go_resource_name);
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    Point3 p1 = dmGameObject::GetPosition(go);
    ASSERT_EQ(1, p1.getX());
    ASSERT_EQ(2, p1.getY());
    ASSERT_EQ(3, p1.getZ());

    dmTime::Sleep(1000000); // TODO: Currently seconds time resolution in modification time

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name,
               "function update(self)\n"
               "    go.set_position(vmath.vector3(10,20,30))\n"
               "end\n");

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, script_resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    Point3 p2 = dmGameObject::GetPosition(go);
    ASSERT_EQ(10, p2.getX());
    ASSERT_EQ(20, p2.getY());
    ASSERT_EQ(30, p2.getZ());

    dmSys::Unlink(script_file_name);
    rr = dmResource::ReloadResource(m_Factory, script_resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmSys::Unlink(go_file_name);
}

TEST_F(ScriptTest, Null)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/null.goc");
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &action, 1));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestModule)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/main.goc");
    ASSERT_NE(0, go);
    dmGameObject::Init(m_Collection);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestReloadModule)
{
    const char* script_resource_name = "/test_reload_mod.luac";
    char script_file_name[512];
    dmTestUtil::MakeHostPathf(script_file_name, sizeof(script_file_name), "%s%s", m_Path, script_resource_name);

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name,
               "function test_reload_mod_func()\n"
               "    return 1010\n"
               "end\n");

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/reload.goc");
    ASSERT_NE(0, go);
    dmGameObject::Init(m_Collection);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name,
               "function test_reload_mod_func()\n"
               "    return 2020\n"
               "end\n");

    dmTime::Sleep(1000000); // TODO: Currently seconds time resolution in modification time
    dmResource::Result rr = dmResource::ReloadResource(m_Factory, script_resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
    dmSys::Unlink(script_file_name);
}

struct ReloadModuleRegistration
{
    dmResource::HFactory m_Factory;
    dmScript::HContext m_Context;
    dmGameObject::LuaScript* m_Resource;
    dmhash_t m_Path;
    int m_Value;
};

static int RegisterModuleDuringReload(lua_State* L)
{
    ReloadModuleRegistration* state = (ReloadModuleRegistration*)lua_touserdata(L, lua_upvalueindex(1));
    dmResource::IncRef(state->m_Factory, state->m_Resource);
    dmScript::Result result = dmScript::AddModule(state->m_Context, &state->m_Resource->m_LuaModule->m_Source, "reload_registered", state->m_Resource, state->m_Path);
    if (result != dmScript::RESULT_OK)
    {
        dmResource::Release(state->m_Factory, state->m_Resource);
        return luaL_error(L, "Failed to register module during reload");
    }

    lua_State* target = dmScript::GetLuaState(state->m_Context);
    if (luaL_dostring(target, "return require('reload_registered').value") != 0)
        return luaL_error(L, "%s", lua_tostring(target, -1));
    state->m_Value = lua_tointeger(target, -1);
    lua_pop(target, 1);
    return 0;
}

// Registration during reload must see new resource code even in a context the reload loop already visited.
TEST_F(ScriptTest, ReloadModuleRegistersEarlierContext)
{
    dmScript::ContextParams params = {};
    params.m_Factory = m_Factory;
    dmScript::HContext second = dmScript::NewContext(params);
    dmScript::Initialize(second);
    m_ModuleContext.m_ScriptContexts.SetCapacity(2);
    m_ModuleContext.m_ScriptContexts.Push(second);

    const char* initial = "return { value = 123 }";
    dmLuaDDF::LuaModule module = {};
    module.m_Source.m_Bytecode.m_Data = (uint8_t*)initial;
    module.m_Source.m_Bytecode.m_Count = strlen(initial);
    module.m_Source.m_Filename = "reload_registered.lua";
    dmArray<uint8_t> data;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&module, dmLuaDDF::LuaModule::m_DDFDescriptor, data));
    dmGameObject::LuaScript* resource = 0;
    const char* path = "/reload_registered.luac";
    dmhash_t path_hash = dmHashString64(path);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, path, data.Begin(), data.Size(), (void**)&resource));
    ASSERT_EQ(dmScript::RESULT_OK, dmScript::AddModule(second, &resource->m_LuaModule->m_Source, "reload_registered", resource, path_hash));

    ReloadModuleRegistration registration = { m_Factory, m_ScriptContext, resource, path_hash, 0 };
    lua_State* second_state = dmScript::GetLuaState(second);
    lua_pushlightuserdata(second_state, &registration);
    lua_pushcclosure(second_state, RegisterModuleDuringReload, 1);
    lua_setglobal(second_state, "register_module");

    const char* next = "if register_module then register_module() end; return { value = 456 }";
    module.m_Source.m_Bytecode.m_Data = (uint8_t*)next;
    module.m_Source.m_Bytecode.m_Count = strlen(next);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&module, dmLuaDDF::LuaModule::m_DDFDescriptor, data));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, path_hash, data.Begin(), data.Size()));
    ASSERT_EQ(456, registration.m_Value);

    lua_State* first_state = dmScript::GetLuaState(m_ScriptContext);
    ASSERT_EQ(0, luaL_dostring(first_state, "package.loaded.reload_registered = nil; assert(require('reload_registered').value == 456)"));
    dmScript::Finalize(second);
    dmScript::DeleteContext(second);
    m_ModuleContext.m_ScriptContexts.SetSize(1);
}

static void ModuleWithSource(dmArray<uint8_t>& data, const char* path, const char* source)
{
    dmLuaDDF::LuaModule module = {};
    module.m_Source.m_Script.m_Data = (uint8_t*)source;
    module.m_Source.m_Script.m_Count = strlen(source);
    module.m_Source.m_Filename = path;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&module, dmLuaDDF::LuaModule::m_DDFDescriptor, data));
}

static bool RegisterResourceModule(dmResource::HFactory factory, dmScript::HContext context, const char* name, const char* path)
{
    dmLuaDDF::LuaModule module = {};
    module.m_Modules.m_Data = &name;
    module.m_Modules.m_Count = 1;
    module.m_Resources.m_Data = &path;
    module.m_Resources.m_Count = 1;
    return dmGameObject::RegisterSubModules(factory, context, &module);
}

// File-backed modules release code before require, retry read errors, and load independently into another context.
TEST_F(ScriptTest, ModuleCodeLoadsOnDemand)
{
    const char* path = "/on_demand.luac";
    dmArray<uint8_t> data;
    ModuleWithSource(data, path, "return { value = 111 }");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, data.Size(), data.Begin()));
    ASSERT_TRUE(RegisterResourceModule(m_Factory, m_ScriptContext, "on_demand", path));
    dmGameObject::LuaScript* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path, (void**)&resource));
    ASSERT_EQ(0, resource->m_LuaModule->m_Source.m_Script.m_Count);
    ASSERT_EQ(0, resource->m_LuaModule->m_Source.m_Bytecode.m_Count);
    dmResource::Release(m_Factory, resource);

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
    ASSERT_EQ(0, luaL_dostring(L, "assert(not pcall(require, 'on_demand')); assert(package.loaded.on_demand == nil)"));
    dmArray<uint8_t> invalid;
    ModuleWithSource(invalid, path, "local =");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, invalid.Size(), invalid.Begin()));
    ASSERT_EQ(0, luaL_dostring(L, "assert(not pcall(require, 'on_demand')); assert(package.loaded.on_demand == nil)"));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, data.Size(), data.Begin()));
    ASSERT_EQ(0, luaL_dostring(L, "assert(require('on_demand').value == 111); package.loaded.on_demand = nil; assert(require('on_demand').value == 111)"));

    dmScript::ContextParams params = {};
    params.m_Factory = m_Factory;
    dmScript::HContext second = dmScript::NewContext(params);
    dmScript::Initialize(second);
    ASSERT_TRUE(RegisterResourceModule(m_Factory, second, "on_demand", path));
    ASSERT_EQ(0, luaL_dostring(dmScript::GetLuaState(second), "require('on_demand').value = 222"));
    ASSERT_EQ(0, luaL_dostring(L, "assert(require('on_demand').value == 111)"));
    dmScript::Finalize(second);
    dmScript::DeleteContext(second);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
}

// Uncached coroutine require returns its chunk and errors on the coroutine stack, leaving the main stack balanced.
TEST_F(ScriptTest, ModuleCodeLoadsFromCoroutine)
{
    const char* path = "/coroutine_module.luac";
    dmArray<uint8_t> data;
    ModuleWithSource(data, path, "return { value = 111 }");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, data.Size(), data.Begin()));
    ASSERT_TRUE(RegisterResourceModule(m_Factory, m_ScriptContext, "coroutine_module", path));
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    int top = lua_gettop(L);
    ASSERT_EQ(0, luaL_dostring(L,
                               "local co = coroutine.create(function() return require('coroutine_module').value end); "
                               "local ok, value = coroutine.resume(co); assert(ok, value); assert(value == 111)"));
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
    ASSERT_EQ(0, luaL_dostring(L,
                               "package.loaded.coroutine_module = nil; "
                               "local co = coroutine.create(function() return pcall(require, 'coroutine_module') end); "
                               "local resumed, loaded, err = coroutine.resume(co); assert(resumed); assert(not loaded); "
                               "assert(err:find('/coroutine_module.luac', 1, true))"));
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, data.Size(), data.Begin()));
    ASSERT_EQ(0, luaL_dostring(L,
                               "local co = coroutine.create(function() return require('coroutine_module').value end); "
                               "local ok, value = coroutine.resume(co); assert(ok, value); assert(value == 111)"));
    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
}

// Deferred require registers dependencies from the fetched code and can retry after a missing dependency becomes available.
TEST_F(ScriptTest, ModuleCodeLoadsUpdatedDependencies)
{
    const char* path = "/updated_dependencies.luac";
    const char* dependency_path = "/updated_dependency.luac";
    const char* dependency_name = "updated_dependency";
    dmArray<uint8_t> initial, replacement, dependency;
    ModuleWithSource(initial, path, "return { value = 111 }");
    ModuleWithSource(dependency, dependency_path, "return { value = 456 }");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, initial.Size(), initial.Begin()));
    ASSERT_TRUE(RegisterResourceModule(m_Factory, m_ScriptContext, "updated_dependencies", path));

    const char* source = "return require('updated_dependency')";
    dmLuaDDF::LuaModule module = {};
    module.m_Source.m_Script.m_Data = (uint8_t*)source;
    module.m_Source.m_Script.m_Count = strlen(source);
    module.m_Source.m_Filename = path;
    module.m_Modules.m_Data = &dependency_name;
    module.m_Modules.m_Count = 1;
    module.m_Resources.m_Data = &dependency_path;
    module.m_Resources.m_Count = 1;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&module, dmLuaDDF::LuaModule::m_DDFDescriptor, replacement));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, replacement.Size(), replacement.Begin()));

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    ASSERT_EQ(0, luaL_dostring(L, "assert(not pcall(require, 'updated_dependencies')); assert(package.loaded.updated_dependencies == nil)"));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, dependency_path, dependency.Size(), dependency.Begin()));
    ASSERT_EQ(0, luaL_dostring(L,
                               "local co = coroutine.create(function() return require('updated_dependencies').value end); "
                               "local ok, value = coroutine.resume(co); assert(ok, value); assert(value == 456)"));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, dependency_path));
}

// Recreated code remains available after its input is freed, without falling back to stale archive code.
TEST_F(ScriptTest, RecreatedModuleKeepsCurrentCode)
{
    const char* path = "/on_demand_reload.luac";
    dmArray<uint8_t> data;
    ModuleWithSource(data, path, "return { value = 111 }");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, data.Size(), data.Begin()));
    ASSERT_TRUE(RegisterResourceModule(m_Factory, m_ScriptContext, "on_demand_reload", path));
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    ASSERT_EQ(0, luaL_dostring(L, "assert(require('on_demand_reload').value == 111)"));

    ModuleWithSource(data, path, "return { value = 333 }");
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64(path), data.Begin(), data.Size()));
    data.SetSize(0);
    data.SetCapacity(0);
    ASSERT_EQ(0, luaL_dostring(L, "package.loaded.on_demand_reload = nil; assert(require('on_demand_reload').value == 333)"));

    dmScript::ContextParams params = {};
    params.m_Factory = m_Factory;
    dmScript::HContext second = dmScript::NewContext(params);
    dmScript::Initialize(second);
    ASSERT_TRUE(RegisterResourceModule(m_Factory, second, "on_demand_reload", path));
    ASSERT_EQ(0, luaL_dostring(dmScript::GetLuaState(second), "assert(require('on_demand_reload').value == 333)"));
    dmScript::Finalize(second);
    dmScript::DeleteContext(second);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, path));
}

// Runtime-created modules have no archive backing and must retain their supplied code for deferred require.
TEST_F(ScriptTest, CreatedModuleKeepsSuppliedCode)
{
    const char* path = "/created_module.luac";
    dmArray<uint8_t> data;
    ModuleWithSource(data, path, "return { value = 777 }");
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, path, data.Begin(), data.Size(), &resource));
    ASSERT_TRUE(RegisterResourceModule(m_Factory, m_ScriptContext, "created_module", path));
    dmResource::Release(m_Factory, resource);
    data.SetSize(0);
    data.SetCapacity(0);
    ASSERT_EQ(0, luaL_dostring(dmScript::GetLuaState(m_ScriptContext), "assert(require('created_module').value == 777); package.loaded.created_module = nil; assert(require('created_module').value == 777)"));
}

static void ModuleWithDependency(dmArray<uint8_t>& data, const char* path, const char* dependency)
{
    const char* source = "return {}";
    dmLuaDDF::LuaModule module = {};
    module.m_Source.m_Script.m_Data = (uint8_t*)source;
    module.m_Source.m_Script.m_Count = strlen(source);
    module.m_Source.m_Filename = path;
    module.m_Resources.m_Data = &dependency;
    module.m_Resources.m_Count = dependency ? 1 : 0;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&module, dmLuaDDF::LuaModule::m_DDFDescriptor, data));
}

// Lua recreation replaces hint-only dependency edges even when no script context holds the module.
TEST_F(ScriptTest, RecreateModuleDependencies)
{
    const char* paths[] = { "/reload_parent.luac", "/reload_previous.luac", "/reload_next.luac" };
    dmArray<uint8_t> data[3];
    void* resources[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        ModuleWithDependency(data[i], paths[i], i == 0 ? paths[1] : 0);
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, paths[i], data[i].Size(), data[i].Begin()));
    }
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, paths[1], &resources[1]));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, paths[2], &resources[2]));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, paths[0], &resources[0]));
    dmArray<uint8_t> replacement;
    ModuleWithDependency(replacement, paths[0], paths[2]);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64(paths[0]), replacement.Begin(), replacement.Size()));
    ModuleWithDependency(replacement, paths[1], paths[0]);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64(paths[1]), replacement.Begin(), replacement.Size()));
    ModuleWithDependency(replacement, paths[2], paths[0]);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, dmResource::SetResource(m_Factory, dmHashString64(paths[2]), replacement.Begin(), replacement.Size()));
    for (uint32_t i = 0; i < 3; ++i)
    {
        dmResource::Release(m_Factory, resources[i]);
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, paths[i]));
    }
}

TEST_F(ScriptTest, TestURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/url.goc");
    ASSERT_NE(0, go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_id"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(ScriptTest, TestURLNilId)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/url_nil_id.goc");
    ASSERT_NE(0, go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_id"));

    ASSERT_FALSE(dmGameObject::Init(m_Collection));
}

#define REF_VALUE "__ref_value"

int TestRef(lua_State* L)
{
    lua_getglobal(L, REF_VALUE);
    int* ref = (int*)lua_touserdata(L, -1);
    dmScript::GetInstance(L);
    *ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1);
    return 0;
}

TEST_F(ScriptTest, TestInstanceCallback)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    lua_register(L, "test_ref", TestRef);

    int ref = LUA_NOREF;

    lua_pushlightuserdata(L, &ref);
    lua_setglobal(L, REF_VALUE);

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/instance_ref.goc");
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_NE(ref, LUA_NOREF);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_TRUE(dmScript::IsInstanceValid(L));

    // Verify that the original GO lookup and the typed overload resolve the same instance,
    // while an unrelated script instance type is rejected.
    dmScript::GetInstance(L);
    uint32_t script_instance_type_hash = dmScript::GetUserType(L, -1);
    lua_pop(L, 1);

    ASSERT_EQ(go, dmGameObject::GetInstanceFromLua(L));
    ASSERT_EQ(go, dmGameObject::GetInstanceFromLua(L, script_instance_type_hash));
    ASSERT_EQ((dmGameObject::HInstance)0, dmGameObject::GetInstanceFromLua(L, dmHashString32("incorrect_type")));

    ASSERT_EQ(m_Collection, dmGameObject::GetCollection(dmGameObject::GetInstanceFromLua(L)));

    dmGameObject::Delete(m_Collection, go, false);

    dmGameObject::PostUpdate(m_Collection);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_FALSE(dmScript::IsInstanceValid(L));
}

#undef REF_VALUE

// Makes sure we get to test the component indices and payloads properly DEF-2979
TEST_F(ScriptTest, TestScriptMany)
{
    dmHashEnableReverseHash(true); // while debugging

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/many.goc");
    ASSERT_NE(0, go);

    char name[64];
    uint16_t component_index;
    dmhash_t component_id;
    const uint32_t num_components = 300;
    for( uint32_t i = 0; i < num_components; ++i)
    {
        dmSnPrintf(name, sizeof(name), "script%d", i);
        dmhash_t id = dmHashString64(name);
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(go, id, &component_index));
        ASSERT_EQ(i, component_index);

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentId(go, component_index, &component_id));
        ASSERT_EQ(id, component_id);
    }
    dmSnPrintf(name, sizeof(name), "script%d", num_components);
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentIndex(go, dmHashString64(name), &component_index));
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentId(go, num_components, &component_id));

    dmGameObject::Init(m_Collection);

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    DM_LUA_STACK_CHECK(L, 0);

    for( uint32_t i = 0; i < num_components; ++i)
    {

        if (i == 0) { // a.script
            lua_getglobal(L, "globalvar_a");
            int value = lua_tointeger(L, -1);
            lua_pop(L, 1);
            ASSERT_EQ(1, value);

            lua_getglobal(L, "globalvar_a_name");
            dmhash_t name = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            ASSERT_EQ(dmHashString64("script0"), name);
        }
        else if (i == 256) { // b.script
            lua_getglobal(L, "globalvar_b");
            int value = lua_tointeger(L, -1);
            lua_pop(L, 1);
            ASSERT_EQ(20, value);

            lua_getglobal(L, "globalvar_b_name");
            dmhash_t name = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            ASSERT_EQ(dmHashString64("script256"), name);
        }
    }

    dmGameObject::Delete(m_Collection, go, false);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

int TestSetInstanceContext(lua_State* L)
{
    if(!dmScript::IsInstanceValid(L))
    {
        return 0;
    }

    lua_pushstring(L, "__my_context_value");
    lua_pushnumber(L, 81233);
    if(!dmScript::SetInstanceContextValue(L))
    {
        return 0;
    }

    return 0;
}

int TestGetInstanceContext(lua_State* L)
{
    if(!dmScript::IsInstanceValid(L))
    {
        return 0;
    }

    lua_pushstring(L, "__my_context_value");
    dmScript::GetInstanceContextValue(L);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return 0;
    }
    lua_Number number = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (81233 != number)
    {
        return 0;
    }

    lua_pushboolean(L, 1);
    lua_setglobal(L, "INSTANCE_CONTEXT_SUCCESFUL");

    return 0;
}

TEST_F(ScriptTest, TestInstanceContext)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    lua_register(L, "test_set_instance_context", TestSetInstanceContext);
    lua_register(L, "test_get_instance_context", TestGetInstanceContext);

    lua_pushboolean(L, 0);
    lua_setglobal(L, "INSTANCE_CONTEXT_SUCCESFUL");

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/instance_context.goc");
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    lua_getglobal(L, "INSTANCE_CONTEXT_SUCCESFUL");
    ASSERT_EQ(0, lua_toboolean(L, -1));
    lua_pop(L, 1);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    lua_getglobal(L, "INSTANCE_CONTEXT_SUCCESFUL");
    ASSERT_EQ(1, lua_toboolean(L, -1));
    lua_pop(L, 1);

    dmGameObject::Delete(m_Collection, go, false);

    dmGameObject::PostUpdate(m_Collection);
}


TEST_F(ScriptTest, TestExists)
{
    dmGameObject::HInstance go_null = dmGameObject::New(m_Collection, "/null.goc");
    ASSERT_NE(0, go_null);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go_null, "a"));

    dmGameObject::HInstance go_exists = dmGameObject::New(m_Collection, "/exists.goc");
    ASSERT_NE(0, go_exists);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go_exists, "b"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
}

static void RunWorldToLocalPositionTest(
    ScriptTest* t,
    const Matrix4& target_world,
    const Vector3& world_input,
    const Vector3& expected_local)
{
    lua_State* L = dmScript::GetLuaState(t->m_ScriptContext);

    dmScript::PushVector3(L, world_input);
    lua_setglobal(L, "WORLD_POS_INPUT");
    lua_pushnumber(L, expected_local.getX()); lua_setglobal(L, "EXPECTED_X");
    lua_pushnumber(L, expected_local.getY()); lua_setglobal(L, "EXPECTED_Y");
    lua_pushnumber(L, expected_local.getZ()); lua_setglobal(L, "EXPECTED_Z");

    dmGameObject::HInstance tester = dmGameObject::New(t->m_Collection, "/world_to_local_pos.goc");
    ASSERT_NE((dmGameObject::HInstance)0, tester);

    dmGameObject::HInstance target = dmGameObject::New(t->m_Collection, "/null.goc");
    ASSERT_NE((dmGameObject::HInstance)0, target);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(t->m_Collection, target, "target"));

    ASSERT_TRUE(dmGameObject::Init(t->m_Collection));

    SetCachedWorldTransform(t->m_Collection, target, target_world);

    ASSERT_TRUE(dmGameObject::Update(t->m_Collection, &t->m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(t->m_Collection));
    dmGameObject::Delete(t->m_Collection, tester, false);
    dmGameObject::Delete(t->m_Collection, target, false);
}

// World point coincides with the target origin: local result must be (0,0,0).
TEST_F(ScriptTest, WorldToLocalPosition_PointAtTargetOrigin)
{
    Vector3 target_pos(10.0f, 5.0f, -3.0f);
    RunWorldToLocalPositionTest(this,
        Matrix4::translation(target_pos),
        target_pos,         // world_input == target world origin
        Vector3(0, 0, 0));  // expected local
}

// World point offset from target along X: local result must reflect that pure offset.
TEST_F(ScriptTest, WorldToLocalPosition_OffsetAlongX)
{
    Vector3 target_pos(10.0f, 0.0f, 0.0f);
    Vector3 world_input(25.0f, 0.0f, 0.0f);
    RunWorldToLocalPositionTest(this,
        Matrix4::translation(target_pos),
        world_input,
        Vector3(15.0f, 0.0f, 0.0f)); // 25 - 10 = 15 in local X
}

static void RunWorldToLocalTransformTest(
    ScriptTest* t,
    const Matrix4& target_world,
    const Matrix4& world_input,
    const Vector3& expected_local_translation)
{
    lua_State* L = dmScript::GetLuaState(t->m_ScriptContext);

    dmScript::PushMatrix4(L, world_input);
    lua_setglobal(L, "WORLD_TRANSFORM_INPUT");
    lua_pushnumber(L, expected_local_translation.getX()); lua_setglobal(L, "EXPECTED_TX");
    lua_pushnumber(L, expected_local_translation.getY()); lua_setglobal(L, "EXPECTED_TY");
    lua_pushnumber(L, expected_local_translation.getZ()); lua_setglobal(L, "EXPECTED_TZ");

    dmGameObject::HInstance tester = dmGameObject::New(t->m_Collection, "/world_to_local_transform.goc");
    ASSERT_NE((dmGameObject::HInstance)0, tester);

    dmGameObject::HInstance target = dmGameObject::New(t->m_Collection, "/null.goc");
    ASSERT_NE((dmGameObject::HInstance)0, target);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(t->m_Collection, target, "target"));

    ASSERT_TRUE(dmGameObject::Init(t->m_Collection));

    SetCachedWorldTransform(t->m_Collection, target, target_world);

    ASSERT_TRUE(dmGameObject::Update(t->m_Collection, &t->m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(t->m_Collection));
    dmGameObject::Delete(t->m_Collection, tester, false);
    dmGameObject::Delete(t->m_Collection, target, false);
}

// Input transform at same position as target: local translation must be (0,0,0).
TEST_F(ScriptTest, WorldToLocalTransform_IdentityRelativeTransform)
{
    Vector3 pos(10.0f, 5.0f, -3.0f);
    RunWorldToLocalTransformTest(this,
        Matrix4::translation(pos),
        Matrix4::translation(pos),
        Vector3(0.0f, 0.0f, 0.0f));
}

// Input transform offset from target along X: local translation must reflect that offset.
TEST_F(ScriptTest, WorldToLocalTransform_OffsetAlongX)
{
    Vector3 target_pos(10.0f, 0.0f, 0.0f);
    Vector3 input_pos(25.0f, 0.0f, 0.0f);
    RunWorldToLocalTransformTest(this,
        Matrix4::translation(target_pos),
        Matrix4::translation(input_pos),
        Vector3(15.0f, 0.0f, 0.0f)); // 25 - 10 = 15 in local X
}

// --- Tests derived from forum report ---
// https://forum.defold.com/t/trouble-with-go-world-to-local-position-seems-to-be-adding-positions-together/80333
//
// The old broken implementation multiplied world_transform * go_transform (forward),
// which effectively *added* positions instead of subtracting them.
// e.g. click at (268.5, 264.5), object at (250, 250) → got (518.5, 514.5) instead of (18.5, 14.5).

// Reproduces the exact numbers from the first forum reporter.
// Object at (250, 250), click at (268.5, 264.5) → local offset must be (18.5, 14.5), not the sum (518.5, 514.5).
TEST_F(ScriptTest, WorldToLocalPosition_ForumReport_ClickOffset)
{
    RunWorldToLocalPositionTest(this,
        Matrix4::translation(Vector3(250.0f, 250.0f, 0.0f)),
        Vector3(268.5f, 264.5f, 0.0f),
        Vector3(18.5f, 14.5f, 0.0f));
}

// Reproduces the second reporter's case: object's own world position → local must be (0,0,0), not (2,0,0).
TEST_F(ScriptTest, WorldToLocalPosition_ForumReport_OwnPositionIsZero)
{
    Vector3 obj_world_pos(2.0f, 0.0f, 0.0f);
    RunWorldToLocalPositionTest(this,
        Matrix4::translation(obj_world_pos),
        obj_world_pos,
        Vector3(0.0f, 0.0f, 0.0f));
}

// Verifies that a 45-degree rotation is correctly accounted for in the inverse.
// Object at (10,10) rotated 45 deg CCW around Z.
// World point that corresponds to local (3,4) computed as: world = T + R * local.
TEST_F(ScriptTest, WorldToLocalPosition_ForumReport_45DegRotation)
{
    Matrix4 rot = Matrix4::rotationZ(M_PI * 0.25f);
    Matrix4 target_world = Matrix4::translation(Vector3(10.0f, 10.0f, 0.0f)) * rot;

    // world = (10,10) + R(45) * (3,4): verified numerically as (9.2929, 14.9497)
    RunWorldToLocalPositionTest(this,
        target_world,
        Vector3(9.292893f, 14.949747f, 0.0f),
        Vector3(3.0f, 4.0f, 0.0f));
}

// Verifies that a 90-degree rotation is correctly accounted for in the inverse.
// Object at (10,10) rotated 90 deg around Z. World point directly above object in world space
// maps to a point along local X after inverse-rotating.
TEST_F(ScriptTest, WorldToLocalPosition_ForumReport_WithRotation)
{
    // 90 deg CCW around Z: local X -> world Y, local Y -> -world X
    Matrix4 rot = Matrix4::rotationZ(M_PI * 0.5f);
    Matrix4 target_world = Matrix4::translation(Vector3(10.0f, 10.0f, 0.0f)) * rot;

    // A point 5 units along world Y from the object origin.
    // R(90 CCW) maps local X -> world Y, so inverse maps world Y -> local X: result is +5 on local X.
    Vector3 world_input(10.0f, 15.0f, 0.0f);
    RunWorldToLocalPositionTest(this,
        target_world,
        world_input,
        Vector3(5.0f, 0.0f, 0.0f));
}
