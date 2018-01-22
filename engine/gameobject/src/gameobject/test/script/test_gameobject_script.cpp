#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/script/test_gameobject_script_ddf.h"
#include "../proto/gameobject/gameobject_ddf.h"
#include "../proto/gameobject/lua_ddf.h"

using namespace Vectormath::Aos;

class ScriptTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/default/src/gameobject/test/script";
        m_Factory = dmResource::NewFactory(&params, m_Path);
        m_ScriptContext = dmScript::NewContext(0, m_Factory, true);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        m_ModuleContext.m_ScriptContexts.SetCapacity(1);
        m_ModuleContext.m_ScriptContexts.Push(m_ScriptContext);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0);
        dmMessage::Result result = dmMessage::NewSocket("@system", &m_Socket);
        assert(result == dmMessage::RESULT_OK);
    }

    virtual void TearDown()
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
};

struct TestScript01Context
{
    dmGameObject::HRegister m_Register;
    bool m_Result;
};

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
    ASSERT_NE((void*) 0, (void*) go);

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

TEST_F(ScriptTest, TestFailingScript02)
{
    // Test init failure

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    dmGameObject::New(m_Collection, "/go2.goc");
    bool result = dmGameObject::Init(m_Collection);
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    EXPECT_FALSE(result);
    result = dmGameObject::Final(m_Collection);
    EXPECT_FALSE(result);
}

TEST_F(ScriptTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go3.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestFailingScript04)
{
    // Test update failure, lua update-identifier used for something else than function callback
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go4.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, TestFailingScript05)
{
    // Test posting to missing component id
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go5.goc");
    ASSERT_NE((void*) 0, (void*) go);
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

TEST_F(ScriptTest, TestReload)
{
    const char* script_resource_name = "/__test__.scriptc";
    char script_file_name[512];
    DM_SNPRINTF(script_file_name, sizeof(script_file_name), "/%s%s", m_Path, script_resource_name);

    const char* go_resource_name = "/__go__.goc";
    char go_file_name[512];
    DM_SNPRINTF(go_file_name, sizeof(go_file_name), "/%s%s", m_Path, go_resource_name);

    dmGameObjectDDF::PrototypeDesc prototype;
    memset(&prototype, 0, sizeof(prototype));
    prototype.m_Components.m_Count = 1;
    dmGameObjectDDF::ComponentDesc component_desc;
    memset(&component_desc, 0, sizeof(component_desc));
    component_desc.m_Id = "script";
    component_desc.m_Component = script_resource_name;
    prototype.m_Components.m_Data = &component_desc;

    // NOTE: +1 to remove /
    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, go_file_name + 1);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name + 1,
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
    CreateScriptFile(script_file_name + 1,
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

    // NOTE: +1 to remove /
    unlink(script_file_name + 1);
    rr = dmResource::ReloadResource(m_Factory, script_resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    unlink(go_file_name);
}

TEST_F(ScriptTest, Null)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/null.goc");
    ASSERT_NE((void*) 0, (void*) go);

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
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::Init(m_Collection);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestReloadModule)
{
    const char* script_resource_name = "/test_reload_mod.luac";
    char script_file_name[512];
    DM_SNPRINTF(script_file_name, sizeof(script_file_name), "/%s%s", m_Path, script_resource_name);

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name + 1,
               "function test_reload_mod_func()\n"
               "    return 1010\n"
               "end\n");

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/reload.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::Init(m_Collection);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // NOTE: +1 to remove /
    CreateScriptFile(script_file_name + 1,
               "function test_reload_mod_func()\n"
               "    return 2020\n"
               "end\n");

    dmTime::Sleep(1000000); // TODO: Currently seconds time resolution in modification time
    dmResource::Result rr = dmResource::ReloadResource(m_Factory, script_resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ScriptTest, TestURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/url.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_id"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(ScriptTest, TestURLNilId)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/url_nil_id.goc");
    ASSERT_NE((void*) 0, (void*) go);

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
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_NE(ref, LUA_NOREF);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    dmScript::SetInstance(L);
    ASSERT_TRUE(dmScript::IsInstanceValid(L));

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
    ASSERT_NE((void*) 0, (void*) go);

    char name[64];
    uint16_t component_index;
    dmhash_t component_id;
    const uint32_t num_components = 300;
    for( uint32_t i = 0; i < num_components; ++i)
    {
        DM_SNPRINTF(name, sizeof(name), "script%d", i);
        dmhash_t id = dmHashString64(name);
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(go, id, &component_index));
        ASSERT_EQ(i, component_index);

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentId(go, component_index, &component_id));
        ASSERT_EQ(id, component_id);
    }
    DM_SNPRINTF(name, sizeof(name), "script%d", num_components);
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


int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
