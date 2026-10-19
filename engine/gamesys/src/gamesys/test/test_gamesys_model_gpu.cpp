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
#include <dlib/math.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmVMath;

class ModelGpuTest : public ModelTest
{
    protected:
    dmGameSystem::MaterialResource* m_Resources[2];
    dmRender::HMaterial             m_OriginalMaterials[2];
    dmRender::HMaterial             m_Materials[2];
    dmGraphics::HProgram            m_Program;
    dmGraphics::HRenderTarget       m_Target;

    void                            LoadShaderBytes(const char* path, dmArray<uint8_t>& bytes)
    {
        FILE* file = fopen(path, "rb");
        ASSERT_NE((FILE*)0, file);
        fseek(file, 0, SEEK_END);
        uint32_t size = ftell(file);
        rewind(file);
        bytes.SetCapacity(size);
        bytes.SetSize(size);
        ASSERT_EQ(size, fread(bytes.Begin(), 1, size, file));
        fclose(file);
    }

    void SetUp() override
    {
        memset(m_Resources, 0, sizeof(m_Resources));
        memset(m_Materials, 0, sizeof(m_Materials));
        m_Program = 0;
        m_Target = 0;
        ModelTest::SetUp();
        ASSERT_EQ(dmGraphics::ADAPTER_FAMILY_METAL, dmGraphics::GetInstalledAdapterFamily());

        dmArray<uint8_t> bytes;
        LoadShaderBytes(INDIRECT_SHADER_FILE, bytes);
        dmRender::RenderContextParams params;
        params.m_InstanceCullingShaderProgramDesc = bytes.Begin();
        params.m_InstanceCullingShaderProgramDescSize = bytes.Size();
        dmRender::InitializeGpuIndirect(m_RenderContext, params);
        ASSERT_TRUE(dmRender::IsGpuIndirectSupported(m_RenderContext));

        dmGraphics::ShaderDesc* desc = 0;
        LoadShaderBytes(MODEL_GPU_SHADER_FILE, bytes);
        ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &desc));
        char error[512] = {};
        m_Program = dmGraphics::NewProgram(m_GraphicsContext, desc, error, sizeof(error));
        dmDDF::FreeMessage(desc);
        ASSERT_NE((dmGraphics::HProgram)0, m_Program);
        const char* paths[] = { "/model/dynamic_vertex_attributes.materialc", "/model/material_world_vertexspace.materialc" };
        for (uint32_t i = 0; i < 2; ++i)
        {
            ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, paths[i], (void**)&m_Resources[i]));
            m_OriginalMaterials[i] = m_Resources[i]->m_Material;
            m_Materials[i] = dmRender::NewMaterial(m_RenderContext, m_Program);
            dmRender::SetMaterialVertexSpace(m_Materials[i], dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL);
            dmRender::SetMaterialTags(m_Materials[i], 0, 0);
            dmGraphics::VertexAttribute attribute = {};
            attribute.m_Name = (char*)"custom_world";
            attribute.m_NameHash = dmHashString64(attribute.m_Name);
            attribute.m_SemanticType = dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX;
            attribute.m_VectorType = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4;
            attribute.m_DataType = dmGraphics::VertexAttribute::TYPE_FLOAT;
            attribute.m_StepFunction = i ? dmGraphics::VERTEX_STEP_FUNCTION_VERTEX : dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
            dmRender::SetMaterialProgramAttributes(m_Materials[i], &attribute, 1);
            dmRender::SetMaterialProgramConstantType(m_Materials[i], dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
            Vector4 tint(1, 0.25f, 0, 1);
            dmRender::SetMaterialProgramConstant(m_Materials[i], dmHashString64("tint"), &tint, 1);
            m_Resources[i]->m_Material = m_Materials[i];
        }
        dmRender::SetViewMatrix(m_RenderContext, Matrix4::identity());
        dmRender::SetProjectionMatrix(m_RenderContext, Matrix4::orthographic(-2, 2, -2, 2, -10, 10));
        dmGraphics::RenderTargetCreationParams target = {};
        target.m_ColorBufferCreationParams[0].m_Width = target.m_ColorBufferParams[0].m_Width = 64;
        target.m_ColorBufferCreationParams[0].m_Height = target.m_ColorBufferParams[0].m_Height = 64;
        target.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        target.m_ColorBufferStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;
        target.m_DepthBufferCreationParams.m_Width = target.m_DepthBufferParams.m_Width = 64;
        target.m_DepthBufferCreationParams.m_Height = target.m_DepthBufferParams.m_Height = 64;
        target.m_DepthBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;
        m_Target = dmGraphics::NewRenderTarget(m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT, target);
        ASSERT_NE((dmGraphics::HRenderTarget)0, m_Target);
    }

    void TearDown() override
    {
        if (m_Target)
            dmGraphics::DeleteRenderTarget(m_GraphicsContext, m_Target);
        for (uint32_t i = 0; i < 2; ++i)
        {
            if (m_Resources[i])
            {
                m_Resources[i]->m_Material = m_OriginalMaterials[i];
                dmResource::Release(m_Factory, m_Resources[i]);
            }
            if (m_Materials[i])
                dmRender::DeleteMaterial(m_RenderContext, m_Materials[i]);
        }
        if (m_Program)
            dmGraphics::DeleteProgram(m_GraphicsContext, m_Program);
        ModelTest::TearDown();
    }

    void DrawModels(bool gpu, bool depth_write, uint32_t expected_gpu, uint32_t expected_cpu, uint8_t* pixels)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGraphics::BeginFrame(m_GraphicsContext);
        dmGraphics::SetRenderTarget(m_GraphicsContext, m_Target, dmGraphics::RenderTargetBindingParams());
        dmGraphics::SetViewport(m_GraphicsContext, 0, 0, 64, 64);
        dmGraphics::SetColorMask(m_GraphicsContext, true, true, true, true);
        dmGraphics::EnableState(m_GraphicsContext, dmGraphics::STATE_DEPTH_TEST);
        dmGraphics::SetDepthMask(m_GraphicsContext, depth_write);
        dmGraphics::SetDepthFunc(m_GraphicsContext, dmGraphics::COMPARE_FUNC_LEQUAL);
        dmGraphics::DisableState(m_GraphicsContext, dmGraphics::STATE_BLEND);
        dmGraphics::DisableState(m_GraphicsContext, dmGraphics::STATE_STENCIL_TEST);
        dmGraphics::DisableState(m_GraphicsContext, dmGraphics::STATE_CULL_FACE);
        dmGraphics::Clear(m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 255, 1, 0);
        dmRender::RenderListBegin(m_RenderContext);
        dmGameObject::Render(m_Collection);
        dmRender::RenderListEnd(m_RenderContext);
        dmRender::RenderContext* context = (dmRender::RenderContext*)m_RenderContext;
        dmGraphics::HProgram     culling = context->m_GpuIndirect.m_Program;
        if (!gpu)
            context->m_GpuIndirect.m_Program = 0;
        dmRender::FrustumOptions frustum;
        frustum.m_Matrix = Matrix4::orthographic(-2, 2, -2, 2, -10, 10);
        frustum.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
        dmRender::Result result = dmRender::DrawRenderList(m_RenderContext, 0, 0, &frustum, dmRender::SORT_NONE);
        context->m_GpuIndirect.m_Program = culling;
        ASSERT_EQ(dmRender::RESULT_OK, result);
        uint32_t                                  gpu_batches = 0;
        uint32_t                                  cpu_instances = 0;
        dmArray<dmGraphics::HAsyncBufferReadback> snapshots;
        snapshots.SetCapacity(context->m_RenderObjects.Size());
        for (uint32_t i = 0; i < context->m_RenderObjects.Size(); ++i)
        {
            dmRender::HGpuRenderBatch batch = context->m_GpuIndirect.m_RenderBatches[i];
            if (batch)
            {
                ++gpu_batches;
                snapshots.Push(dmGraphics::RequestStorageBufferReadback(m_GraphicsContext, batch->m_ArgumentBuffer, 0, 5 * sizeof(uint32_t)));
            }
            else
                cpu_instances += dmMath::Max(1u, context->m_RenderObjects[i]->m_InstanceCount);
        }
        dmGraphics::ReadPixels(m_GraphicsContext, 0, 0, 64, 64, pixels, 64 * 64 * 4);
        dmGraphics::SetRenderTarget(m_GraphicsContext, 0, dmGraphics::RenderTargetBindingParams());
        // ReadPixels completed the draw and the earlier argument snapshot.
        uint32_t gpu_instances = 0;
        for (uint32_t i = 0; i < snapshots.Size(); ++i)
        {
            uint32_t arguments[5];
            ASSERT_TRUE(dmGraphics::PollStorageBufferReadback(snapshots[i], arguments));
            gpu_instances += arguments[1];
            dmGraphics::DeleteStorageBufferReadback(snapshots[i]);
        }
        dmGraphics::Flip(m_GraphicsContext);
        dmRender::ClearRenderObjects(m_RenderContext);
        ASSERT_EQ(expected_gpu != 0, gpu_batches != 0);
        ASSERT_EQ(expected_gpu, gpu_instances);
        ASSERT_EQ(expected_cpu, cpu_instances);
    }
};

