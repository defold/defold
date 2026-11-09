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
#include "../../../../graphics/src/graphics_adapter.h"
#include <dlib/profile/profile.h>
#include <errno.h>
#include <stdlib.h>

namespace dmGameSystem
{
    struct SpriteWorld;
    static uint32_t g_QuadMode;
    static uint64_t g_QuadPackTime;
    static uint64_t g_QuadUploadTime;
    static bool RenderQuadBenchmarkBatch(SpriteWorld*, dmRender::HRenderContext, dmRender::RenderListEntry*, uint32_t*, uint32_t*);
    static void UpdateQuadBenchmarkBuffers(dmRender::HRenderContext);
    static void BeginQuadBenchmarkDispatch();
    static bool EndQuadBenchmarkDispatch(dmRender::HRenderContext);
}

// Replace the library's component object in this executable only. This keeps
// the real baseline and prototype on the same update/sort/dispatch machinery,
// without adding a runtime switch or a new material contract to the engine.
#include "../components/comp_sprite.cpp"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmVMath;

static dmGraphics::GraphicsAdapterRegisterFunctionsCb g_NullRegister;
static dmGraphics::DrawFn g_NullDraw;
static uint64_t g_SubmittedIndices;

// NullDrawElements normally allocates and expands every indexed vertex for
// graphics tests, but ignores instance_count when doing so. That asymmetry is
// not a rendering cost. Keep null draw setup/counters without vertex capture;
// uploaded data and render-object bindings are validated independently below.
static void CountQuadDraw(dmGraphics::HContext context, dmGraphics::PrimitiveType primitive, uint32_t first, uint32_t count,
                          dmGraphics::Type type, dmGraphics::HIndexBuffer indices, uint32_t instances)
{
    g_SubmittedIndices += (uint64_t)count * dmMath::Max(1u, instances);
    g_NullDraw(context, primitive, first, count, instances);
}

static dmGraphics::GraphicsAdapterFunctionTable RegisterQuadBenchmarkAdapter()
{
    dmGraphics::GraphicsAdapterFunctionTable functions = g_NullRegister();
    g_NullDraw = functions.m_Draw;
    functions.m_DrawElements = CountQuadDraw;
    return functions;
}

