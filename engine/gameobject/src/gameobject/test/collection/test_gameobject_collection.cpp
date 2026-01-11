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
#include <dlib/log.h>
#include <dlib/time.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

#include <dmsdk/resource/resource.h>
#include "gameobject/gameobject_ddf.h"

using namespace dmVMath;

class CollectionTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/collection");

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

        m_Collection = dmGameObject::NewCollection("testcollection", m_Factory, m_Register, dmGameObject::GetCollectionDefaultCapacity(m_Register), 0x0);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceType resource_type;
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
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 2);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    void TearDown() override
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

    // dmResource::Get API but with preloader instead
    dmResource::Result PreloaderGet(dmResource::HFactory factory, const char *ref, void** resource)
    {
        dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, ref);
        dmResource::Result r;
        for (uint32_t i=0;i<33;i++)
        {
            r = dmResource::UpdatePreloader(pr, 0, 0, 30*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(30000);
        }
        if (r == dmResource::RESULT_OK)
        {
            r = dmResource::Get(factory, ref, resource);
        }
        else
        {
            *resource = 0x0;
        }
        dmResource::DeletePreloader(pr);
        return r;
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
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

static dmResource::Result NullResourceCreate(const dmResource::ResourceCreateParams* params)
{
    ResourceDescriptorSetResource(params->m_Resource, (void*)1); // asserted for != 0 in dmResource
    return dmResource::RESULT_OK;
}

static dmResource::Result NullResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    return dmResource::RESULT_OK;
}

static dmGameObject::CreateResult TestComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    // Hard coded for the specific case "CreateCallback" below
    dmGameObject::HInstance instance = params.m_Instance;
    if (dmGameObject::GetIdentifier(instance) != dmHashString64("/go2")) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    if (dmGameObject::GetWorldPosition(instance).getX() != 2.0f) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    if (dmGameObject::GetIdentifier(dmGameObject::GetParent(instance)) != dmHashString64("/go1")) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    return dmGameObject::CREATE_RESULT_OK;
}

static dmGameObject::CreateResult TestComponentDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate CollectionTest::ACreate              = NullResourceCreate;
dmResource::FResourceDestroy CollectionTest::ADestroy            = NullResourceDestroy;
dmGameObject::ComponentCreate CollectionTest::AComponentCreate   = TestComponentCreate;
dmGameObject::ComponentDestroy CollectionTest::AComponentDestroy = TestComponentDestroy;

static dmGameObject::Result Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* path, const char* id_prefix, dmGameObject::InstancePropertyContainers *property_buffers,
        const Point3& position, const Quat& rotation, const Vector3& scale,
        dmGameObject::InstanceIdMap *instances)
{
    void *msg;
    uint32_t msg_size;
    dmResource::Result r = dmResource::GetRaw(factory, path, &msg, &msg_size);
    if (r != dmResource::RESULT_OK) {
        dmLogError("failed to load collection [%s]", path);
        return dmGameObject::RESULT_RESOURCE_ERROR;
    }

    dmGameObjectDDF::CollectionDesc* desc;
    dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(msg, msg_size, &desc);
    if (e != dmDDF::RESULT_OK)
    {
        dmLogError("Failed to parse collection [%s]", path);
        return dmGameObject::RESULT_RESOURCE_ERROR;
    }
    dmGameObject::Result result = dmGameObject::SpawnFromCollection(collection, desc, id_prefix, property_buffers, position, rotation, scale, instances);
    dmDDF::FreeMessage(desc);
    free(msg);
    return result;
}

TEST_F(CollectionTest, Collection)
{
    for (int i = 0; i < 20; ++i)
    {
        // NOTE: Coll is local and not m_Collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::Result r;
        if (i < 10)
            r = dmResource::Get(m_Factory, "/test.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/test.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        dmhash_t go01ident = dmHashString64("/go1");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);

        dmhash_t go02ident = dmHashString64("/go2");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);

        dmGameObject::Init(coll);
        dmGameObject::Update(coll, &m_UpdateContext);

        ASSERT_NE(go01, go02);

        dmResource::Release(m_Factory, (void*) coll);

        dmGameObject::PostUpdate(m_Register);
    }
}

