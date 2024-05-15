// Copyright 2020-2024 The Defold Foundation
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
#include "../proto/gameobject/gameobject_ddf.h"

#define EPSILON 0.000001f

using namespace dmVMath;

class HierarchyTest : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmHashEnableReverseHash(true);

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/hierarchy");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
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

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

TEST_F(HierarchyTest, TestHierarchy1)
{
    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance child = dmGameObject::New(m_Collection, "/go.goc");

        const float parent_rot = 3.14159265f / 4.0f;

        Point3 parent_pos(1, 2, 0);
        Point3 child_pos(4, 5, 0);

        Matrix4 parent_m = Matrix4::rotationZ(parent_rot);
        parent_m.setCol3(Vector4(parent_pos));

        Matrix4 child_m = Matrix4::identity();
        child_m.setCol3(Vector4(child_pos));

        dmGameObject::SetPosition(parent, parent_pos);
        dmGameObject::SetRotation(parent, Quat::rotationZ(parent_rot));
        dmGameObject::SetPosition(child, child_pos);

        ASSERT_EQ(0U, dmGameObject::GetDepth(child));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        dmGameObject::SetParent(child, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

        ASSERT_EQ(1U, dmGameObject::GetDepth(child));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        bool ret;
        ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(m_Collection);
        ASSERT_TRUE(ret);

        Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());

        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);

        if (i % 2 == 0)
        {
            dmGameObject::Delete(m_Collection, parent, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
            ASSERT_EQ(0U, dmGameObject::GetDepth(child));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(child));
            dmGameObject::Delete(m_Collection, child, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
        else
        {
            dmGameObject::Delete(m_Collection, child, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(parent));
            dmGameObject::Delete(m_Collection, parent, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
    }
}

TEST_F(HierarchyTest, TestHierarchy2)
{
    // Test transform
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child_child = dmGameObject::New(m_Collection, "/go.goc");

    const float parent_rot = 3.14159265f / 4.0f;
    const float child_rot = 3.14159265f / 5.0f;

    Point3 parent_pos(1, 1, 0);
    Point3 child_pos(0, 1, 0);
    Point3 child_child_pos(7, 2, 0);

    Matrix4 parent_m = Matrix4::rotationZ(parent_rot);
    parent_m.setCol3(Vector4(parent_pos));

    Matrix4 child_m = Matrix4::rotationZ(child_rot);
    child_m.setCol3(Vector4(child_pos));

    Matrix4 child_child_m = Matrix4::identity();
    child_child_m.setCol3(Vector4(child_child_pos));

    dmGameObject::SetPosition(parent, parent_pos);
    dmGameObject::SetRotation(parent, Quat::rotationZ(parent_rot));
    dmGameObject::SetPosition(child, child_pos);
    dmGameObject::SetRotation(child, Quat::rotationZ(child_rot));
    dmGameObject::SetPosition(child_child, child_child_pos);

    dmGameObject::SetParent(child, parent);
    dmGameObject::SetParent(child_child, child);

    ASSERT_EQ(1U, dmGameObject::GetChildCount(child));
    ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

    ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
    ASSERT_EQ(1U, dmGameObject::GetDepth(child));
    ASSERT_EQ(2U, dmGameObject::GetDepth(child_child));

    bool ret;
    ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());
    Point3 expected_child_child_pos = Point3(((parent_m * child_m) * child_child_pos).getXYZ());
    Point3 expected_child_child_pos2 = Point3(((parent_m * child_m) * child_child_m).getCol3().getXYZ());

    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos2), 0.001f);

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child, false);
    dmGameObject::Delete(m_Collection, child_child, false);
}

