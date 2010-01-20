#include <gtest/gtest.h>

#include <map>
#include <dlib/hash.h>
#include <dlib/event.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include "../resource.h"
#include "../gameobject.h"
#include "gamesys/test/test_resource_ddf.h"
#include "../proto/gameobject_ddf.h"

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        update_context.m_DT = 1.0f / 60.0f;
        update_context.m_GlobalData = 0;
        update_context.m_DDFGlobalDataDescriptor = 0;

        factory = dmResource::NewFactory(16, "build/default/src/gamesys/test", RESOURCE_FACTORY_FLAGS_EMPTY);
        collection = dmGameObject::NewCollection();
        dmGameObject::RegisterResourceTypes(factory);

        // Register dummy physical resource type
        dmResource::FactoryResult e;
        e = dmResource::RegisterType(factory, "pc", this, PhysCreate, PhysDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "a", this, ACreate, ADestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "b", this, BCreate, BDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "c", this, CCreate, CDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;

        e = dmResource::GetTypeFromExtension(factory, "pc", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::Result result = dmGameObject::RegisterComponentType(collection, resource_type, this, PhysComponentCreate, PhysComponentDestroy, PhysComponentsUpdate, false);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(factory, "a", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        result = dmGameObject::RegisterComponentType(collection, resource_type, this, AComponentCreate, AComponentDestroy, AComponentsUpdate, true);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = dmResource::GetTypeFromExtension(factory, "b", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        result = dmGameObject::RegisterComponentType(collection, resource_type, this, BComponentCreate, BComponentDestroy, BComponentsUpdate, false);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // C has component_user_data
        e = dmResource::GetTypeFromExtension(factory, "c", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        result = dmGameObject::RegisterComponentType(collection, resource_type, this, CComponentCreate, CComponentDestroy, CComponentsUpdate, true);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(collection, factory);
        dmResource::DeleteFactory(factory);
    }

    static dmResource::FResourceCreate    PhysCreate;
    static dmResource::FResourceDestroy   PhysDestroy;
    static dmGameObject::ComponentCreate  PhysComponentCreate;
    static dmGameObject::ComponentDestroy PhysComponentDestroy;
    static dmGameObject::ComponentsUpdate PhysComponentsUpdate;

    static dmResource::FResourceCreate    ACreate;
    static dmResource::FResourceDestroy   ADestroy;
    static dmGameObject::ComponentCreate  AComponentCreate;
    static dmGameObject::ComponentDestroy AComponentDestroy;
    static dmGameObject::ComponentsUpdate AComponentsUpdate;

    static dmResource::FResourceCreate    BCreate;
    static dmResource::FResourceDestroy   BDestroy;
    static dmGameObject::ComponentCreate  BComponentCreate;
    static dmGameObject::ComponentDestroy BComponentDestroy;
    static dmGameObject::ComponentsUpdate BComponentsUpdate;

    static dmResource::FResourceCreate    CCreate;
    static dmResource::FResourceDestroy   CDestroy;
    static dmGameObject::ComponentCreate  CComponentCreate;
    static dmGameObject::ComponentDestroy CComponentDestroy;
    static dmGameObject::ComponentsUpdate CComponentsUpdate;

public:

    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    std::map<uint64_t, uint32_t> m_ComponentCreateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentDestroyCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_MaxComponentCreateCountMap;

    std::map<uint64_t, int>      m_ComponentUserDataAcc;

    dmGameObject::UpdateContext update_context;
    dmGameObject::HCollection collection;
    dmResource::HFactory factory;
};

template <typename T>
dmResource::CreateResult GenericDDFCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_CreateCountMap[T::m_DDFHash]++;

    T* obj;
    dmDDF::Result e = dmDDF::LoadMessage<T>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

template <typename T>
dmResource::CreateResult GenericDDFDestory(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_DestroyCountMap[T::m_DDFHash]++;

    dmDDF::FreeMessage((void*) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}

template <typename T, int add_to_user_data>
static dmGameObject::CreateResult GenericComponentCreate(dmGameObject::HCollection collection,
                                                         dmGameObject::HInstance instance,
                                                         void* resource,
                                                         void* context,
                                                         uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;

    if (user_data && add_to_user_data != -1)
    {
        *user_data += add_to_user_data;
    }

    if (game_object_test->m_MaxComponentCreateCountMap.find(T::m_DDFHash) != game_object_test->m_MaxComponentCreateCountMap.end())
    {
        if (game_object_test->m_ComponentCreateCountMap[T::m_DDFHash] >= game_object_test->m_MaxComponentCreateCountMap[T::m_DDFHash])
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    game_object_test->m_ComponentCreateCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static void GenericComponentsUpdate(dmGameObject::HCollection collection,
                                    const dmGameObject::UpdateContext* update_context,
                                    void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* context,
                                                          uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    if (user_data)
    {
        game_object_test->m_ComponentUserDataAcc[T::m_DDFHash] += *user_data;
    }

    game_object_test->m_ComponentDestroyCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate GameObjectTest::PhysCreate              = GenericDDFCreate<TestResource::PhysComponent>;
dmResource::FResourceDestroy GameObjectTest::PhysDestroy            = GenericDDFDestory<TestResource::PhysComponent>;
dmGameObject::ComponentCreate GameObjectTest::PhysComponentCreate   = GenericComponentCreate<TestResource::PhysComponent,-1>;
dmGameObject::ComponentDestroy GameObjectTest::PhysComponentDestroy = GenericComponentDestroy<TestResource::PhysComponent>;
dmGameObject::ComponentsUpdate GameObjectTest::PhysComponentsUpdate = GenericComponentsUpdate<TestResource::PhysComponent>;

dmResource::FResourceCreate GameObjectTest::ACreate              = GenericDDFCreate<TestResource::AResource>;
dmResource::FResourceDestroy GameObjectTest::ADestroy            = GenericDDFDestory<TestResource::AResource>;
dmGameObject::ComponentCreate GameObjectTest::AComponentCreate   = GenericComponentCreate<TestResource::AResource, 1>;
dmGameObject::ComponentDestroy GameObjectTest::AComponentDestroy = GenericComponentDestroy<TestResource::AResource>;
dmGameObject::ComponentsUpdate GameObjectTest::AComponentsUpdate = GenericComponentsUpdate<TestResource::AResource>;

dmResource::FResourceCreate GameObjectTest::BCreate              = GenericDDFCreate<TestResource::BResource>;
dmResource::FResourceDestroy GameObjectTest::BDestroy            = GenericDDFDestory<TestResource::BResource>;
dmGameObject::ComponentCreate GameObjectTest::BComponentCreate   = GenericComponentCreate<TestResource::BResource, -1>;
dmGameObject::ComponentDestroy GameObjectTest::BComponentDestroy = GenericComponentDestroy<TestResource::BResource>;
dmGameObject::ComponentsUpdate GameObjectTest::BComponentsUpdate = GenericComponentsUpdate<TestResource::BResource>;

dmResource::FResourceCreate GameObjectTest::CCreate              = GenericDDFCreate<TestResource::CResource>;
dmResource::FResourceDestroy GameObjectTest::CDestroy            = GenericDDFDestory<TestResource::CResource>;
dmGameObject::ComponentCreate GameObjectTest::CComponentCreate   = GenericComponentCreate<TestResource::CResource, 10>;
dmGameObject::ComponentDestroy GameObjectTest::CComponentDestroy = GenericComponentDestroy<TestResource::CResource>;
dmGameObject::ComponentsUpdate GameObjectTest::CComponentsUpdate = GenericComponentsUpdate<TestResource::CResource>;

TEST_F(GameObjectTest, Test01)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto01.go", 0x0);
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = dmGameObject::Update(collection, go, &update_context);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(collection, go, &update_context);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(collection, go, &update_context);
    ASSERT_TRUE(ret);
    dmGameObject::Delete(collection, factory, go);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto02.go", 0x0);
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::Update(collection, &update_context);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateCountMap[TestResource::PhysComponent::m_DDFHash]);

    dmGameObject::Delete(collection, factory, go);
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto03.go", 0x0);
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto04.go", 0x0);
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1;
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto05.go", 0x0);
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    // One component should get created
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestComponentUserdata)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "goproto06.go", 0x0);
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Delete(collection, factory, go);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestResource::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestResource::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestResource::CResource::m_DDFHash]);
}

void TestScript01Dispatch(dmEvent::Event *event_object, void* user_ptr)
{
    dmGameObject::ScriptEventData* script_event_data = (dmGameObject::ScriptEventData*) event_object->m_Data;
    TestResource::Spawn* s = (TestResource::Spawn*) script_event_data->m_DDFData;
    // NOTE: We relocate the string here (from offset to pointer)
    s->m_Prototype = (const char*) ((uintptr_t) s->m_Prototype + (uintptr_t) s);
    bool* dispatch_result = (bool*) user_ptr;

    uint32_t event_id = dmHashBuffer32("spawn_result", sizeof("spawn_result"));

    uint8_t reply_buf[sizeof(dmGameObject::ScriptEventData) + sizeof(TestResource::SpawnResult)];

    TestResource::SpawnResult* result = (TestResource::SpawnResult*) (reply_buf + sizeof(dmGameObject::ScriptEventData));
    result->m_Status = 1010;

    dmGameObject::ScriptEventData* reply_script_event = (dmGameObject::ScriptEventData*) reply_buf;
    reply_script_event->m_ScriptInstance = script_event_data->m_ScriptInstance;
    reply_script_event->m_DDFDescriptor = TestResource::SpawnResult::m_DDFDescriptor;

    uint32_t reply_socket = dmHashBuffer32(DMGAMEOBJECT_SCRIPT_REPLY_EVENT_SOCKET_NAME, sizeof(DMGAMEOBJECT_SCRIPT_REPLY_EVENT_SOCKET_NAME));
    dmEvent::Post(reply_socket, event_id, reply_buf);

    *dispatch_result = s->m_Pos.m_X == 1.0 && s->m_Pos.m_Y == 2.0 && s->m_Pos.m_Z == 3.0 && strcmp("test", s->m_Prototype) == 0;
}

TEST_F(GameObjectTest, TestScript01)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "testscriptproto01.go", 0x0);
    ASSERT_NE((void*) 0, (void*) go);

    TestResource::GlobalData global_data;
    global_data.m_UIntValue = 12345;
    global_data.m_IntValue = -123;
    global_data.m_StringValue = "string_value";
    global_data.m_VecValue.m_X = 1.0f;
    global_data.m_VecValue.m_Y = 2.0f;
    global_data.m_VecValue.m_Z = 3.0f;

    update_context.m_GlobalData = &global_data;
    update_context.m_DDFGlobalDataDescriptor = TestResource::GlobalData::m_DDFDescriptor;

    ASSERT_TRUE(dmGameObject::Update(collection, go, &update_context));

    uint32_t socket = dmHashBuffer32(DMGAMEOBJECT_SCRIPT_EVENT_SOCKET_NAME, sizeof(DMGAMEOBJECT_SCRIPT_EVENT_SOCKET_NAME));
    bool dispatch_result = false;
    dmEvent::Dispatch(socket, TestScript01Dispatch, &dispatch_result);

    ASSERT_TRUE(dispatch_result);

    ASSERT_TRUE(dmGameObject::Update(collection, &update_context));

    dmGameObject::Delete(collection, factory, go);
}

