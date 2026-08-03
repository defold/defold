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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <string.h>

#include "../particle.h"

namespace dmGameSystem
{
    dmParticle::FetchResourcesResult FetchResourcesCallback(const dmParticle::FetchResourcesParams* params, dmParticle::FetchResourcesData* out_data);
}

TEST(dmParticleNull, EmptyContext)
{
    dmParticle::HParticleContext context = dmParticle::CreateContext(64, 1024);
    ASSERT_NE(dmParticle::INVALID_CONTEXT, context);

    dmParticle::HInstance instance = dmParticle::CreateInstance(context, dmParticle::INVALID_PROTOTYPE, 0);
    ASSERT_EQ(dmParticle::INVALID_INSTANCE, instance);
    ASSERT_TRUE(dmParticle::IsSleeping(context, instance));
    ASSERT_EQ(0u, dmParticle::GetInstanceEmitterCount(context, instance));
    ASSERT_EQ(0u, dmParticle::GetEmitterVertexCount(context, instance, 0));
    ASSERT_EQ(0u, dmParticle::GetVertexBufferSize(1024, 40));

    dmParticle::EmitterRenderData* render_data = (dmParticle::EmitterRenderData*)1;
    dmParticle::GetEmitterRenderData(context, instance, 0, &render_data);
    ASSERT_EQ((dmParticle::EmitterRenderData*)0, render_data);

    dmGraphics::VertexAttributeInfos attribute_infos;
    uint32_t                         vertex_buffer_size = 1;
    ASSERT_EQ(dmParticle::GENERATE_VERTEX_DATA_INVALID_INSTANCE,
              dmParticle::GenerateVertexData(context,
                                             instance,
                                             0,
                                             attribute_infos,
                                             dmVMath::Vector4(1.0f),
                                             0,
                                             0,
                                             &vertex_buffer_size));
    ASSERT_EQ(0u, vertex_buffer_size);

    uint8_t fetch_resources_buffer[sizeof(dmParticle::FetchResourcesData)];
    memset(fetch_resources_buffer, 0xff, sizeof(fetch_resources_buffer));
    dmParticle::FetchResourcesData* fetch_resources_data = (dmParticle::FetchResourcesData*)fetch_resources_buffer;
    ASSERT_EQ(dmParticle::FETCH_RESOURCES_NOT_FOUND, dmGameSystem::FetchResourcesCallback(0, fetch_resources_data));
    ASSERT_EQ((void*)0, fetch_resources_data->m_Material);
    ASSERT_EQ((void*)0, fetch_resources_data->m_AnimationData.m_Texture);

    dmParticle::Update(context, 1.0f / 60.0f, 0);
    dmParticle::DestroyInstance(context, instance);
    dmParticle::DestroyContext(context);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