TEST_F(HierarchyTest, TestHierarchy3)
{
    // Test with siblings
    for (int i = 0; i < 3; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");

        ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
        ASSERT_EQ(0U, dmGameObject::GetDepth(child2));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(parent));

        dmGameObject::SetParent(child1, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

        dmGameObject::SetParent(child2, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(2U, dmGameObject::GetChildCount(parent));

        ASSERT_EQ(1U, dmGameObject::GetDepth(child1));
        ASSERT_EQ(1U, dmGameObject::GetDepth(child2));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        bool ret;
        ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(m_Collection);
        ASSERT_TRUE(ret);

        // Test all possible delete orders in this configuration
        if (i == 0)
        {
            dmGameObject::Delete(m_Collection, parent, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child1, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
        else if (i == 1)
        {
            dmGameObject::Delete(m_Collection, child1, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, parent, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
        else if (i == 2)
        {
            dmGameObject::Delete(m_Collection, child2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(m_Collection, parent, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(m_Collection, child1, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
        else
        {
            assert(false);
        }
    }
}

TEST_F(HierarchyTest, TestHierarchy4)
{
    // Test RESULT_MAXIMUM_HIEARCHICAL_DEPTH

    static const uint32_t max_levels = dmGameObject::MAX_HIERARCHICAL_DEPTH+1;

    dmGameObject::HInstance instances[max_levels];
    for (uint32_t i = 0; i < max_levels; ++i)
    {
        instances[i] = dmGameObject::New(m_Collection, "/go.goc");
    }

    dmGameObject::Result r;
    for (uint32_t i = 1; i < max_levels-1; ++i)
    {
        r = dmGameObject::SetParent(instances[i], instances[i-1]);
        ASSERT_EQ(dmGameObject::RESULT_OK, r);
    }
    r = dmGameObject::SetParent(instances[max_levels-1], instances[max_levels-2]);
    ASSERT_EQ(dmGameObject::RESULT_MAXIMUM_HIEARCHICAL_DEPTH, r);

    ASSERT_EQ(0U, dmGameObject::GetChildCount(instances[max_levels-2]));

    for (uint32_t i = 0; i < max_levels; ++i)
    {
        dmGameObject::Delete(m_Collection, instances[i], false);
    }
}

TEST_F(HierarchyTest, TestHierarchy5)
{
    // Test parent subtree

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child3 = dmGameObject::New(m_Collection, "/go.goc");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child3, child2);

    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));
    ASSERT_EQ(child2, dmGameObject::GetParent(child3));

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child1, false);
    dmGameObject::Delete(m_Collection, child2, false);
    dmGameObject::Delete(m_Collection, child3, false);
}

TEST_F(HierarchyTest, TestHierarchy6)
{
    // Test invalid reparent.
    // Test that the child node is not present in the upward trace from parent

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");

    // parent -> child1
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child1, parent));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    // child1 -> parent
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameObject::SetParent(parent, child1));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child1, false);
}

TEST_F(HierarchyTest, TestHierarchy7)
{
    // Test remove interior node

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));

    dmGameObject::Delete(m_Collection, child1, false);
    bool ret = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(ret);
    ASSERT_EQ(parent, dmGameObject::GetParent(child2));
    ASSERT_TRUE(dmGameObject::IsChildOf(child2, parent));

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child2, false);
}

