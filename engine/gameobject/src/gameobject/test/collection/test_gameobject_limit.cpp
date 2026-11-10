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

#include <stdint.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/path.h>
#include <dlib/testutil.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

#include <dmsdk/resource/resource.h>

using namespace dmVMath;

class CollectionLimitTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        char path[DMPATH_MAX_PATH];
        m_Factory = dmResource::NewFactory(&params, dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/gameobject/test/collection"));

        dmScript::ContextParams script_context_params = {};
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);

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
    }

    void TearDown() override
    {
        if (m_Collection)
        {
            dmGameObject::DeleteCollection(m_Collection);
            dmGameObject::PostUpdate(m_Register);
        }
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        if (m_Register)
            dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection = 0;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

TEST_F(CollectionLimitTest, CreateAndHitLimitAndSetGetPosition)
{
    // Exercise fixed-capacity storage past the old 16-bit instance-index boundary.
    const uint32_t max_instances = 65537;

    m_Collection = dmGameObject::NewCollection("limit_col", m_Factory, m_Register, max_instances, 0x0);
    ASSERT_NE(0, m_Collection);

    // Create max_instances objects and set position.x to 1..max_instances
    dmArray<dmGameObject::HInstance> instances;
    instances.SetCapacity(max_instances);
    for (uint32_t i = 0; i < max_instances; ++i)
    {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
        ASSERT_NE(0, go);
        instances.Push(go);
        dmGameObject::SetPosition(go, Point3((float)(i + 1), 0.0f, 0.0f));
    }

    // Next creation should fail (buffer full)
    dmGameObject::HInstance overflow = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_EQ(0, overflow);

    // Verify we can read back what we wrote
    for (uint32_t i = 0; i < instances.Size(); ++i)
    {
        float expected = (float)(i + 1);
        ASSERT_NEAR(expected, dmGameObject::GetPosition(instances[i]).getX(), 0.0f);
    }
}

// Prototype slots must grow without changing existing indices, reuse released slots,
// and reject overflow without wrapping a 16-bit index into the empty prototype.
TEST_F(CollectionLimitTest, PrototypePoolGrowthReuseAndLimit)
{
    const uint32_t prototype_count = dmGameObject::INVALID_PROTOTYPE_INDEX - 1;
    dmGameObject::Prototype* prototypes = new dmGameObject::Prototype[prototype_count];
    for (uint32_t i = 0; i < prototype_count; ++i)
    {
        ASSERT_TRUE(dmGameObject::RegisterPrototype(m_Register, &prototypes[i]));
        ASSERT_EQ(i + 1, (uint32_t)prototypes[i].m_Index);
    }
    for (uint32_t i = 0; i < prototype_count; ++i)
        ASSERT_EQ(&prototypes[i], m_Register->m_Prototypes[prototypes[i].m_Index]);

    dmGameObject::Prototype overflow;
    ASSERT_FALSE(dmGameObject::RegisterPrototype(m_Register, &overflow));
    ASSERT_EQ(dmGameObject::INVALID_PROTOTYPE_INDEX, overflow.m_Index);
    const uint32_t released = 1024;
    uint16_t index = prototypes[released].m_Index;
    dmGameObject::UnregisterPrototype(m_Register, &prototypes[released]);
    ASSERT_TRUE(dmGameObject::RegisterPrototype(m_Register, &overflow));
    ASSERT_EQ(index, overflow.m_Index);
    ASSERT_EQ(&overflow, m_Register->m_Prototypes[index]);
    dmGameObject::UnregisterPrototype(m_Register, &overflow);
    for (uint32_t i = 0; i < prototype_count; ++i)
    {
        if (i != released)
            dmGameObject::UnregisterPrototype(m_Register, &prototypes[i]);
    }
    ASSERT_EQ(1u, (uint32_t)m_Register->m_PrototypeIndices.Size());
    delete[] prototypes;
}

// Instances share a resource slot, which stays alive until its final resource reference is released.
TEST_F(CollectionLimitTest, SharedPrototypeResourceLifetime)
{
    m_Collection = dmGameObject::NewCollection("shared_prototype", m_Factory, m_Register, 8, 0);
    ASSERT_NE((dmGameObject::HCollection)0, m_Collection);
    dmGameObject::Prototype* prototype;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/go1.goc", (void**)&prototype));
    uint16_t index = prototype->m_Index;
    dmGameObject::HInstance first = dmGameObject::New(m_Collection, "/go1.goc");
    dmGameObject::HInstance second = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE((dmGameObject::HInstance)0, first);
    ASSERT_NE((dmGameObject::HInstance)0, second);
    ASSERT_EQ(index, dmGameObject::GetInstanceFromHandle(first)->m_PrototypeIndex);
    ASSERT_EQ(index, dmGameObject::GetInstanceFromHandle(second)->m_PrototypeIndex);
    dmGameObject::Delete(m_Collection, first, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(prototype, m_Register->m_Prototypes[index]);
    dmGameObject::Delete(m_Collection, second, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(prototype, m_Register->m_Prototypes[index]);
    dmResource::Release(m_Factory, prototype);
    ASSERT_EQ((dmGameObject::Prototype*)0, m_Register->m_Prototypes[index]);
    ASSERT_EQ(1u, (uint32_t)m_Register->m_PrototypeIndices.Size());
}

// Engine shutdown deletes the register before the factory; later resource destruction must not use the freed pool.
TEST_F(CollectionLimitTest, PrototypeResourceOutlivesRegister)
{
    dmGameObject::Prototype* prototype;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/go1.goc", (void**)&prototype));
    ASSERT_NE(dmGameObject::INVALID_PROTOTYPE_INDEX, prototype->m_Index);
    dmGameObject::DeleteRegister(m_Register);
    m_Register = 0;
    ASSERT_EQ(dmGameObject::INVALID_PROTOTYPE_INDEX, prototype->m_Index);
    dmResource::Release(m_Factory, prototype);
}

TEST_F(CollectionLimitTest, RejectsCollectionAboveHandleCapacity)
{
    m_Collection = dmGameObject::NewCollection("too_large", m_Factory, m_Register, (1U << 20) + 1, 0x0);
    ASSERT_EQ(0, m_Collection);
}

TEST_F(CollectionLimitTest, CollectionRegistryExhaustion)
{
    const uint32_t collection_count = 1U << 12;
    dmArray<dmGameObject::HCollection> collections;
    collections.SetCapacity(collection_count);

    for (uint32_t i = 0; i < collection_count; ++i)
    {
        char name[32];
        dmSnPrintf(name, sizeof(name), "registry_%u", i);
        dmGameObject::HCollection collection = dmGameObject::NewCollection(name, m_Factory, m_Register, 1, 0x0);
        ASSERT_NE(0, collection);
        ASSERT_LT((uint32_t)(collection & 0xffff), collection_count);
        collections.Push(collection);
        dmGameObject::Collection* internal_collection = dmGameObject::GetCollectionFromHandle(collection);
        ASSERT_NE((dmGameObject::Collection*)0, internal_collection);
        dmGameObject::DetachCollection(internal_collection, false);
    }

    ASSERT_EQ(0,
              dmGameObject::NewCollection("registry_overflow", m_Factory, m_Register, 1, 0x0));

    for (uint32_t i = 0; i < collections.Size(); ++i)
    {
        dmGameObject::HCollection collection = collections[i];
        dmGameObject::Collection* internal_collection = dmGameObject::GetCollectionFromHandle(collection);
        ASSERT_NE((dmGameObject::Collection*)0, internal_collection);
        dmGameObject::DeleteCollection(internal_collection);
        ASSERT_EQ((dmGameObject::Collection*)0, dmGameObject::GetCollectionFromHandle(collection));
    }
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Register));
}