namespace dmGameSystem
{
    static dmArray<uint8_t> g_QuadData;
    static dmRender::HBufferedRenderBuffer g_QuadBuffer;
    static dmGraphics::HVertexBuffer g_QuadVertices;
    static dmGraphics::HIndexBuffer g_QuadIndices;
    static uint32_t g_QuadDispatch;
    static const uint16_t QUAD_INDICES[] = { 0, 3, 2, 0, 2, 1 };
    static const float QUAD_CORNERS[][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 1, 0 } };

    static uint32_t InstanceStride()
    {
        return g_QuadMode == 1 ? 104 : 64;
    }

    static void UpdateQuadBenchmarkBuffers(dmRender::HRenderContext context)
    {
        if (g_QuadMode)
        {
            dmRender::TrimBuffer(context, g_QuadBuffer);
            dmRender::RewindBuffer(context, g_QuadBuffer);
            g_QuadDispatch = 0;
        }
    }

    static void BeginQuadBenchmarkDispatch()
    {
        g_QuadData.SetSize(0);
    }

    static bool EndQuadBenchmarkDispatch(dmRender::HRenderContext context)
    {
        if (!g_QuadMode)
        {
            return false;
        }
        if (!g_QuadData.Empty())
        {
            uint64_t begin = dmTime::GetMonotonicTime();
            dmRender::SetBufferData(context, g_QuadBuffer, g_QuadData.Size(), g_QuadData.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            g_QuadUploadTime += dmTime::GetMonotonicTime() - begin;
            ++g_QuadDispatch;
        }
        return true;
    }

    static bool RenderQuadBenchmarkBatch(SpriteWorld* world, dmRender::HRenderContext context, dmRender::RenderListEntry* entries, uint32_t* begin, uint32_t* end)
    {
        if (!g_QuadMode)
        {
            return false;
        }
        const SpriteComponent* first = &world->m_Components.Get((uint32_t)entries[*begin].m_UserData);
        dmRender::HMaterial material = GetRenderMaterial(context, first);
        const uint32_t offset = g_QuadData.Size();
        const uint32_t stride = InstanceStride();
        const uint32_t bytes = (uint32_t)(end - begin) * stride;
        if (g_QuadData.Remaining() < bytes)
        {
            g_QuadData.OffsetCapacity(bytes - g_QuadData.Remaining());
        }
        uint8_t* output = g_QuadData.End();
        uint64_t pack_begin = dmTime::GetMonotonicTime();
        for (uint32_t* i = begin; i != end; ++i)
        {
            const SpriteComponent* component = &world->m_Components.Get((uint32_t)entries[*i].m_UserData);
            AnimationData* animation = GetOrCreateAnimationData(world, component);
            assert(animation->m_CanUseQuads && !component->m_UseSlice9 && component->m_NumTextures == 1);
            float* uvs[MAX_TEXTURE_COUNT] = {};
            float* pages[MAX_TEXTURE_COUNT] = {};
            float* transforms[MAX_TEXTURE_COUNT] = {};
            ResolveUVDataFromQuads(component, animation, world->m_ScratchUVs, uvs, pages, transforms);
            float px, py;
            GetFlippedPivot(component, &px, &py);
            const float width = component->m_Size.getX();
            const float height = component->m_Size.getY();
            const Vector4 x = component->m_World.getCol0() * width;
            const Vector4 y = component->m_World.getCol1() * height;
            const Vector4 origin = component->m_World * Vector4((-0.5f - px) * width, (-0.5f - py) * height, 0, 1);
            const float* uv = uvs[0];
            float data[26];
            if (g_QuadMode == 1)
            {
                const Matrix4 transform(x, y, component->m_World.getCol2(), origin);
                memcpy(data, &transform, 64);
                data[16] = uv[6] - uv[0];
                data[17] = uv[7] - uv[1];
                data[18] = 0;
                data[19] = uv[2] - uv[0];
                data[20] = uv[3] - uv[1];
                data[21] = 0;
                data[22] = uv[0];
                data[23] = uv[1];
                data[24] = 1;
                data[25] = *pages[0];
            }
            else
            {
                // Four vec4 inputs: three world XYZ vectors, six affine UV
                // coefficients, and page. Supports a sprite plane tilted in 3D.
                for (uint32_t c = 0; c < 3; ++c)
                {
                    data[c] = x.getElem(c);
                    data[3 + c] = y.getElem(c);
                    data[6 + c] = origin.getElem(c);
                }
                data[9] = uv[6] - uv[0];
                data[10] = uv[7] - uv[1];
                data[11] = uv[2] - uv[0];
                data[12] = uv[3] - uv[1];
                data[13] = uv[0];
                data[14] = uv[1];
                data[15] = *pages[0];
            }
            memcpy(output, data, stride);
            output += stride;
        }
        g_QuadPackTime += dmTime::GetMonotonicTime() - pack_begin;
        g_QuadData.SetSize(offset + bytes);
        if (dmRender::GetBufferIndex(context, g_QuadBuffer) < g_QuadDispatch)
        {
            dmRender::AddRenderBuffer(context, g_QuadBuffer);
        }
        if (world->m_RenderObjectsInUse == world->m_RenderObjects.Capacity())
        {
            world->m_RenderObjects.OffsetCapacity(1);
            world->m_RenderObjects.Push(new dmRender::RenderObject);
        }
        dmRender::RenderObject& ro = *world->m_RenderObjects[world->m_RenderObjectsInUse++];
        ro.Init();
        ro.m_Material = GetComponentMaterial(first);
        ro.m_VertexDeclarations[0] = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
        ro.m_VertexDeclarations[1] = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
        assert(dmGraphics::GetVertexDeclarationStride(ro.m_VertexDeclarations[1]) == stride);
        ro.m_VertexBuffers[0] = g_QuadVertices;
        ro.m_VertexBuffers[1] = (dmGraphics::HVertexBuffer)dmRender::GetBuffer(context, g_QuadBuffer);
        ro.m_VertexBufferOffsets[1] = offset;
        ro.m_IndexBuffer = g_QuadIndices;
        ro.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexCount = 6;
        ro.m_InstanceCount = (uint32_t)(end - begin);
        ro.m_Textures[0] = GetMaterialTexture(first, 0);
        HComponentRenderConstants constants = GetRenderConstants(first);
        if (constants)
        {
            EnableRenderObjectConstants(&ro, constants);
        }
        assert(first->m_Resource->m_DDF->m_BlendMode == dmGameSystemDDF::SpriteDesc::BLEND_MODE_ALPHA);
        ro.m_SetBlendFactors = 1;
        ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
        ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        dmRender::AddToRender(context, &ro);
        return true;
    }
}

