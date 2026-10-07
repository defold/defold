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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <errno.h>
#include <stdlib.h>
#include <dlib/profile/profile.h>

using namespace dmVMath;

// Run from the gamesys-test-runtime directory. MODEL_BENCHMARK rows contain
// per-sample means in microseconds; startup and scene construction are excluded.
// Timings include null-backend buffer copies, not GPU work or driver uploads.
// The cold row measures the first frame; subsequent rows follow a fixed warmup.
struct BenchmarkOptions
{
    uint32_t m_Instances;
    uint32_t m_Frames;
    uint32_t m_Samples;
    uint32_t m_Warmup;
};

static BenchmarkOptions g_Options = { 10000, 60, 7, 30 };

struct FrameTimes
{
    uint64_t m_Update;
    uint64_t m_Submit;
    uint64_t m_Draw;
    uint64_t m_Total;
};

class ModelBenchmark : public ModelTest
{
    public:
    void SetUp() override
    {
        // Leave room for the animated model's bone game objects.
        m_projectOptions.m_MaxInstances = g_Options.m_Instances + 256;
        ModelTest::SetUp();
        dmGameObject::DeleteCollection(m_Collection);
        m_ModelContext.m_MaxModelCount = g_Options.m_Instances;
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, m_projectOptions.m_MaxInstances, 0);
        ASSERT_NE((dmGameObject::HCollection)0, m_Collection);
    }

    void Run(const char* name, bool component_offset, bool normal_matrix, bool generic_layout, bool animated)
    {
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
        dmArray<dmGameObject::HInstance>  instances;
        dmArray<dmGameObject::HComponent> components;
        instances.SetCapacity(g_Options.m_Instances);
        components.SetCapacity(g_Options.m_Instances);
        dmGameObject::HComponentWorld model_world = 0;
        const Quat                    rotation = Quat::rotationY(0.3f);
        for (uint32_t i = 0; i < g_Options.m_Instances; ++i)
        {
            const char* prototype = component_offset ? "/model/static_vertex_attributes_offset.goc" : "/model/static_vertex_attributes.goc";
            if (animated && i == 0)
                prototype = "/model/benchmark_animated.goc";
            dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, prototype, dmHashBuffer64(&i, sizeof(i)), 0, Point3((float)(i % 100), (float)(i / 100), 0), rotation, Vector3(2, 3, 4));
            ASSERT_NE((dmGameObject::HInstance)0, instance);
            instances.Push(instance);
            uint32_t                 component_type;
            dmGameObject::HComponent component;
            ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(instance, dmHashString64("model"), &component_type, &component, &model_world));
            components.Push(component);
        }

        typedef dmGraphics::VertexAttribute Attribute;
        TestShaderDesc shader("void main() {}");
        shader.AddInput("position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC3);
        shader.AddInput("custom_color", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
        shader.AddInput("instance_world", 2, dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
        if (normal_matrix)
            shader.AddInput("instance_normal", 6, generic_layout ? dmGraphics::ShaderDesc::SHADER_TYPE_MAT3 : dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);

        dmGraphics::HProgram program = dmGraphics::NewProgram(m_GraphicsContext, shader.Get(), 0, 0);
        ASSERT_NE((dmGraphics::HProgram)0, program);
        dmRender::HMaterial material = dmRender::NewMaterial(m_RenderContext, program);
        dmRender::SetMaterialVertexSpace(material, dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL);
        Matrix4   identity = Matrix4::identity();
        Attribute attributes[2] = {};
        for (uint32_t i = 0; i < (normal_matrix ? 2u : 1u); ++i)
        {
            attributes[i].m_NameHash = dmHashString64(i == 0 ? "instance_world" : "instance_normal");
            attributes[i].m_SemanticType = i == 0 ? Attribute::SEMANTIC_TYPE_WORLD_MATRIX : Attribute::SEMANTIC_TYPE_NORMAL_MATRIX;
            attributes[i].m_VectorType = generic_layout && i == 1 ? Attribute::VECTOR_TYPE_MAT3 : Attribute::VECTOR_TYPE_MAT4;
            attributes[i].m_DataType = Attribute::TYPE_FLOAT;
            attributes[i].m_StepFunction = dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
            attributes[i].m_Values.m_BinaryValues.m_Data = (uint8_t*)&identity;
            attributes[i].m_Values.m_BinaryValues.m_Count = dmGraphics::VectorTypeToElementCount(attributes[i].m_VectorType) * sizeof(float);
        }
        dmRender::SetMaterialProgramAttributes(material, attributes, normal_matrix ? 2 : 1);
        dmRender::RenderContext* render_context = (dmRender::RenderContext*)m_RenderContext;
        render_context->m_Material = material;
        const uint32_t stride = dmGraphics::GetVertexDeclarationStride(dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE));

        FrameTimes     cold = {};
        Frame(instances, 0, &cold);
        Report(name, "cold", 0, 1, cold, components, model_world, stride);
        for (uint32_t i = 0; i < g_Options.m_Warmup; ++i)
        {
            FrameTimes ignored;
            Frame(instances, i + 1, &ignored);
        }
        for (uint32_t sample = 0; sample < g_Options.m_Samples; ++sample)
        {
            FrameTimes total = {};
            for (uint32_t i = 0; i < g_Options.m_Frames; ++i)
            {
                FrameTimes times;
                Frame(instances, 1 + g_Options.m_Warmup + sample * g_Options.m_Frames + i, &times);
                total.m_Update += times.m_Update;
                total.m_Submit += times.m_Submit;
                total.m_Draw += times.m_Draw;
                total.m_Total += times.m_Total;
            }
            Report(name, "warm", sample, g_Options.m_Frames, total, components, model_world, stride);
        }

        render_context->m_Material = 0;
        ASSERT_TRUE(dmGameObject::Final(m_Collection));
        dmRender::DeleteMaterial(m_RenderContext, material);
        dmGraphics::DeleteProgram(m_GraphicsContext, program);
    }

    private:
    void Frame(const dmArray<dmGameObject::HInstance>& instances, uint32_t frame, FrameTimes* times)
    {
        // Move every object so repeated frames exercise transform preparation.
        // Changing the inputs and collecting counters are outside the timed region.
        for (uint32_t i = 0; i < instances.Size(); ++i)
            dmGameObject::SetPosition(instances[i], Point3((float)(i % 100) + frame * 0.01f, (float)(i / 100), 0));
        dmRender::ClearRenderObjects(m_RenderContext);
        dmGraphics::ResetDrawCount();
        HProfile profile = ProfileFrameBegin();
        uint64_t begin = dmTime::GetMonotonicTime();
        bool     updated = dmGameObject::Update(m_Collection, &m_UpdateContext);
        bool     post_updated = dmGameObject::PostUpdate(m_Collection);
        uint64_t update = dmTime::GetMonotonicTime();
        dmRender::RenderListBegin(m_RenderContext);
        dmGameObject::Render(m_Collection);
        dmRender::RenderListEnd(m_RenderContext);
        uint64_t         submit = dmTime::GetMonotonicTime();
        dmRender::Result result = dmRender::DrawRenderList(m_RenderContext, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT);
        uint64_t         draw = dmTime::GetMonotonicTime();
        ProfileFrameEnd(profile);
        ASSERT_TRUE(updated);
        ASSERT_TRUE(post_updated);
        ASSERT_EQ(dmRender::RESULT_OK, result);
        ASSERT_EQ(g_Options.m_Instances, ((dmRender::RenderContext*)m_RenderContext)->m_RenderList.Size());
        times->m_Update = update - begin;
        times->m_Submit = submit - update;
        times->m_Draw = draw - submit;
        times->m_Total = draw - begin;
    }

    void Report(const char* name, const char* state, uint32_t sample, uint32_t frames, const FrameTimes& times, const dmArray<dmGameObject::HComponent>& components, dmGameObject::HComponentWorld world, uint32_t stride)
    {
        uint32_t caches = 0;
        for (uint32_t i = 0; i < components.Size(); ++i)
        {
            uint32_t initialized;
            dmGameSystem::GetModelComponentAttributeRenderDataCount(components[i], &initialized);
            caches += initialized;
        }
        dmRender::BufferedRenderBuffer* buffer;
        dmGameSystem::GetModelWorldInstanceRenderBuffer(world, &buffer);
        ASSERT_GT(buffer->m_Buffers.Size(), 0u);
        uint32_t bytes = dmGraphics::GetVertexBufferSize(buffer->m_Buffers[0]);
        ASSERT_EQ(g_Options.m_Instances * stride, bytes);
        uint8_t world_batches, local_batches, instanced_batches;
        dmGameSystem::GetModelWorldRenderBatchStats(world, &world_batches, &local_batches, &instanced_batches);
        ASSERT_EQ(0u, world_batches);
        ASSERT_EQ(0u, local_batches);
        ASSERT_GT(instanced_batches, 0u);
        printf("MODEL_BENCHMARK,%s,%s,%u,%u,%u,%.3f,%.3f,%.3f,%.3f,%llu,%u,%u\n", name, state, g_Options.m_Instances, sample, frames, (double)times.m_Update / frames, (double)times.m_Submit / frames, (double)times.m_Draw / frames, (double)times.m_Total / frames, (unsigned long long)dmGraphics::GetDrawCount(), bytes, caches);
    }
};