TEST_F(CollectionTest, CollectionSpawning)
{
    // NOTE: Coll is local and not m_Collection in CollectionTest
    dmGameObject::HCollection coll;

    dmResource::Result r = dmResource::Get(m_Factory, "/empty.collectionc", (void**) &coll);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*) 0, coll);

    dmGameObject::Init(coll);

    dmVMath::Point3 pos(0,0,0);
    dmVMath::Quat rot(0,0,0,1);
    dmVMath::Vector3 scale(1,1,1);

    bool ret;
    ret = dmGameObject::Update(coll, &m_UpdateContext);
    ASSERT_TRUE(ret);

    // spawn 5 collections with a autogenerated id
    for (int i=0;i!=5;i++)
    {
        dmGameObject::InstanceIdMap output;
        dmGameObject::InstancePropertyContainers props;

        dmGameObject::Result result = Spawn(m_Factory, coll, "/root1.collectionc", nullptr, &props, pos, rot, scale, &output);
        ASSERT_TRUE(result == dmGameObject::RESULT_OK);
        ASSERT_NE(output.Size(), 0u);

        ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);

        ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);
    }

    // spawn 5 collections with a custom id
    const char* prefix = "/custom";
    char buffer[32];
    for (int i=0;i!=5;i++)
    {
        dmGameObject::InstanceIdMap output;
        dmGameObject::InstancePropertyContainers props;
        dmSnPrintf(buffer, sizeof(buffer), "%s%d", prefix, i);
        dmGameObject::Result result = Spawn(m_Factory, coll, "/root1.collectionc", buffer, &props, pos, rot, scale, &output);
        ASSERT_TRUE(result == dmGameObject::RESULT_OK);
        ASSERT_NE(output.Size(), 0u);

        dmSnPrintf(buffer, sizeof(buffer), "%s%d/go1", prefix, i);
        ASSERT_EQ((uint64_t)*output.Get(dmHashString64("/go1")), (uint64_t)dmHashString64(buffer));
        dmSnPrintf(buffer, sizeof(buffer), "%s%d/go2", prefix, i);
        ASSERT_EQ((uint64_t)*output.Get(dmHashString64("/go2")), (uint64_t)dmHashString64(buffer));
        dmSnPrintf(buffer, sizeof(buffer), "%s%d/sub1/child", prefix, i);
        ASSERT_EQ((uint64_t)*output.Get(dmHashString64("/sub1/child")), (uint64_t)dmHashString64(buffer));

        ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);

        ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);
        output.Clear();
    }

    dmResource::Release(m_Factory, (void*) coll);
    dmGameObject::PostUpdate(m_Register);
}

TEST_F(CollectionTest, CollectionSpawningToFail)
{
    const uint32_t max = 100;

    dmGameObject::HCollection coll;
    coll = dmGameObject::NewCollection("TestCollection", m_Factory, m_Register, max, 0x0);
    dmGameObject::Init(coll);

    dmVMath::Point3 pos(0,0,0);
    dmVMath::Quat rot(0,0,0,1);
    dmVMath::Vector3 scale(1,1,1);

    // Spawn until failure
    bool filled = false;
    for (int i=0;i<50;i++)
    {
        dmGameObject::InstanceIdMap output;
        dmGameObject::InstancePropertyContainers props;
        dmGameObject::Result result = Spawn(m_Factory, coll, "/root1.collectionc", nullptr, &props, pos, rot, scale, &output);
        if (result != dmGameObject::RESULT_OK)
        {
            ASSERT_NE(i, 0);
            ASSERT_EQ(output.Size(), 0u);
            filled = true;
            break;
        }
        ASSERT_TRUE(result == dmGameObject::RESULT_OK);
        ASSERT_NE(output.Size(), 0u);
        bool ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);
    }

    dmGameObject::DeleteCollection(coll);

    ASSERT_TRUE(filled);
    dmGameObject::PostUpdate(m_Register);

    ASSERT_TRUE(true);
}

TEST_F(CollectionTest, CollectionSpawningResults)
{
    // NOTE: Coll is local and not m_Collection in CollectionTest
    dmGameObject::HCollection coll;

    dmResource::Result r = dmResource::Get(m_Factory, "/empty.collectionc", (void**) &coll);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*) 0, coll);

    dmGameObject::Init(coll);

    dmVMath::Point3 pos(0,0,0);
    dmVMath::Quat rot(0,0,0,1);
    dmVMath::Vector3 scale(1,1,1);

    bool ret;
    ret = dmGameObject::Update(coll, &m_UpdateContext);
    ASSERT_TRUE(ret);

    dmGameObject::InstanceIdMap output;
    dmGameObject::InstancePropertyContainers props;

    // RESULT_IDENTIFIER_INVALID: custom prefix without leading slash
    dmGameObject::Result result = Spawn(m_Factory, coll, "/root1.collectionc", "custom", &props, pos, rot, scale, &output);
    ASSERT_TRUE(result == dmGameObject::RESULT_IDENTIFIER_INVALID);
    ASSERT_EQ(output.Size(), 0u);

    // RESULT_IDENTIFIER_IN_USE: duplicate id (prefix)
    result = Spawn(m_Factory, coll, "/root1.collectionc", "/custom", &props, pos, rot, scale, &output);
    ASSERT_TRUE(result == dmGameObject::RESULT_OK);
    ASSERT_NE(output.Size(), 0u);
    output.Clear();
    result = Spawn(m_Factory, coll, "/root1.collectionc", "/custom", &props, pos, rot, scale, &output);
    ASSERT_TRUE(result == dmGameObject::RESULT_IDENTIFIER_IN_USE);
    ASSERT_EQ(output.Size(), 0u);
    
    dmResource::Release(m_Factory, (void*) coll);
    dmGameObject::PostUpdate(m_Register);
}