struct QuadOptions
{
    uint32_t m_Count, m_Frames, m_Samples, m_Warmup, m_Mode, m_Moving, m_Animated, m_Batches, m_Passes;
};
static QuadOptions g_Options = { 10000, 60, 7, 30, 0, 1, 1, 1, 1 };
static const char* MODE_NAMES[] = { "expanded", "matrix", "compact" };

struct QuadTimes
{
    uint64_t m_Update, m_Submit, m_Draw, m_Total, m_Pack, m_Upload;
};

class SpriteQuadBenchmark : public SpriteTest
{
    public:
    dmArray<dmGameObject::HInstance> m_Instances;
    dmGameSystem::SpriteWorld*       m_World;

    void SetUp() override
    {
        m_projectOptions.m_MaxInstances = g_Options.m_Count + 16;
        SpriteTest::SetUp();
        dmGameObject::DeleteCollection(m_Collection);
        m_SpriteContext.m_MaxSpriteCount = g_Options.m_Count;
        m_SpriteContext.m_Subpixels = true;
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, m_projectOptions.m_MaxInstances, 0);
        ASSERT_NE((dmGameObject::HCollection)0, m_Collection);
        m_World = (dmGameSystem::SpriteWorld*)dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
        ASSERT_NE((dmGameSystem::SpriteWorld*)0, m_World);
    }

    void Frame(uint32_t frame, QuadTimes* times)
    {
        using namespace dmGameSystem;
        // Input mutation is outside timing; transform propagation is inside.
        if (g_Options.m_Moving)
        {
            for (uint32_t i = 0; i < m_Instances.Size(); ++i)
            {
                dmGameObject::SetPosition(m_Instances[i], Point3((float)(i % 100) + frame * 0.01f, (float)(i / 100), (float)(i % g_Options.m_Batches)));
            }
        }
        dmRender::ClearRenderObjects(m_RenderContext);
        dmGraphics::ResetDrawCount();
        g_SubmittedIndices = 0;
        g_QuadPackTime = g_QuadUploadTime = 0;
        HProfile profile = ProfileFrameBegin();
        uint64_t begin = dmTime::GetMonotonicTime();
        bool updated = dmGameObject::Update(m_Collection, &m_UpdateContext);
        bool post_updated = dmGameObject::PostUpdate(m_Collection);
        uint64_t update = dmTime::GetMonotonicTime();
        dmRender::RenderListBegin(m_RenderContext);
        dmGameObject::Render(m_Collection);
        dmRender::RenderListEnd(m_RenderContext);
        uint64_t submit = dmTime::GetMonotonicTime();
        for (uint32_t pass = 0; pass < g_Options.m_Passes; ++pass)
        {
            ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_RenderContext, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT));
        }
        uint64_t draw = dmTime::GetMonotonicTime();
        ProfileFrameEnd(profile);
        ASSERT_TRUE(updated);
        ASSERT_TRUE(post_updated);
        ASSERT_EQ(g_Options.m_Count, ((dmRender::RenderContext*)m_RenderContext)->m_RenderListSortBuffer.Size());
        ASSERT_EQ((uint64_t)g_Options.m_Batches * g_Options.m_Passes, dmGraphics::GetDrawCount());
        ASSERT_EQ((uint64_t)g_Options.m_Count * 6 * g_Options.m_Passes, g_SubmittedIndices);
        times->m_Update = update - begin;
        times->m_Submit = submit - update;
        times->m_Draw = draw - submit;
        times->m_Total = draw - begin;
        times->m_Pack = g_QuadPackTime;
        times->m_Upload = g_QuadUploadTime;
    }

    void Validate()
    {
        using namespace dmGameSystem;
        if (!g_QuadMode)
        {
            return;
        }
        dmRender::RenderContext* context = (dmRender::RenderContext*)m_RenderContext;
        const uint32_t count = g_Options.m_Count;
        ASSERT_EQ(count * InstanceStride(), g_QuadData.Size());
        ASSERT_EQ(g_Options.m_Passes, ((dmRender::BufferedRenderBuffer*)g_QuadBuffer)->m_Buffers.Size());
        for (uint32_t pass = 0; pass < g_Options.m_Passes; ++pass)
        {
            dmGraphics::VertexBuffer* buffer = (dmGraphics::VertexBuffer*)((dmRender::BufferedRenderBuffer*)g_QuadBuffer)->m_Buffers[pass];
            ASSERT_EQ(g_QuadData.Size(), dmGraphics::GetVertexBufferSize((dmGraphics::HVertexBuffer)buffer));
            ASSERT_EQ(0, memcmp(buffer->m_Buffer, g_QuadData.Begin(), g_QuadData.Size()));
        }
        ASSERT_EQ(g_Options.m_Batches, m_World->m_RenderObjectsInUse);
        uint32_t instance_offset = 0;
        for (uint32_t batch = 0; batch < m_World->m_RenderObjectsInUse; ++batch)
        {
            const dmRender::RenderObject& ro = *m_World->m_RenderObjects[batch];
            ASSERT_EQ(6u, ro.m_VertexCount);
            ASSERT_EQ(0u, ro.m_VertexStart);
            ASSERT_GT(ro.m_InstanceCount, 0u);
            ASSERT_EQ(instance_offset * InstanceStride(), ro.m_VertexBufferOffsets[1]);
            ASSERT_EQ(g_QuadVertices, ro.m_VertexBuffers[0]);
            ASSERT_EQ(g_QuadIndices, ro.m_IndexBuffer);
            ASSERT_EQ(dmRender::GetBuffer(m_RenderContext, g_QuadBuffer), ro.m_VertexBuffers[1]);
            instance_offset += ro.m_InstanceCount;
        }
        ASSERT_EQ(count, instance_offset);
        ASSERT_EQ(0, memcmp(((dmGraphics::VertexBuffer*)g_QuadVertices)->m_Buffer, QUAD_CORNERS, sizeof(QUAD_CORNERS)));
        ASSERT_EQ(0, memcmp(((dmGraphics::IndexBuffer*)g_QuadIndices)->m_Buffer, QUAD_INDICES, sizeof(QUAD_INDICES)));
        // Generate the oracle with the actual expanded sprite writer, then
        // restore the world's staging state. Validation is never timed.
        uint8_t* saved_vertices = m_World->m_VertexBufferData;
        uint32_t saved_written = m_World->m_VerticesWritten;
        bool saved_16bit = m_World->m_Is16BitIndex;
        m_World->m_VertexBufferData = (uint8_t*)malloc(count * 96);
        m_World->m_VerticesWritten = 0;
        m_World->m_Is16BitIndex = count <= 16384;
        uint8_t* indices = (uint8_t*)malloc(count * 24);
        uint8_t* vertex_out = m_World->m_VertexBufferData;
        uint8_t* index_out = indices;
        SpriteComponent* first = &m_World->m_Components.Get((uint32_t)context->m_RenderList[context->m_RenderListSortBuffer[0]].m_UserData);
        dmGraphics::VertexAttributeInfos infos;
        dmRender::HMaterial material = GetComponentMaterial(first);
        FillMaterialAttributeInfos(material, dmRender::GetVertexDeclaration(material), &infos);
        ASSERT_EQ(24u, infos.m_VertexStride);
        CreateVertexData(m_World, &infos, &vertex_out, &index_out, context->m_RenderList.Begin(), context->m_RenderListSortBuffer.Begin(), context->m_RenderListSortBuffer.End());
        for (uint32_t i = 0; i < count; ++i)
        {
            float instance[26];
            memcpy(instance, g_QuadData.Begin() + i * InstanceStride(), InstanceStride());
            for (uint32_t v = 0; v < 4; ++v)
            {
                const float* expected = (float*)(m_World->m_VertexBufferData + (i * 4 + v) * 24);
                const float s = QUAD_CORNERS[v][0], t = QUAD_CORNERS[v][1];
                for (uint32_t c = 0; c < 3; ++c)
                {
                    float actual = g_QuadMode == 1 ? instance[12 + c] + s * instance[c] + t * instance[4 + c] : instance[6 + c] + s * instance[c] + t * instance[3 + c];
                    ASSERT_NEAR(expected[c], actual, 0.001f);
                }
                for (uint32_t c = 0; c < 2; ++c)
                {
                    float actual = g_QuadMode == 1 ? instance[22 + c] + s * instance[16 + c] + t * instance[19 + c] : instance[13 + c] + s * instance[9 + c] + t * instance[11 + c];
                    ASSERT_NEAR(expected[4 + c], actual, 0.00001f);
                }
            }
            ASSERT_EQ(0.0f, instance[g_QuadMode == 1 ? 25 : 15]);
        }
        free(indices);
        free(m_World->m_VertexBufferData);
        m_World->m_VertexBufferData = saved_vertices;
        m_World->m_VerticesWritten = saved_written;
        m_World->m_Is16BitIndex = saved_16bit;
    }

    void Report(const char* state, uint32_t sample, uint32_t frames, const QuadTimes& times)
    {
        using namespace dmGameSystem;
        uint32_t vb = g_QuadMode ? g_QuadData.Size() : (uint32_t)(m_World->m_VertexBufferWritePtr - m_World->m_VertexBufferData);
        uint32_t ib = g_QuadMode ? 0 : (uint32_t)(m_World->m_IndexBufferWritePtr - m_World->m_IndexBufferData);
        ASSERT_EQ(g_Options.m_Count * (g_QuadMode ? InstanceStride() : 96), vb);
        ASSERT_EQ(g_QuadMode ? 0u : g_Options.m_Count * 6 * (g_Options.m_Count <= 16384 ? 2 : 4), ib);
        printf("SPRITE_QUAD,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%llu,%u,%u\n",
               MODE_NAMES[g_QuadMode], state, g_Options.m_Count, g_Options.m_Moving, g_Options.m_Animated, g_Options.m_Batches, g_Options.m_Passes, sample, frames, g_Options.m_Warmup,
               (double)times.m_Update / frames, (double)times.m_Submit / frames, (double)times.m_Draw / frames, (double)times.m_Total / frames,
               (double)times.m_Pack / frames, (double)times.m_Upload / frames, (unsigned long long)dmGraphics::GetDrawCount(), vb * g_Options.m_Passes, ib * g_Options.m_Passes);
    }
};