// Measure static models with an instance world matrix and immutable custom vertex data.
TEST_F(ModelBenchmark, WorldMatrix)
{
    Run("world_matrix", false, false, false, false);
}

// Measure the native world/normal matrix layout with nonuniform instance scaling.
TEST_F(ModelBenchmark, WorldNormalMatrices)
{
    Run("world_normal_matrices", false, true, false, false);
}

// Retain a control with a nonidentity component transform.
TEST_F(ModelBenchmark, ComponentOffset)
{
    Run("component_offset", true, false, false, false);
}

// Retain a control that requires the generic writer's mat4-to-mat3 conversion.
TEST_F(ModelBenchmark, GenericMatrixConversion)
{
    Run("generic_matrix_conversion", false, true, true, false);
}

// Retain a mixed-context control where an animated rig prevents the static-only shortcut.
TEST_F(ModelBenchmark, MixedAnimated)
{
    Run("mixed_animated", false, false, false, true);
}

static bool ParseOption(const char* text, uint32_t* value)
{
    if (text[0] < '0' || text[0] > '9')
        return false;
    errno = 0;
    char*         end;
    unsigned long parsed = strtoul(text, &end, 10);
    if (errno || *end || parsed > 1000000)
        return false;
    *value = (uint32_t)parsed;
    return true;
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    uint32_t*   values[] = { &g_Options.m_Instances, &g_Options.m_Frames, &g_Options.m_Samples, &g_Options.m_Warmup };
    const char* names[] = { "--instances=", "--frames=", "--samples=", "--warmup=" };
    int         remaining = 1;
    for (int i = 1; i < argc; ++i)
    {
        bool matched = false;
        for (uint32_t j = 0; j < DM_ARRAY_SIZE(names); ++j)
        {
            size_t length = strlen(names[j]);
            if (strncmp(argv[i], names[j], length) == 0)
            {
                if (!ParseOption(argv[i] + length, values[j]) || (j != 3 && *values[j] == 0))
                {
                    fprintf(stderr, "Invalid benchmark option: %s\n", argv[i]);
                    return 1;
                }
                matched = true;
                break;
            }
        }
        if (!matched)
            argv[remaining++] = argv[i];
    }
    argc = remaining;
    dmExportedSymbols();
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    ProfileInitialize();
    printf("marker,case,state,instances,sample,frames,update_us,submit_us,draw_us,cpu_frame_us,draw_calls,instance_bytes,custom_caches\n");
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    ProfileFinalize();
    return result;
}