TEST_F(HierarchyTest, TestHierarchy8)
{
    /*
        A1
      B2  C2
     D3

     Rearrange tree to:

        A1
          C2
            B2
              D3
     */

    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HInstance a1 = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance b2 = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance c2 = dmGameObject::New(m_Collection, "/go.goc");
        dmGameObject::HInstance d3 = dmGameObject::New(m_Collection, "/go.goc");

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(d3, b2));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(b2, a1));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(c2, a1));

        ASSERT_EQ(a1, dmGameObject::GetParent(b2));
        ASSERT_EQ(a1, dmGameObject::GetParent(c2));
        ASSERT_EQ(b2, dmGameObject::GetParent(d3));

        bool ret;
        ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(m_Collection);
        ASSERT_TRUE(ret);

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(b2, c2));

        ASSERT_EQ(a1, dmGameObject::GetParent(c2));
        ASSERT_EQ(c2, dmGameObject::GetParent(b2));
        ASSERT_EQ(b2, dmGameObject::GetParent(d3));

        ASSERT_EQ(1U, dmGameObject::GetChildCount(a1));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(c2));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(b2));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(d3));

        ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
        ASSERT_EQ(1U, dmGameObject::GetDepth(c2));
        ASSERT_EQ(2U, dmGameObject::GetDepth(b2));
        ASSERT_EQ(3U, dmGameObject::GetDepth(d3));

        ASSERT_TRUE(dmGameObject::IsChildOf(c2, a1));
        ASSERT_TRUE(dmGameObject::IsChildOf(b2, c2));
        ASSERT_TRUE(dmGameObject::IsChildOf(d3, b2));

        if (i == 0)
        {
            ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
            dmGameObject::Delete(m_Collection, a1, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            dmGameObject::Delete(m_Collection, c2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(b2));
            dmGameObject::Delete(m_Collection, b2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, d3, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
        else
        {
            ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
            ASSERT_EQ(3U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, a1, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetDepth(b2));
            ASSERT_EQ(2U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, b2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
            ASSERT_EQ(c2, dmGameObject::GetParent(d3));
            ASSERT_TRUE(dmGameObject::IsChildOf(d3, c2));

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            ASSERT_EQ(1U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, c2, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, d3, false);
            ret = dmGameObject::PostUpdate(m_Collection);
            ASSERT_TRUE(ret);
        }
    }
}

TEST_F(HierarchyTest, TestHierarchy9)
{
    // Test unparent

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));

    ASSERT_EQ(1U, dmGameObject::GetDepth(child1));
    ASSERT_EQ(2U, dmGameObject::GetDepth(child2));

    dmGameObject::SetParent(child1, 0);

    ASSERT_EQ((void*)0, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));

    ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
    ASSERT_EQ(1U, dmGameObject::GetDepth(child2));

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child1, false);
    dmGameObject::Delete(m_Collection, child2, false);
}

TEST_F(HierarchyTest, TestMultiReparent)
{
    // Test unparent

    dmGameObject::HInstance parent1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance parent2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");

    dmGameObject::SetParent(child1, parent1);
    dmGameObject::SetParent(child2, parent1);

    ASSERT_EQ(parent1, dmGameObject::GetParent(child1));
    ASSERT_EQ(parent1, dmGameObject::GetParent(child2));

    dmGameObject::SetParent(child2, parent2);

    ASSERT_EQ(parent2, dmGameObject::GetParent(child2));
    ASSERT_EQ(parent1, dmGameObject::GetParent(child1));

    dmGameObject::Delete(m_Collection, parent1, false);
    dmGameObject::Delete(m_Collection, parent2, false);
    dmGameObject::Delete(m_Collection, child1, false);
    dmGameObject::Delete(m_Collection, child2, false);
}

TEST_F(HierarchyTest, TestHierarchyScale)
{
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, "/go.goc");

    const float scale = 2.0f;

    dmGameObject::SetScale(parent, scale);
    dmGameObject::SetPosition(child, Point3(1.0f, 0.0f, 0.0f));

    dmGameObject::SetParent(child, parent);

    dmTransform::Transform world = dmGameObject::GetWorldTransform(child);

    // Needs update to obtain new world transform
    ASSERT_NE(scale, world.GetTranslation().getX());
    ASSERT_NE(scale, world.GetUniformScale());

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    // New world transform updated
    world = dmGameObject::GetWorldTransform(child);
    ASSERT_EQ(scale, world.GetTranslation().getX());
    ASSERT_EQ(scale, world.GetUniformScale());

    // Unparent to verify the scale is reset
    dmGameObject::SetParent(child, 0);

    ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    // New world transform updated
    world = dmGameObject::GetWorldTransform(child);
    ASSERT_NE(scale, world.GetTranslation().getX());
    ASSERT_NE(scale, world.GetUniformScale());

    dmGameObject::Delete(m_Collection, child, false);
    dmGameObject::Delete(m_Collection, parent, false);
}