// Real model components must choose indirect draws only for eligible layouts,
// refresh that choice after material changes, and match CPU-culling pixels.
TEST_F(ModelGpuTest, MaterialChangesAndCpuFallback)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance instances[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        char id[16];
        dmSnPrintf(id, sizeof(id), "/model_%u", i);
        instances[i] = Spawn(m_Factory, m_Collection, "/model/static_vertex_attributes.goc", dmHashString64(id), 0, Point3(i == 2 ? 100.0f : i - 0.5f, 0, 0), Quat::identity(), Vector3(0.3f));
        ASSERT_NE((dmGameObject::HInstance)0, instances[i]);
    }
    for (uint32_t phase = 0; phase < 4; ++phase)
    {
        if (phase == 1 || phase == 2)
        {
            const char*                   path = phase == 1 ? "/model/material_world_vertexspace.materialc" : "/model/dynamic_vertex_attributes.materialc";
            dmGameObject::PropertyOptions options;
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(instances[1], dmHashString64("model"), dmHashString64("material"), options, dmGameObject::PropertyVar(dmHashString64(path))));
        }
        uint8_t cpu[64 * 64 * 4];
        uint8_t gpu[sizeof(cpu)];
        DrawModels(false, phase != 3, 0, 2, cpu);
        uint32_t gpu_visible = phase == 3 ? 0 : phase == 1 ? 1 :
                                                             2;
        DrawModels(true, phase != 3, gpu_visible, 2 - gpu_visible, gpu);
        ASSERT_EQ(0, memcmp(cpu, gpu, sizeof(cpu)));
        uint32_t colored = 0;
        for (uint32_t i = 0; i < sizeof(gpu); i += 4)
            colored += gpu[i + 2] != 0;
        ASSERT_GT(colored, 0u);
    }
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

extern "C" void dmExportedSymbols();

int             main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