TEST_F(CollectionTest, PostCollection)
{
    for (int i = 0; i < 10; ++i)
    {
        dmResource::Result r;
        dmGameObject::HCollection coll1;
        r = dmResource::Get(m_Factory, "/post1.collectionc", (void**) &coll1);
        ASSERT_EQ(dmResource::RESULT_OK, r);
        ASSERT_NE((void*) 0, coll1);

        dmGameObject::HCollection coll2;
        r = dmResource::Get(m_Factory, "/post2.collectionc", (void**) &coll2);
        ASSERT_EQ(dmResource::RESULT_OK, r);
        ASSERT_NE((void*) 0, coll2);

        bool ret;
        dmGameObject::Init(coll1);
        dmGameObject::Init(coll2);

        ret = dmGameObject::Update(coll1, &m_UpdateContext);
        ASSERT_TRUE(ret);
        ret = dmGameObject::Update(coll2, &m_UpdateContext);
        ASSERT_TRUE(ret);

        dmResource::Release(m_Factory, (void*) coll1);
        dmResource::Release(m_Factory, (void*) coll2);

        dmGameObject::PostUpdate(m_Register);
    }
}

TEST_F(CollectionTest, CollectionFail)
{
    dmLogSetLevel(LOG_SEVERITY_FATAL);
    for (int i = 0; i < 20; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::Result r;

        // Test both with normal loading and preloading
        if (i < 10)
            r = dmResource::Get(m_Factory, "/failing_sub.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/failing_sub.collectionc", (void**) &coll);

        ASSERT_NE(dmResource::RESULT_OK, r);
        dmGameObject::PostUpdate(m_Register);
    }
    dmLogSetLevel(LOG_SEVERITY_WARNING);
}

TEST_F(CollectionTest, CollectionComponentFail)
{
    dmLogSetLevel(LOG_SEVERITY_FATAL);
    for (int i = 0; i < 4; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::Result r;
        // Test both with normal loading and preloading
        if (i < 2)
            r = dmResource::Get(m_Factory, "/failing_component.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/failing_component.collectionc", (void**) &coll);

        ASSERT_NE(dmResource::RESULT_OK, r);
        dmGameObject::PostUpdate(m_Register);
    }
    dmLogSetLevel(LOG_SEVERITY_WARNING);
}

TEST_F(CollectionTest, CollectionInCollection)
{
    for (int i = 0; i < 20; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::Result r;

        if (i < 10)
            r = dmResource::Get(m_Factory, "/root1.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/root1.collectionc", (void**) &coll);

        ASSERT_EQ(dmResource::RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        dmhash_t go01ident = dmHashString64("/go1");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);
        ASSERT_NEAR(dmGameObject::GetPosition(go01).getX(), 123.0f, 0.0000f);

        dmhash_t go02ident = dmHashString64("/go2");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);
        ASSERT_NEAR(dmGameObject::GetPosition(go02).getX(), 456.0f, 0.0000f);

        ASSERT_NE(go01, go02);

        ASSERT_TRUE(dmGameObject::Update(coll, &m_UpdateContext));

        dmhash_t parent_sub1_ident = dmHashString64("/sub1/parent");
        dmGameObject::HInstance parent_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, parent_sub1_ident);
        ASSERT_NE((void*) 0, parent_sub1);

        dmhash_t child_sub1_ident = dmHashString64("/sub1/child");
        dmGameObject::HInstance child_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, child_sub1_ident);
        ASSERT_NE((void*) 0, child_sub1);

        dmhash_t parent_sub2_ident = dmHashString64("/sub2/parent");
        dmGameObject::HInstance parent_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, parent_sub2_ident);
        ASSERT_NE((void*) 0, parent_sub2);

        dmhash_t child_sub2_ident = dmHashString64("/sub2/child");
        dmGameObject::HInstance child_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, child_sub2_ident);
        ASSERT_NE((void*) 0, child_sub2);

        // Relative identifiers
        ASSERT_EQ(dmHashString64("/a"), dmGameObject::GetAbsoluteIdentifier(go01, "a"));
        ASSERT_EQ(dmHashString64("/a"), dmGameObject::GetAbsoluteIdentifier(go02, "a"));
        ASSERT_EQ(dmHashString64("/sub1/a"), dmGameObject::GetAbsoluteIdentifier(parent_sub1, "a"));
        ASSERT_EQ(dmHashString64("/sub2/a"), dmGameObject::GetAbsoluteIdentifier(parent_sub2, "a"));
        ASSERT_EQ(dmHashString64("/sub1/a"), dmGameObject::GetAbsoluteIdentifier(parent_sub1, "/sub1/a"));
        ASSERT_EQ(dmHashString64("/sub2/a"), dmGameObject::GetAbsoluteIdentifier(parent_sub2, "/sub2/a"));

        bool ret = dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_TRUE(ret);

        dmResource::Release(m_Factory, (void*) coll);

        dmGameObject::PostUpdate(m_Register);
    }
}

