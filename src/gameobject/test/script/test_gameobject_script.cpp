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
#include "../proto/gameobject_ddf.h"

using namespace Vectormath::Aos;

class ScriptTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();
        dmGameObject::RegisterDDFType(TestGameObjectDDF::Spawn::m_DDFDescriptor);

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/default/src/gameobject/test/script";
        m_Factory = dmResource::NewFactory(&params, m_Path);
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

public:

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    const char* m_Path;
};

TEST_F(ScriptTest, TestScriptProperty)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "script_property.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::SetScriptIntProperty(go, "my_int_prop", 1010);
    dmGameObject::SetScriptFloatProperty(go, "my_float_prop", 1.0);
    dmGameObject::SetScriptStringProperty(go, "my_string_prop", "a string prop");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

struct TestScript01Context
{
    dmGameObject::HRegister m_Register;
    bool m_Result;
};

void TestScript01Dispatch(dmMessage::Message *message_object, void* user_ptr)
{
    dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;
    TestGameObjectDDF::Spawn* s = (TestGameObjectDDF::Spawn*) instance_message_data->m_Buffer;
    // NOTE: We relocate the string here (from offset to pointer)
    s->m_Prototype = (const char*) ((uintptr_t) s->m_Prototype + (uintptr_t) s);
    TestScript01Context* context = (TestScript01Context*)user_ptr;
    bool* dispatch_result = &context->m_Result;

    TestGameObjectDDF::SpawnResult result;
    result.m_Status = 1010;
    dmGameObject::PostDDFMessageTo(instance_message_data->m_Instance, 0x0, TestGameObjectDDF::SpawnResult::m_DDFDescriptor, (char*)&result);

    *dispatch_result = s->m_Pos.getX() == 1.0 && s->m_Pos.getY() == 2.0 && s->m_Pos.getZ() == 3.0 && strcmp("test", s->m_Prototype) == 0;
}

void TestScript01DispatchReply(dmMessage::Message *message_object, void* user_ptr)
{

}

TEST_F(ScriptTest, TestScript01)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "go1.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "my_object01"));

    TestGameObjectDDF::GlobalData global_data;
    global_data.m_UIntValue = 12345;
    global_data.m_IntValue = -123;
    global_data.m_StringValue = "string_value";
    global_data.m_VecValue.setX(1.0f);
    global_data.m_VecValue.setY(2.0f);
    global_data.m_VecValue.setZ(3.0f);

    dmGameObject::Init(m_Collection);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));

    dmMessage::HSocket socket = dmGameObject::GetMessageSocket(m_Register);
    dmMessage::HSocket reply_socket = dmGameObject::GetReplyMessageSocket(m_Register);
    TestScript01Context context;
    context.m_Register = m_Register;
    context.m_Result = false;
    dmMessage::Dispatch(socket, TestScript01Dispatch, &context);

    ASSERT_TRUE(context.m_Result);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    // Final dispatch to deallocate message data
    dmMessage::Dispatch(socket, TestScript01Dispatch, &context);
    dmMessage::Dispatch(reply_socket, TestScript01DispatchReply, &context);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(&m_Collection, 1, &action, 1));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ScriptTest, TestFailingScript02)
{
    // Test init failure

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    dmGameObject::New(m_Collection, "go2.goc");
    bool result = dmGameObject::Init(m_Collection);
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    ASSERT_FALSE(result);
}

TEST_F(ScriptTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "go3.goc");
    ASSERT_NE((void*) 0, (void*) go);

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    ASSERT_FALSE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ScriptTest, TestFailingScript04)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "go4.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

static void CreateFile(const char* file_name, const char* contents)
{
    FILE* f;
    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "%s", contents);
    fclose(f);
}

TEST_F(ScriptTest, TestReload)
{
    const char* script_file_name = "__test__.scriptc";
    char script_path[512];
    DM_SNPRINTF(script_path, sizeof(script_path), "%s/%s", m_Path, script_file_name);

    char go_file_name[512];
    DM_SNPRINTF(go_file_name, sizeof(go_file_name), "%s/%s", m_Path, "__go__.goc");

    dmGameObjectDDF::PrototypeDesc prototype;
    //memset(&prototype, 0, sizeof(prototype));
    prototype.m_Components.m_Count = 1;
    dmGameObjectDDF::ComponentDesc component_desc;
    memset(&component_desc, 0, sizeof(component_desc));
    component_desc.m_Resource = script_file_name;
    prototype.m_Components.m_Data = &component_desc;

    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, go_file_name);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    CreateFile(script_path,
               "function update(self)\n"
               "    go.set_position(self, vmath.vector3(1,2,3))\n"
               "end\n");

    dmGameObject::HInstance go;
    go = dmGameObject::New(m_Collection, "__go__.goc");
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    dmGameObject::Update(&m_Collection, 0, 1);
    Point3 p1 = dmGameObject::GetPosition(go);
    ASSERT_EQ(1, p1.getX());
    ASSERT_EQ(2, p1.getY());
    ASSERT_EQ(3, p1.getZ());

    dmTime::Sleep(1000000); // TODO: Currently seconds time resolution in modification time

    CreateFile(script_path,
               "function update(self)\n"
               "    go.set_position(self, vmath.vector3(10,20,30))\n"
               "end\n");

    uint32_t type;
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, dmResource::GetTypeFromExtension(m_Factory, "scriptc", &type));
    dmResource::FactoryResult fr = dmResource::ReloadType(m_Factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, fr);

    dmGameObject::Update(&m_Collection, 0, 1);
    Point3 p2 = dmGameObject::GetPosition(go);
    ASSERT_EQ(10, p2.getX());
    ASSERT_EQ(20, p2.getY());
    ASSERT_EQ(30, p2.getZ());

    unlink(script_path);
    fr = dmResource::ReloadType(m_Factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_RESOURCE_NOT_FOUND, fr);

    unlink(go_file_name);
}


TEST_F(ScriptTest, Null)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "null.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::PostNamedMessageTo(go, 0, dmHashString64("test"), 0x0, 0));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(&m_Collection, 1, &action, 1));

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
