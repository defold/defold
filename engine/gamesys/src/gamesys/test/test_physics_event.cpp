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
#include <extension/extension.hpp>
#include <dmsdk/physics/physics_event.hpp>
#include "test_gamesys.h"
#include "components/box2d/comp_collision_object_box2d.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"

#if defined(DM_PHYSICS_BOX2D_V3)
#include <box2d/box2d.h>
#else
#include <Box2D/Box2D.h>
#endif
#include <btBulletDynamicsCommon.h>

struct PhysicsEventProbe
{
    HContextRegistry     m_Registry;
    const void*          m_World;
    dmPhysics::EventType m_Type;
    uint32_t             m_Count[2];
    uint32_t             m_Bodies[2];
    uint32_t             m_Contacts[2];
    float                m_Dt;
};

static PhysicsEventProbe* g_PhysicsProbe;
static uint32_t g_PhysicsListener[2] = { 0, 1 };

static void ObservePhysicsStep(dmExtension::Params* params, dmhash_t event_id, const void* data, uint32_t data_size)
{
    if (!g_PhysicsProbe || event_id != dmPhysics::EVENT_POST_STEP)
    {
        return;
    }
    ASSERT_NE((const void*)0, data);
    ASSERT_EQ(sizeof(dmPhysics::Event), data_size);
    const dmPhysics::Event* event = (const dmPhysics::Event*)data;
    ASSERT_EQ(g_PhysicsProbe->m_Registry, dmExtension::ParamsGetContextRegistry(params));
    ASSERT_EQ(g_PhysicsProbe->m_World, event->m_World);
    ASSERT_GT(event->m_Dt, 0.0f);
    ASSERT_EQ((lua_State*)ContextRegistryGet(g_PhysicsProbe->m_Registry, LUA_CONTEXT_NAME), params->m_L);
    ASSERT_EQ((HConfigFile)ContextRegistryGet(g_PhysicsProbe->m_Registry, CONFIGFILE_CONTEXT_NAME), params->m_ConfigFile);
    float extension_dt = -1.0f;
    ASSERT_TRUE(dmExtension::ParamsGetFloat(params, dmExtension::PARAM_DT, &extension_dt));
    ASSERT_EQ(0.0f, extension_dt);
    const uint32_t* listener = (const uint32_t*)dmExtension::ParamsGetUserData(params);
    ASSERT_TRUE(listener == &g_PhysicsListener[0] || listener == &g_PhysicsListener[1]);
    uint32_t index = *listener;
    ++g_PhysicsProbe->m_Count[index];
    g_PhysicsProbe->m_Type = event->m_Type;
    g_PhysicsProbe->m_Dt = event->m_Dt;
    if (event->m_Type == dmPhysics::EVENT_TYPE_BULLET3D)
    {
        const btDiscreteDynamicsWorld* world = (const btDiscreteDynamicsWorld*)event->m_World;
        g_PhysicsProbe->m_Bodies[index] = world->getNumCollisionObjects();
        return;
    }
#if defined(DM_PHYSICS_BOX2D_V3)
    ASSERT_EQ(dmPhysics::EVENT_TYPE_BOX2D_V3, event->m_Type);
    b2WorldId world = *(const b2WorldId*)event->m_World;
    ASSERT_TRUE(b2World_IsValid(world));
    g_PhysicsProbe->m_Bodies[index] = b2World_GetCounters(world).bodyCount;
    b2ContactEvents contacts = b2World_GetContactEvents(world);
    g_PhysicsProbe->m_Contacts[index] = contacts.beginCount;
    ASSERT_EQ(contacts.beginCount, b2World_GetContactEvents(world).beginCount);
#else
    ASSERT_EQ(dmPhysics::EVENT_TYPE_BOX2D, event->m_Type);
    const b2World* world = (const b2World*)event->m_World;
    ASSERT_FALSE(world->IsLocked());
    g_PhysicsProbe->m_Bodies[index] = world->GetBodyCount();
    for (const b2Contact* contact = world->GetContactList(); contact; contact = contact->GetNext())
    {
        g_PhysicsProbe->m_Contacts[index] += contact->IsTouching();
    }
#endif
}

