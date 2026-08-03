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

#include <particle/particle.h>
#include <script/script.h>

#include "../gui.h"

static void GetURL(dmGui::HScene, dmMessage::URL*)
{
}

static uintptr_t GetUserData(dmGui::HScene)
{
    return 0;
}

static dmhash_t ResolvePath(dmGui::HScene, const char* path)
{
    return dmHashString64(path);
}

static void GetTextMetrics(const void*, const char*, float, bool, float, float, dmGui::TextMetrics* out_metrics)
{
    out_metrics->m_Width = 0.0f;
    out_metrics->m_Height = 0.0f;
    out_metrics->m_MaxAscent = 0.0f;
    out_metrics->m_MaxDescent = 0.0f;
}

TEST(dmGuiParticleNull, EmptyScene)
{
    dmScript::ContextParams script_context_params = {};
    dmScript::HContext      script_context = dmScript::NewContext(script_context_params);
    ASSERT_NE((dmScript::HContext)0, script_context);
    dmScript::Initialize(script_context);

    dmGui::NewContextParams context_params;
    context_params.m_ScriptContext = script_context;
    context_params.m_GetURLCallback = GetURL;
    context_params.m_GetUserDataCallback = GetUserData;
    context_params.m_ResolvePathCallback = ResolvePath;
    context_params.m_GetTextMetricsCallback = GetTextMetrics;
    context_params.m_PhysicalWidth = 1;
    context_params.m_PhysicalHeight = 1;
    context_params.m_DefaultProjectWidth = 1;
    context_params.m_DefaultProjectHeight = 1;

    dmGui::HContext context = dmGui::NewContext(&context_params);
    ASSERT_NE((dmGui::HContext)0, context);

    dmParticle::HParticleContext particle_context = dmParticle::CreateContext(8, 64);
    ASSERT_NE(dmParticle::INVALID_CONTEXT, particle_context);

    dmGui::NewSceneParams scene_params;
    scene_params.m_MaxNodes = 16;
    scene_params.m_MaxAnimations = 8;
    scene_params.m_MaxParticlefxs = 4;
    scene_params.m_MaxParticlefx = 8;
    scene_params.m_ParticlefxContext = particle_context;
    dmGui::HScene scene = dmGui::NewScene(context, &scene_params);
    ASSERT_NE((dmGui::HScene)0, scene);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::UpdateScene(scene, 1.0f / 60.0f));

    dmGui::DeleteScene(scene);
    dmParticle::DestroyContext(particle_context);
    dmGui::DeleteContext(context, script_context);
    dmScript::Finalize(script_context);
    dmScript::DeleteContext(script_context);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
