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

#include "test_gamesys_private.h"
#include <gamesys/resources/res_mesh.h>
#include <gamesys/resources/res_material.h>
#include <dlib/profile/profile.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmVMath;

struct Counter
{
    const char* m_Name;
    uint32_t    m_Value;
    uint32_t    m_AddCount;
};

static Counter g_Counters[PROFILER_MAX_NUM_PROPERTIES];

// Use the profiler listener interface, as in dlib's profiler tests, so calls
// through the real component code are observable without elapsed-time checks.
static void* CreateListener()
{
    return g_Counters;
}

static void CreateU32(void* ctx, const char* name, const char*, uint32_t, uint32_t, ProfileIdx idx, ProfileIdx)
{
    ((Counter*)ctx)[idx].m_Name = name;
}

static void AddU32(void* ctx, ProfileIdx idx, uint32_t value)
{
    Counter& counter = ((Counter*)ctx)[idx];
    counter.m_Value += value;
    ++counter.m_AddCount;
}

static ProfileResult BeginFrame(void* ctx)
{
    Counter* counters = (Counter*)ctx;
    for (uint32_t i = 0; i < PROFILER_MAX_NUM_PROPERTIES; ++i)
    {
        counters[i].m_Value = 0;
        counters[i].m_AddCount = 0;
    }
    return PROFILE_RESULT_OK;
}

static void CheckCounter(const char* name, uint32_t value, uint32_t max_calls)
{
    for (uint32_t i = 0; i < PROFILER_MAX_NUM_PROPERTIES; ++i)
    {
        if (g_Counters[i].m_Name && strcmp(g_Counters[i].m_Name, name) == 0)
        {
            ASSERT_EQ(value, g_Counters[i].m_Value);
            ASSERT_LE(g_Counters[i].m_AddCount, max_calls);
            return;
        }
    }
    ASSERT_TRUE(false);
}

class ComponentCounterTest : public GamesysTest<const char*>
{
    public:
    void Run(bool sprite, uint32_t enabled_count, bool world_space = false)
    {
        const uint32_t count = 16;
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
        for (uint32_t i = 0; i < count; ++i)
        {
            const char* prototype = sprite ? "/sprite/texture_transform_sprite.goc" : "/mesh/profile_counts.goc";
            dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, prototype, dmHashBuffer64(&i, sizeof(i)));
            ASSERT_NE((dmGameObject::HInstance)0, instance);
            if (i >= enabled_count)
            {
                dmMessage::URL receiver;
                receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
                receiver.m_Path = dmGameObject::GetIdentifier(instance);
                receiver.m_Fragment = dmHashString64(sprite ? "sprite" : "mesh");
                ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0, &receiver, dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameObjectDDF::Disable::m_DDFDescriptor, 0, 0, 0));
            }
        }
        if (world_space)
        {
            dmGameSystem::MeshResource* mesh;
            ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/mesh/triangle.meshc", (void**)&mesh));
            dmRender::SetMaterialVertexSpace(mesh->m_Material->m_Material, dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD);
            dmResource::Release(m_Factory, mesh);
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            dmRender::ClearRenderObjects(m_RenderContext);
            dmGraphics::ResetDrawCount();
            HProfile profile = ProfileFrameBegin();
            dmRender::RenderListBegin(m_RenderContext);
            ASSERT_TRUE(dmGameObject::Render(m_Collection));
            dmRender::RenderListEnd(m_RenderContext);
            ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_RenderContext, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT));
            ProfileFrameEnd(profile);

            ASSERT_EQ(enabled_count, ((dmRender::RenderContext*)m_RenderContext)->m_RenderList.Size());
            CheckCounter(sprite ? "rmtp_Sprite" : "rmtp_Mesh", enabled_count, 1);
            if (!sprite)
            {
                uint32_t batches = enabled_count ? 1 : 0;
                CheckCounter("rmtp_MeshVertexCount", enabled_count * 3, batches);
                CheckCounter("rmtp_MeshVertexSize", enabled_count * 36, batches);
            }
            uint64_t draws = enabled_count ? (sprite || world_space ? 1 : enabled_count) : 0;
            ASSERT_EQ(draws, dmGraphics::GetDrawCount());
        }
        ASSERT_TRUE(dmGameObject::Final(m_Collection));
    }
};

class SpriteCounterTest : public ComponentCounterTest
{
    public:
    SpriteCounterTest() { SetContentFolder("sprite"); }
};

class MeshCounterTest : public ComponentCounterTest
{
    public:
    MeshCounterTest() { SetContentFolder("mesh"); }
};

// Guard the sprite count's single update per world when all components render.
TEST_F(SpriteCounterTest, AllEnabled)
{
    Run(true, 16);
}

// Guard batching and the exclusion of disabled sprites from the total.
TEST_F(SpriteCounterTest, HalfEnabled)
{
    Run(true, 8);
}

// Guard the zero total when a populated sprite world has no enabled components.
TEST_F(SpriteCounterTest, AllDisabled)
{
    Run(true, 0);
}

// Guard the mesh count and both vertex statistics against per-mesh updates.
TEST_F(MeshCounterTest, AllEnabled)
{
    Run(false, 16);
}

// Guard disabled-mesh filtering in the component and vertex totals.
TEST_F(MeshCounterTest, HalfEnabled)
{
    Run(false, 8);
}

// Guard zero totals without vertex-statistic updates when no meshes render.
TEST_F(MeshCounterTest, AllDisabled)
{
    Run(false, 0);
}

// Keep the existing world-space batching's totals and update count covered.
TEST_F(MeshCounterTest, WorldSpace)
{
    Run(false, 16, true);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    ProfileListener listener = {};
    listener.m_Create = CreateListener;
    listener.m_CreatePropertyU32 = CreateU32;
    listener.m_PropertyAddU32 = AddU32;
    listener.m_FrameBegin = BeginFrame;
    ProfileRegisterProfiler("CounterTest", &listener);
    ProfileInitialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    ProfileFinalize();
    ProfileUnregisterProfiler("CounterTest");
    return result;
}