TEST_F(HierarchyTest, TestHierarchyNonUniformScale)
{
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, "/go.goc");

    const Vector3 scale(4,5,6);
    dmGameObject::SetScale(parent, scale);
    dmGameObject::SetPosition(child, Point3(7.0f, 8.0f, 9.0f));
    dmGameObject::SetParent(child, parent);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmTransform::Transform world = dmGameObject::GetWorldTransform(child);
    ASSERT_EQ(world.GetScale().getX(), scale.getX());
    ASSERT_EQ(world.GetScale().getY(), scale.getY());
    ASSERT_EQ(world.GetScale().getZ(), scale.getZ());

    dmGameObject::Delete(m_Collection, child, false);
    dmGameObject::Delete(m_Collection, parent, false);
}

TEST_F(HierarchyTest, TestHierarchyInheritScale)
{
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, "/go.goc");

    const float scale = 2.0f;

    dmGameObject::SetScale(parent, scale);
    dmGameObject::SetPosition(child, Point3(1.0f, 0.0f, 0.0f));

    dmGameObject::SetParent(child, parent);

    dmTransform::Transform world = dmGameObject::GetWorldTransform(child);

    // Needs update to obtain new world transform
    ASSERT_NE(scale, world.GetTranslation().getX());
    ASSERT_NE(scale, world.GetUniformScale());

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    // New world transform updated
    world = dmGameObject::GetWorldTransform(child);
    ASSERT_EQ(scale, world.GetTranslation().getX());
    ASSERT_EQ(scale, world.GetUniformScale());

    ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    world = dmGameObject::GetWorldTransform(child);
    ASSERT_EQ(scale, world.GetTranslation().getX());
    ASSERT_EQ(scale, world.GetUniformScale());

    dmGameObject::Delete(m_Collection, child, false);
    dmGameObject::Delete(m_Collection, parent, false);
}

// Test depth-first order
TEST_F(HierarchyTest, TestHierarchyBonesOrder)
{
    dmGameObject::HInstance root = dmGameObject::New(m_Collection, 0x0);

    const uint32_t instance_count = 7;
    dmGameObject::HInstance instances[instance_count];
    uint32_t parent_indices[instance_count] = {~0u, 0u, 1u, 1u, 0u, 4u, 4u};
    dmTransform::Transform transforms[instance_count];
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        instances[i] = dmGameObject::New(m_Collection, 0x0);
        dmGameObject::SetBone(instances[i], true);
        transforms[i].SetIdentity();
        transforms[i].SetTranslation(Vector3((float)i + 1, (float)i + 2, (float)i + 3));
    }
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        uint32_t index = instance_count - 1 - i;
        dmGameObject::HInstance parent = root;
        if (parent_indices[index] != ~0u)
            parent = instances[parent_indices[index]];
        dmGameObject::SetParent(instances[index], parent);
    }

    dmTransform::Transform component_transform;
    component_transform.SetIdentity();
    ASSERT_EQ(instance_count, SetBoneTransforms(instances[0], component_transform, transforms, instance_count));

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        ASSERT_NEAR((float)i + 1.f, dmGameObject::GetPosition(instances[i]).getX(), EPSILON);
        ASSERT_NEAR((float)i + 2.f, dmGameObject::GetPosition(instances[i]).getY(), EPSILON);
        ASSERT_NEAR((float)i + 3.f, dmGameObject::GetPosition(instances[i]).getZ(), EPSILON);
    }

    component_transform.SetTranslation(Vector3(100.0f, 100.0f, 100.0f));
    ASSERT_EQ(instance_count, SetBoneTransforms(instances[0], component_transform, transforms, instance_count));

    ASSERT_NEAR((float)101.f, dmGameObject::GetPosition(instances[0]).getX(), EPSILON);
    ASSERT_NEAR((float)102.f, dmGameObject::GetPosition(instances[0]).getY(), EPSILON);
    ASSERT_NEAR((float)103.f, dmGameObject::GetPosition(instances[0]).getZ(), EPSILON);

    for (uint32_t i = 1; i < instance_count; ++i)
    {
        ASSERT_NEAR((float)i + 1, dmGameObject::GetPosition(instances[i]).getX(), EPSILON);
        ASSERT_NEAR((float)i + 2, dmGameObject::GetPosition(instances[i]).getY(), EPSILON);
        ASSERT_NEAR((float)i + 3, dmGameObject::GetPosition(instances[i]).getZ(), EPSILON);
    }

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        dmGameObject::Delete(m_Collection, instances[i], false);
    }
    dmGameObject::Delete(m_Collection, root, false);
}

