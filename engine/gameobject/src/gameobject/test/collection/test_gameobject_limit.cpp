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

#include <dlib/hash.h>

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
    // Max usable index is INVALID_INSTANCE_INDEX - 1
    const uint32_t max_instances = dmGameObject::INVALID_INSTANCE_INDEX - 1;

    m_Collection = dmGameObject::NewCollection("limit_col", m_Factory, m_Register, max_instances, 0x0);
    ASSERT_NE((void*)0, (void*)m_Collection);

    // Create max_instances objects and set position.x to 1..max_instances
    dmArray<dmGameObject::HInstance> instances;
    instances.SetCapacity(max_instances);
    for (uint32_t i = 0; i < max_instances; ++i)
    {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
        ASSERT_NE((void*)0, (void*)go);
        instances.Push(go);
        dmGameObject::SetPosition(go, Point3((float)(i + 1), 0.0f, 0.0f));
    }

    // Next creation should fail (buffer full)
    dmGameObject::HInstance overflow = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_EQ((void*)0, (void*)overflow);

    // Verify we can read back what we wrote
    for (uint32_t i = 0; i < instances.Size(); ++i)
    {
        float expected = (float)(i + 1);
        ASSERT_NEAR(expected, dmGameObject::GetPosition(instances[i]).getX(), 0.0f);
    }
}