static dmExtension::Result RegisterPhysicsListener0(dmExtension::HExtension extension)
{
    dmExtension::SetUserData(extension, &g_PhysicsListener[0]);
    dmExtension::SetEventFn(extension, ObservePhysicsStep);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result RegisterPhysicsListener1(dmExtension::HExtension extension)
{
    dmExtension::SetUserData(extension, &g_PhysicsListener[1]);
    dmExtension::SetEventFn(extension, ObservePhysicsStep);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(TestPhysicsListener0, RegisterPhysicsListener0, 0);
DM_DECLARE_EXTENSION(TestPhysicsListener1, RegisterPhysicsListener1, 0);

static void RegisterPhysicsListeners()
{
    static bool registered = false;
    if (!registered)
    {
        TestPhysicsListener0();
        TestPhysicsListener1();
        registered = true;
    }
}

class PhysicsPostStepTest : public ComponentTest
{
    protected:
    PhysicsEventProbe m_Probe;

    void SetUp()
    {
        RegisterPhysicsListeners();
        ComponentTest::SetUp();
        memset(&m_Probe, 0, sizeof(m_Probe));
        m_Probe.m_Registry = m_ContextRegistry;
        uint32_t type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("collisionobjectc"));
        m_Probe.m_World = dmGameSystem::CompCollisionObjectGetBox2DWorld(dmGameObject::GetWorld(m_Collection, type));
        g_PhysicsProbe = &m_Probe;
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
        ASSERT_NE((dmGameObject::HInstance)0, Spawn(m_Factory, m_Collection, "/collision_object/body.goc", dmHashString64("/physics_event_a"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1)));
        ASSERT_NE((dmGameObject::HInstance)0, Spawn(m_Factory, m_Collection, "/collision_object/body.goc", dmHashString64("/physics_event_b"), 0, dmVMath::Point3(2, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1)));
    }

    void TearDown()
    {
        g_PhysicsProbe = 0;
        ComponentTest::TearDown();
    }
};

// Both observers see the live native world and contact data after solving, with the owning registry and world dt.
TEST_F(PhysicsPostStepTest, NativeWorldAndContacts)
{
    m_UpdateContext.m_DT = 1.0f / 30.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1u, m_Probe.m_Count[0]);
    ASSERT_EQ(1u, m_Probe.m_Count[1]);
    ASSERT_EQ(2u, m_Probe.m_Bodies[0]);
    ASSERT_EQ(m_Probe.m_Bodies[0], m_Probe.m_Bodies[1]);
    ASSERT_GT(m_Probe.m_Contacts[0], 0u);
    ASSERT_EQ(m_Probe.m_Contacts[0], m_Probe.m_Contacts[1]);
    ASSERT_EQ(m_UpdateContext.m_DT, m_Probe.m_Dt);
}

// Fixed mode publishes per actual world update, including multiple fixed updates and frames without a fixed step.
TEST_F(PhysicsPostStepTest, FixedStepTiming)
{
    m_PhysicsContextBox2D.m_BaseContext.m_UseFixedTimestep = true;
    m_UpdateContext.m_FixedUpdateFrequency = 60;
    m_UpdateContext.m_DT = 3.5f / 60.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(3u, m_Probe.m_Count[0]);
    ASSERT_EQ(3u, m_Probe.m_Count[1]);
    ASSERT_EQ(1.0f / 60.0f, m_Probe.m_Dt);
    m_UpdateContext.m_DT = 0.2f / 60.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(3u, m_Probe.m_Count[0]);
    ASSERT_EQ(3u, m_Probe.m_Count[1]);
}

// A paused, zero-duration world update must not replay a post-step notification.
TEST_F(PhysicsPostStepTest, ZeroDeltaDoesNotPublish)
{
    m_UpdateContext.m_DT = 0.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(0u, m_Probe.m_Count[0]);
    ASSERT_EQ(0u, m_Probe.m_Count[1]);
}

class PhysicsPostStep3DTest : public Bullet3DComponentTest
{
    protected:
    PhysicsEventProbe m_Probe;

    void SetUp()
    {
        RegisterPhysicsListeners();
        Bullet3DComponentTest::SetUp();
        memset(&m_Probe, 0, sizeof(m_Probe));
        m_Probe.m_Registry = m_ContextRegistry;
        uint32_t type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("collisionobjectc"));
        m_Probe.m_World = dmGameSystem::CompCollisionObjectGetBullet3DWorld(dmGameObject::GetWorld(m_Collection, type));
        g_PhysicsProbe = &m_Probe;
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
        ASSERT_NE((dmGameObject::HInstance)0, Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_rigid_body.goc", dmHashString64("/physics_event_3d"), 0, dmVMath::Point3(0, 0, 0), dmVMath::Quat(0, 0, 0, 1), dmVMath::Vector3(1, 1, 1)));
    }

    void TearDown()
    {
        g_PhysicsProbe = 0;
        Bullet3DComponentTest::TearDown();
    }
};

// Bullet publishes its real dynamics world after stepping through the same engine-thread event path.
TEST_F(PhysicsPostStep3DTest, NativeWorld)
{
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1u, m_Probe.m_Count[0]);
    ASSERT_EQ(1u, m_Probe.m_Count[1]);
    ASSERT_EQ(dmPhysics::EVENT_TYPE_BULLET3D, m_Probe.m_Type);
    ASSERT_EQ(1u, m_Probe.m_Bodies[0]);
    ASSERT_EQ(m_Probe.m_Bodies[0], m_Probe.m_Bodies[1]);
    ASSERT_EQ(m_UpdateContext.m_DT, m_Probe.m_Dt);
}