TEST_F(CollectionTest, CollectionInCollectionChildFail)
{
    dmLogSetLevel(LOG_SEVERITY_FATAL);
    for (int i = 0; i < 20; ++i)
    {
        // NOTE: Coll is local and not collection in CollectionTest
        dmGameObject::HCollection coll;
        dmResource::Result r;
        if (i < 10)
            r = dmResource::Get(m_Factory, "root2.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "root2.collection", (void**) &coll);
        ASSERT_NE(dmResource::RESULT_OK, r);
    }
    dmLogSetLevel(LOG_SEVERITY_WARNING);
}

TEST_F(CollectionTest, DefaultValues)
{
    dmGameObject::HCollection hcollection;
    dmResource::Result r = dmResource::Get(m_Factory, "/defaults.collectionc", (void**) &hcollection);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    uint32_t instance_count = hcollection->m_Collection->m_LevelIndices[0].Size();
    ASSERT_EQ(2U, instance_count);
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        dmGameObject::HInstance instance = hcollection->m_Collection->m_Instances[hcollection->m_Collection->m_LevelIndices[0][i]];
        ASSERT_NE((void*)0, instance);
        dmVMath::Point3 p = dmGameObject::GetPosition(instance);
        ASSERT_EQ(0.0f, p.getX());
        ASSERT_EQ(0.0f, p.getY());
        ASSERT_EQ(0.0f, p.getZ());
        dmVMath::Quat r = dmGameObject::GetRotation(instance);
        ASSERT_EQ(0.0f, r.getX());
        ASSERT_EQ(0.0f, r.getY());
        ASSERT_EQ(0.0f, r.getZ());
        ASSERT_EQ(1.0f, r.getW());
    }
    dmResource::Release(m_Factory, (void*) hcollection);

    dmGameObject::PostUpdate(m_Register);
}

TEST_F(CollectionTest, CollectionCapacity)
{
    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HCollection coll;
        dmResource::Result r;
        if (i < 1)
            r = dmResource::Get(m_Factory, "/test.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/test.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        dmhash_t go01ident = dmHashString64("/go1");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);
        dmhash_t go02ident = dmHashString64("/go2");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);

        dmGameObject::Init(coll);
        dmGameObject::Update(coll, &m_UpdateContext);
        ASSERT_NE(go01, go02);

        dmResource::Release(m_Factory, (void*) coll);
        dmGameObject::PostUpdate(m_Register);
    }

    dmGameObject::SetCollectionDefaultCapacity(m_Register, 1);
    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HCollection coll;
        dmResource::Result r;
        if (i < 1)
            r = dmResource::Get(m_Factory, "/test.collectionc", (void**) &coll);
        else
            r = PreloaderGet(m_Factory, "/test.collectionc", (void**) &coll);
        ASSERT_NE(dmResource::RESULT_OK, r);
        ASSERT_EQ((void*) 0, coll);

        dmGameObject::PostUpdate(m_Register);
    }
}


TEST_F(CollectionTest, CreateCallback)
{
    dmGameObject::HCollection coll;
    dmResource::Result r = dmResource::Get(m_Factory, "/test_create.collectionc", (void**) &coll);
    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_NE((void*) 0, coll);

    dmResource::Release(m_Factory, (void*) coll);

    dmGameObject::PostUpdate(m_Register);
}