// Compare actual quad expansion with two instanced prototypes, verifying geometry,
// UVs, sorted instance order, upload sizes and multi-dispatch buffer ownership.
TEST_F(SpriteQuadBenchmark, Run)
{
    using namespace dmGameSystem;
    g_QuadMode = 0;
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    m_Instances.SetCapacity(g_Options.m_Count);
    for (uint32_t i = 0; i < g_Options.m_Count; ++i)
    {
        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/benchmark_quad.goc", dmHashBuffer64(&i, sizeof(i)), 0,
                                           Point3((float)(i % 100), (float)(i / 100), (float)(i % g_Options.m_Batches)),
                                           Quat::rotationX(0.3f) * Quat::rotationZ(0.7f), Vector3(i & 1 ? -1.25f : 1.25f, 2.5f, 1));
        ASSERT_NE((dmGameObject::HInstance)0, go);
        m_Instances.Push(go);
        uint32_t type;
        dmGameObject::HComponent component;
        dmGameObject::HComponentWorld world;
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go, dmHashString64("sprite"), &type, &component, &world));
        SpriteComponent* sprite = (SpriteComponent*)component;
        sprite->m_FlipHorizontal = i & 1;
        sprite->m_FlipVertical = (i >> 1) & 1;
        ASSERT_TRUE(PlayAnimation(sprite, dmHashString64("anim_loop"), (i % 4) * 0.25f, g_Options.m_Animated ? 12.0f : 0.0f));
    }
    // Give every frame a noncentral pivot. Same immutable fixture in all modes.
    SpriteComponent& sprite = m_World->m_Components.GetRawObjects()[0];
    dmGameSystemDDF::TextureSet* texture_set = GetFirstTextureSet(&sprite)->m_TextureSet;
    for (uint32_t i = 0; i < texture_set->m_Geometries.m_Count; ++i)
    {
        texture_set->m_Geometries[i].m_PivotX = 0.2f;
        texture_set->m_Geometries[i].m_PivotY = -0.3f;
    }
    // Force bounded, deterministic batch fragmentation, without adding unrelated
    // uniform mutation costs. Depth groups have distinct compatibility keys.
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    for (uint32_t i = 0; i < m_World->m_Components.GetRawObjects().Size(); ++i)
    {
        SpriteComponent& component = m_World->m_Components.GetRawObjects()[i];
        dmRender::SetBatchKey(m_RenderContext, component.m_RenderItem, 1 + i % g_Options.m_Batches);
    }

    g_QuadMode = g_Options.m_Mode;
    dmGraphics::HProgram program = 0;
    dmRender::HMaterial material = 0;
    if (g_QuadMode)
    {
        TestShaderDesc shader("void main() {}");
        shader.AddInput("corner", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
        const char* names[] = { "quad_world", "quad_uv", "quad_page", "quad_d" };
        dmGraphics::VertexAttribute attributes[4] = {};
        float defaults[16] = {};
        uint32_t location = 1;
        uint32_t attribute_count = g_QuadMode == 1 ? 3 : 4;
        for (uint32_t i = 0; i < attribute_count; ++i)
        {
            dmGraphics::ShaderDesc::ShaderDataType shader_type = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
            dmGraphics::VertexAttribute::VectorType vector_type = dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4;
            if (g_QuadMode == 1)
            {
                shader_type = i == 0 ? dmGraphics::ShaderDesc::SHADER_TYPE_MAT4 : i == 1 ? dmGraphics::ShaderDesc::SHADER_TYPE_MAT3 : dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
                vector_type = i == 0 ? dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4 : i == 1 ? dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3 : dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR;
            }
            shader.AddInput(names[i], location, shader_type);
            location += g_QuadMode == 1 ? (i == 0 ? 4 : i == 1 ? 3 : 1) : 1;
            attributes[i].m_NameHash = dmHashString64(names[i]);
            attributes[i].m_VectorType = vector_type;
            attributes[i].m_DataType = dmGraphics::VertexAttribute::TYPE_FLOAT;
            attributes[i].m_StepFunction = dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
            attributes[i].m_Values.m_BinaryValues.m_Data = (uint8_t*)defaults;
            attributes[i].m_Values.m_BinaryValues.m_Count = dmGraphics::VectorTypeToElementCount(vector_type) * sizeof(float);
        }
        program = dmGraphics::NewProgram(m_GraphicsContext, shader.Get(), 0, 0);
        material = dmRender::NewMaterial(m_RenderContext, program);
        dmRender::SetMaterialVertexSpace(material, dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL);
        dmRender::SetMaterialProgramAttributes(material, attributes, attribute_count);
        ((dmRender::RenderContext*)m_RenderContext)->m_Material = material;
        g_QuadBuffer = dmRender::NewBufferedRenderBuffer(m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
        g_QuadVertices = dmGraphics::NewVertexBuffer(m_GraphicsContext, sizeof(QUAD_CORNERS), QUAD_CORNERS, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        g_QuadIndices = dmGraphics::NewIndexBuffer(m_GraphicsContext, sizeof(QUAD_INDICES), QUAD_INDICES, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    }
    // Force separate buffer backing per pass as on explicit graphics adapters.
    ((dmRender::RenderContext*)m_RenderContext)->m_MultiBufferingRequired = true;
    QuadTimes cold = {};
    Frame(0, &cold);
    Report("cold", 0, 1, cold);
    Validate();
    for (uint32_t i = 0; i < g_Options.m_Warmup; ++i)
    {
        QuadTimes ignored;
        Frame(i + 1, &ignored);
    }
    for (uint32_t sample = 0; sample < g_Options.m_Samples; ++sample)
    {
        QuadTimes total = {};
        for (uint32_t i = 0; i < g_Options.m_Frames; ++i)
        {
            QuadTimes times;
            Frame(1 + g_Options.m_Warmup + sample * g_Options.m_Frames + i, &times);
            total.m_Update += times.m_Update;
            total.m_Submit += times.m_Submit;
            total.m_Draw += times.m_Draw;
            total.m_Total += times.m_Total;
            total.m_Pack += times.m_Pack;
            total.m_Upload += times.m_Upload;
        }
        Report("warm", sample, g_Options.m_Frames, total);
    }
    Validate();
    ((dmRender::RenderContext*)m_RenderContext)->m_Material = 0;
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    if (g_QuadMode)
    {
        dmRender::DeleteBufferedRenderBuffer(m_RenderContext, g_QuadBuffer);
        dmGraphics::DeleteVertexBuffer(g_QuadVertices);
        dmGraphics::DeleteIndexBuffer(g_QuadIndices);
        dmRender::DeleteMaterial(m_RenderContext, material);
        dmGraphics::DeleteProgram(m_GraphicsContext, program);
        g_QuadData.SetSize(0);
        g_QuadData.SetCapacity(0);
    }
    g_QuadMode = 0;
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    uint32_t* values[] = { &g_Options.m_Count, &g_Options.m_Frames, &g_Options.m_Samples, &g_Options.m_Warmup, &g_Options.m_Mode, &g_Options.m_Moving, &g_Options.m_Animated, &g_Options.m_Batches, &g_Options.m_Passes };
    const char* names[] = { "--instances=", "--frames=", "--samples=", "--warmup=", "--mode=", "--moving=", "--animated=", "--batches=", "--passes=" };
    int remaining = 1;
    for (int i = 1; i < argc; ++i)
    {
        bool matched = false;
        for (uint32_t j = 0; j < DM_ARRAY_SIZE(names); ++j)
        {
            size_t length = strlen(names[j]);
            if (strncmp(argv[i], names[j], length) != 0)
            {
                continue;
            }
            errno = 0;
            char* end;
            unsigned long value = strtoul(argv[i] + length, &end, 10);
            if (errno || *end || argv[i][length] <'0' || argv[i][length]> '9' || value > 1000000)
            {
                return 1;
            }
            *values[j] = (uint32_t)value;
            matched = true;
            break;
        }
        if (!matched)
        {
            argv[remaining++] = argv[i];
        }
    }
    if (!g_Options.m_Count || !g_Options.m_Frames || !g_Options.m_Samples || !g_Options.m_Batches || !g_Options.m_Passes || g_Options.m_Mode > 2 || g_Options.m_Moving > 1 || g_Options.m_Animated > 1 || g_Options.m_Batches > g_Options.m_Count)
    {
        return 1;
    }
    argc = remaining;
    dmExportedSymbols();
    for (uint32_t i = 0; i < dmGraphics::GetRegisteredAdaptersCount(); ++i)
    {
        dmGraphics::GraphicsAdapter* adapter = dmGraphics::GetRegisteredAdapter(i);
        if (adapter->m_Family == dmGraphics::ADAPTER_FAMILY_NULL)
        {
            g_NullRegister = adapter->m_RegisterCb;
            adapter->m_RegisterCb = RegisterQuadBenchmarkAdapter;
        }
    }
    TestMainPlatformInit();
    dmDDF::RegisterAllTypes();
    ProfileInitialize();
    printf("marker,mode,state,instances,moving,animated,batches,passes,sample,frames,warmup,update_us,submit_us,draw_us,cpu_frame_us,pack_us,null_upload_us,draw_calls,vertex_bytes,index_bytes\n");
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    ProfileFinalize();
    return result;
}