TEST_F(HierarchyTest, TestHierarchyBonesMulti)
{
    dmGameObject::HInstance root = dmGameObject::New(m_Collection, "/go.goc");

    // First hierarchy that will be transformed by SetBoneTransforms
    dmGameObject::HInstance p1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetBone(p1, true);
    dmGameObject::SetParent(p1, root);
    dmGameObject::HInstance c1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetBone(c1, true);
    dmGameObject::SetParent(c1, p1);
    dmGameObject::SetPosition(c1, Point3(1.0f, 0.0f, 0.0f));

    // Second hierarchy, attached to the first, that should not be moved with SetBoneTransforms
    dmGameObject::HInstance root2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetParent(root2, c1);
    dmGameObject::HInstance p2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetParent(p2, root2);
    dmGameObject::HInstance c2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::SetBone(c2, true);
    dmGameObject::SetParent(c2, p2);

    dmTransform::Transform t[2];
    t[0].SetIdentity();
    t[1].SetIdentity();
    t[1].SetTranslation(Vector3(2.0f, 0.0f, 0.0f));

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    dmTransform::Transform world = dmGameObject::GetWorldTransform(c2);

    ASSERT_NEAR(1.0f, world.GetTranslation().getX(), EPSILON);

    dmTransform::Transform component_transform;
    component_transform.SetIdentity();
    ASSERT_EQ(2u, SetBoneTransforms(p1, component_transform, t, 2));

    ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    world = dmGameObject::GetWorldTransform(c2);

    ASSERT_NEAR(2.0f, world.GetTranslation().getX(), EPSILON);
    ASSERT_NEAR(0.0f, dmGameObject::GetPosition(c2).getX(), EPSILON);

    dmGameObject::Delete(m_Collection, root, false);
    dmGameObject::Delete(m_Collection, p1, false);
    dmGameObject::Delete(m_Collection, c1, false);
    dmGameObject::Delete(m_Collection, root2, false);
    dmGameObject::Delete(m_Collection, p2, false);
    dmGameObject::Delete(m_Collection, c2, false);
}

TEST_F(HierarchyTest, TestTransformGettersFromScript)
{
    dmGameObject::HInstance controller = dmGameObject::New(m_Collection, "/transform.goc");
    dmGameObject::HInstance parent     = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child      = dmGameObject::New(m_Collection, "/go.goc");

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, controller, "controller"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, parent, "parent"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, child, "child"));

    dmGameObject::SetPosition(parent, Point3(12,4,2));
    dmGameObject::SetPosition(child, Point3(-12,-4,-2));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Test 0: go.get_world_position(child)
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmGameObject::SetScale(parent, Vector3(100, 99, 1));
    dmGameObject::SetScale(child,  Vector3(1.0/100.0, 1.0/99.0, 1.0));

    // Test 1: go.get_world_scale(child)
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    const float rot_90_deg = 3.14159265f / 2.0f;

    dmGameObject::SetRotation(parent, Quat::rotationZ(rot_90_deg));
    dmGameObject::SetRotation(child, Quat::rotationZ(-rot_90_deg));

    // Test 2: go.get_world_rotation(child)
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Test 3: go.get_world_transform(child)
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmGameObject::Delete(m_Collection, controller, false);
    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child, false);
}