TEST_F(GameObjectTest, TestFailingScript02)
{
    // Test init failure
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "testscriptproto02.go", 0x0);
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(GameObjectTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(collection, factory, "testscriptproto03.go", 0x0);
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_FALSE(dmGameObject::Update(collection, go, &update_context));
    dmGameObject::Delete(collection, factory, go);
}

static void CreateFile(const char* file_name, const char* contents)
{
    FILE* f;
    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "%s", contents);
    fclose(f);
}

TEST(ScriptTest, TestReloadScript)
{
    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = "/tmp";
#endif

    dmResource::HFactory factory = dmResource::NewFactory(16, tmp_dir, RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT);
    dmGameObject::HCollection collection = dmGameObject::NewCollection();
    dmGameObject::RegisterResourceTypes(factory);

    uint32_t type;
    dmResource::FactoryResult e = dmResource::GetTypeFromExtension(factory, "scriptc", &type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

    char script_file_name[512];
    DM_SNPRINTF(script_file_name, sizeof(script_file_name), "%s/%s", tmp_dir, "__test__.scriptc");

    char go_file_name[512];
    DM_SNPRINTF(go_file_name, sizeof(go_file_name), "%s/%s", tmp_dir, "__go__.go");

    dmGameObject::PrototypeDesc prototype;
    memset(&prototype, 0, sizeof(prototype));
    prototype.m_Name = "foo";
    prototype.m_Script = "__test__.scriptc";

    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObject::PrototypeDesc::m_DDFDescriptor, go_file_name);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    CreateFile(script_file_name,
               "function Init(self)\n"
               "end\n"
               "function Update(self)\n"
                   "self.Position = {1,2,3}\n"
               "end\n"
               "functions = { Init = Init, Update = Update }\n");

    dmGameObject::HInstance go;
    go = dmGameObject::New(collection, factory, "__go__.go", 0x0);
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    dmGameObject::Update(collection, 0);
    Point3 p1 = dmGameObject::GetPosition(go);
    ASSERT_EQ(1, p1.getX());
    ASSERT_EQ(2, p1.getY());
    ASSERT_EQ(3, p1.getZ());

    dmSleep(1000000); // TODO: Currently seconds time resolution in modification time

    CreateFile(script_file_name,
               "function Init(self)\n"
               "end\n"
               "function Update(self)\n"
                   "self.Position = {10,20,30}\n"
               "end\n"
               "functions = { Init = Init, Update = Update }\n");


    dmResource::FactoryResult fr = dmResource::ReloadType(factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, fr);

    dmGameObject::Update(collection, 0);
    Point3 p2 = dmGameObject::GetPosition(go);
    ASSERT_EQ(10, p2.getX());
    ASSERT_EQ(20, p2.getY());
    ASSERT_EQ(30, p2.getZ());

    unlink(script_file_name);
    fr = dmResource::ReloadType(factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_RESOURCE_NOT_FOUND, fr);

    unlink(go_file_name);

    dmGameObject::Delete(collection, factory, go);
    dmGameObject::DeleteCollection(collection, factory);
    dmResource::DeleteFactory(factory);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    uint32_t event_id = dmHashBuffer32("spawn_result", sizeof("spawn_result"));
    dmEvent::Register(event_id, 256);

    dmGameObject::Initialize();
    dmGameObject::RegisterDDFType(TestResource::Spawn::m_DDFDescriptor);
    int ret = RUN_ALL_TESTS();
    dmGameObject::Finalize();
    return ret;
}
