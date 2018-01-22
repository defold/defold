#include <gtest/gtest.h>

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

using namespace Vectormath::Aos;

class BonesTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/bones");
        m_ScriptContext = dmScript::NewContext(0, m_Factory, true);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
        dmGameObject::Result result;

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "a", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType a_type;
        a_type.m_Name = "a";
        a_type.m_ResourceType = resource_type;
        a_type.m_Context = this;
        a_type.m_CreateFunction = AComponentCreate;
        a_type.m_DestroyFunction = AComponentDestroy;
        a_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 2);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmGameObject::DeleteRegister(m_Register);
        dmResource::DeleteFactory(m_Factory);
    }

    static dmResource::FResourceCreate    ACreate;
    static dmResource::FResourceDestroy   ADestroy;
    static dmGameObject::ComponentCreate  AComponentCreate;
    static dmGameObject::ComponentDestroy AComponentDestroy;

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmGameObject::ModuleContext m_ModuleContext;
    dmResource::HFactory m_Factory;
};

static dmResource::Result NullResourceCreate(const dmResource::ResourceCreateParams& params)
{
    params.m_Resource->m_Resource = (void*)1; // asserted for != 0 in dmResource
    return dmResource::RESULT_OK;
}

static dmResource::Result NullResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    return dmResource::RESULT_OK;
}

struct ComponentData {
    dmGameObject::HInstance m_Child;
};

static dmGameObject::CreateResult TestComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    BonesTest* test = (BonesTest*)params.m_Context;
    ComponentData* data = new ComponentData();

    data->m_Child = dmGameObject::New(test->m_Collection, 0x0);
    if (data->m_Child == 0x0 || data->m_Child->m_Index < params.m_Instance->m_Index) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    dmGameObject::SetBone(data->m_Child, true);
    dmGameObject::SetParent(data->m_Child, params.m_Instance);

    *params.m_UserData = (uintptr_t)data;
    return dmGameObject::CREATE_RESULT_OK;
}

static dmGameObject::CreateResult TestComponentDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ComponentData* data = (ComponentData*)*params.m_UserData;
    dmGameObject::DeleteBones(params.m_Instance);
    delete data;
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate BonesTest::ACreate              = NullResourceCreate;
dmResource::FResourceDestroy BonesTest::ADestroy            = NullResourceDestroy;
dmGameObject::ComponentCreate BonesTest::AComponentCreate   = TestComponentCreate;
dmGameObject::ComponentDestroy BonesTest::AComponentDestroy = TestComponentDestroy;

/**
 * Purpose is to test that delete bones indeed deletes a bone.
 */
TEST_F(BonesTest, DeleteBones)
{
    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0);
    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());

    // Create the game object, the component above will create a child bone to that game object, which in turn will get a lower index because of the gap above
    dmGameObject::HInstance test_inst = dmGameObject::New(m_Collection, "/test_bones.goc");
    ASSERT_NE((void*)0, test_inst);

    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());

    dmGameObject::Delete(m_Collection, test_inst, false);
    dmGameObject::PostUpdate(m_Collection);

    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());

    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
}

/**
 * This test is modeled after the way the spine model component interacts with the game object system.
 */
TEST_F(BonesTest, ComponentCreatingInstances)
{
    m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0);

    // First create three game objects to create gaps in the instance array
    dmGameObject::HInstance tmp_inst[3];
    for (int i = 0; i < 3; ++i) {
        tmp_inst[i] = dmGameObject::New(m_Collection, 0x0);
        ASSERT_NE((void*)0, tmp_inst[i]);
    }
    // Delete the first two in reverse order; the next created will have index 1, the second created will have index 0
    dmGameObject::Delete(m_Collection, tmp_inst[1], false);
    dmGameObject::Delete(m_Collection, tmp_inst[0], false);

    // Create the game object, the component above will create a child bone to that game object, which in turn will get a lower index because of the gap above
    dmGameObject::HInstance test_inst = dmGameObject::New(m_Collection, "/test_bones.goc");
    ASSERT_NE((void*)0, test_inst);

    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