TEST_F(HierarchyTest, TestHierarchyFromScript)
{
    dmGameObject::HInstance controller = dmGameObject::New(m_Collection, "/parenting.goc");
    dmGameObject::HInstance parent     = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child      = dmGameObject::New(m_Collection, "/go.goc");

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, controller, "controller"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, parent, "parent"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, child, "child"));

    Point3 parent_pos(12, 4, 2);
    Point3 child_pos(-12, -4, -2);

    dmGameObject::SetPosition(parent, parent_pos);
    dmGameObject::SetPosition(child, child_pos);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Test 0: go.set_parent(child,parent) - set parent
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(parent, dmGameObject::GetParent(child));
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldPosition(child).getX(), EPSILON);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldPosition(child).getY(), EPSILON);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldPosition(child).getZ(), EPSILON);

    // Test 1: go.set_parent(child) - detaching
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ((void*)0, dmGameObject::GetParent(child));
    ASSERT_NEAR(-12.0f, dmGameObject::GetWorldPosition(child).getX(), EPSILON);
    ASSERT_NEAR( -4.0f, dmGameObject::GetWorldPosition(child).getY(), EPSILON);
    ASSERT_NEAR( -2.0f, dmGameObject::GetWorldPosition(child).getZ(), EPSILON);

    // Test 2: go.set_parent(child,parent,true) - set parent and maintain world pos
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(parent, dmGameObject::GetParent(child));
    ASSERT_NEAR(-12.0f, dmGameObject::GetWorldPosition(child).getX(), EPSILON);
    ASSERT_NEAR( -4.0f, dmGameObject::GetWorldPosition(child).getY(), EPSILON);
    ASSERT_NEAR( -2.0f, dmGameObject::GetWorldPosition(child).getZ(), EPSILON);

    // Test 3: go.set_parent(child,not_found) - unknown id should fail
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Test 4: go.set_parent() - default args should detach object that owns script
    dmGameObject::SetParent(controller, parent);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((void*)0, dmGameObject::GetParent(controller));

    dmGameObject::Delete(m_Collection, controller, false);
    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child, false);
}

TEST_F(HierarchyTest, TestEmptyInstance)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, 0x0);

    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go));

    dmGameObject::Result r;
    r = dmGameObject::SetIdentifier(m_Collection, go, "go");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go));

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    dmGameObject::Delete(m_Collection, go, false);
}

struct TestHierarchyCtx
{
    int num_collections;
    int num_gameobjects;
    int num_components;
    int num_subcomponents;
};

static void PrintIndent(int indent, char* buffer, size_t buffer_size)
{
    const int indent_size = 4;
    for (int i = 0; i < indent*indent_size && i < buffer_size; ++i)
        buffer[i] = ' ';
    buffer[indent*indent_size] = 0;
}

static void PrintProperty(dmGameObject::SceneNodeProperty* property, int indent)
{
    char buffer[64];
    PrintIndent(indent, buffer, sizeof(buffer));

    printf("%s'%s': ", buffer, dmHashReverseSafe64(property->m_NameHash));
    switch(property->m_Type)
    {
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH:
        {
            const void* rev_hash = dmHashReverse64(property->m_Value.m_Hash, 0);
            if (rev_hash)
                printf("%s", (const char*)rev_hash);
            else
                printf("%016llX", (unsigned long long)property->m_Value.m_Hash);
        }
        break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_NUMBER: printf("%f", property->m_Value.m_Number); break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN: printf("%d", property->m_Value.m_Bool?1:0); break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3: printf("%f, %f, %f", property->m_Value.m_V4[0], property->m_Value.m_V4[1], property->m_Value.m_V4[2]); break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4: printf("%f, %f, %f, %f", property->m_Value.m_V4[0], property->m_Value.m_V4[1], property->m_Value.m_V4[2], property->m_Value.m_V4[3]); break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_QUAT: printf("%f, %f, %f, %f", property->m_Value.m_V4[0], property->m_Value.m_V4[1], property->m_Value.m_V4[2], property->m_Value.m_V4[3]); break;
    case dmGameObject::SCENE_NODE_PROPERTY_TYPE_URL: printf("%s", property->m_Value.m_URL); break;
    default: break;
    }
    printf("\n");
}

static void TraverseHierarchy(dmGameObject::SceneNode* node, TestHierarchyCtx* ctx, int indent)
{
    switch(node->m_Type)
    {
    case dmGameObject::SCENE_NODE_TYPE_COLLECTION:      ctx->num_collections++; break;
    case dmGameObject::SCENE_NODE_TYPE_GAMEOBJECT:      ctx->num_gameobjects++; break;
    case dmGameObject::SCENE_NODE_TYPE_COMPONENT:       ctx->num_components++; break;
    case dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT:    ctx->num_subcomponents++; break;
    }

    char buffer[64];
    PrintIndent(indent, buffer, sizeof(buffer));

    dmGameObject::SceneNodePropertyIterator pit = TraverseIterateProperties(node);
    while(dmGameObject::TraverseIteratePropertiesNext(&pit))
    {
        PrintProperty( &pit.m_Property, indent );
    }

    dmGameObject::SceneNodeIterator it = dmGameObject::TraverseIterateChildren(node);
    while(dmGameObject::TraverseIterateNext(&it))
    {
        TraverseHierarchy( &it.m_Node, ctx, indent+1 );
    }
}

static void SetProperties(dmGameObject::HInstance instance)
{
    dmGameObject::Prototype::Component* components = instance->m_Prototype->m_Components;
    uint32_t count = instance->m_Prototype->m_ComponentCount;
    uint32_t component_instance_data_index = 0;
    dmGameObject::ComponentSetPropertiesParams params;
    params.m_Instance = instance;
    params.m_PropertySet.m_GetPropertyCallback = 0;
    params.m_PropertySet.m_FreeUserDataCallback = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::ComponentType* type = components[i].m_Type;
        if (type->m_SetPropertiesFunction != 0x0)
        {
            uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
            params.m_UserData = component_instance_data;
            type->m_SetPropertiesFunction(params);
        }
        if (components[i].m_Type->m_InstanceHasUserData)
            ++component_instance_data_index;
    }
}

// Testing the debug inspection api
TEST_F(HierarchyTest, TestIterateHierarchy)
{
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "/go.goc");
    dmGameObject::HInstance child3 = dmGameObject::New(m_Collection, "/go.goc");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, parent, "parent"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, child1, "child1"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, child2, "child2"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, child3, "child3"));

    // parent
    // +--child1
    //   +--child2
    //     +--child3
    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child3, child2);
    dmGameObject::SetParent(child2, child1);

    dmGameObject::SetPosition(child1, Point3(1,2,3));
    dmGameObject::SetPosition(child3, Point3(2,2,2));

    SetProperties(parent);
    SetProperties(child1);
    SetProperties(child2);
    SetProperties(child3);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    dmGameObject::SceneNode root;
    if (!dmGameObject::TraverseGetRoot(m_Register, &root))
        return;

    TestHierarchyCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    printf("\n");
    TraverseHierarchy(&root, &ctx, 0);

    ASSERT_EQ(1, ctx.num_collections);
    ASSERT_EQ(4, ctx.num_gameobjects);
    ASSERT_EQ(4, ctx.num_components);
    ASSERT_EQ(0, ctx.num_subcomponents);

    dmGameObject::Delete(m_Collection, parent, false);
    dmGameObject::Delete(m_Collection, child1, false);
    dmGameObject::Delete(m_Collection, child2, false);
    dmGameObject::Delete(m_Collection, child3, false);

}

#undef EPSILON
