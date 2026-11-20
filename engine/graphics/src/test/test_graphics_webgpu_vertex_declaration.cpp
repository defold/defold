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


#include "../webgpu/graphics_webgpu_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static WGPURenderPipeline CaptureVertexPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* desc);
static void               CountVertexDeclarationResolution();

#define wgpuDeviceCreateRenderPipeline CaptureVertexPipeline
#define DM_TEST_WEBGPU_VERTEX_DECLARATION_RESOLVED CountVertexDeclarationResolution
#include "../webgpu/graphics_webgpu.cpp"
#undef DM_TEST_WEBGPU_VERTEX_DECLARATION_RESOLVED
#undef wgpuDeviceCreateRenderPipeline

using namespace dmGraphics;

struct WebGPUVertexDeclarationTest : jc_test_base_class
{
    WebGPUVertexDeclarationTest() : m_Buffer(WGPUBufferUsage_Vertex)
    {
    }

    WebGPUProgram          m_Program;
    WebGPUProgram          m_OtherProgram;
    WebGPUShaderModule     m_Vertex;
    WebGPUShaderModule     m_Fragment;
    WebGPURenderTarget     m_Target;
    WebGPUTexture          m_Texture;
    WebGPUBuffer           m_Buffer;
    WGPUVertexAttribute    m_Attributes[MAX_VERTEX_BUFFERS][16];
    WGPUVertexBufferLayout m_Layouts[MAX_VERTEX_BUFFERS];
    WebGPUContext*         m_Context;
    HVertexDeclaration     m_Declaration;
    uint32_t               m_LayoutCount;
    uint32_t               m_PipelineCount;
    uint32_t               m_ResolutionCount;

    // Callbacks use the active fixture pointer; all state belongs to the test.
    static WebGPUVertexDeclarationTest* s_Active;

    void SetUp()
    {
        s_Active = this;
        m_Context = new WebGPUContext();
        g_WebGPUContext = m_Context;
        m_Context->m_RenderPipelineCache.SetCapacity(32);
        m_Context->m_CurrentPipelineState = GetDefaultPipelineState();
        m_Context->m_CurrentProgram = &m_Program;
        m_Context->m_CurrentRenderPass.m_Target = &m_Target;
        m_Program.m_VertexModule = &m_Vertex;
        m_Program.m_FragmentModule = &m_Fragment;
        m_Target.m_Base.m_Id = DM_RENDERTARGET_BACKBUFFER_ID;
        m_Target.m_Base.m_ColorAttachmentCount = 1;
        m_Target.m_Multisample = 1;
        m_Texture.m_Format = WGPUTextureFormat_RGBA8Unorm;
        m_Target.m_TextureColor[0] = StoreAssetInContainer(m_Context->m_BaseContext.m_AssetHandleContainer, &m_Texture, ASSET_TYPE_TEXTURE);
        SetInputs(m_Program, 0);
        SetInputs(m_OtherProgram, 4);
        HVertexStreamDeclaration streams = NewVertexStreamDeclaration((HContext)m_Context);
        AddVertexStream(streams, "position", 3, TYPE_FLOAT, false);
        AddVertexStream(streams, "color", 4, TYPE_UNSIGNED_BYTE, true);
        AddVertexStream(streams, "unused", 2, TYPE_FLOAT, false);
        m_Declaration = WebGPUNewVertexDeclaration((HContext)m_Context, streams);
        DeleteVertexStreamDeclaration(streams);
        m_LayoutCount = 0;
        m_PipelineCount = 0;
        m_ResolutionCount = 0;
    }

    void SetInputs(WebGPUProgram& program, uint32_t first_location)
    {
        program.m_BaseProgram.m_ShaderMeta.m_Inputs.EnsureSize(3);
        ShaderResourceBinding* inputs = program.m_BaseProgram.m_ShaderMeta.m_Inputs.Begin();
        memset(inputs, 0, sizeof(*inputs) * 3);
        inputs[0].m_NameHash = dmHashString64("color");
        inputs[0].m_Binding = first_location + 1;
        inputs[0].m_StageFlags = SHADER_STAGE_FLAG_VERTEX;
        inputs[1].m_NameHash = dmHashString64("position");
        inputs[1].m_Binding = first_location;
        inputs[1].m_StageFlags = SHADER_STAGE_FLAG_VERTEX;
        inputs[2].m_NameHash = dmHashString64("unused");
        inputs[2].m_Binding = first_location + 2;
        inputs[2].m_StageFlags = SHADER_STAGE_FLAG_FRAGMENT;
    }

    VertexDeclaration* Bind(HVertexDeclaration declaration, uint32_t slot, uint32_t offset, WebGPUProgram& program)
    {
        WebGPUEnableVertexBuffer((HContext)m_Context, (HVertexBuffer) & m_Buffer, slot);
        WebGPUEnableVertexDeclaration((HContext)m_Context, declaration, slot, offset, (HProgram) & program);
        return m_Context->m_CurrentVertexDeclaration[slot];
    }

    void TearDown()
    {
        DeleteVertexDeclaration(m_Declaration);
        delete m_Context;
        g_WebGPUContext = NULL;
        s_Active = NULL;
    }

};

WebGPUVertexDeclarationTest* WebGPUVertexDeclarationTest::s_Active = NULL;

static void CountVertexDeclarationResolution()
{
    ++WebGPUVertexDeclarationTest::s_Active->m_ResolutionCount;
}

static WGPURenderPipeline CaptureVertexPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* desc)
{
    WebGPUVertexDeclarationTest* test = WebGPUVertexDeclarationTest::s_Active;
    ++test->m_PipelineCount;
    test->m_LayoutCount = desc->vertex.bufferCount;
    for (uint32_t i = 0; i < test->m_LayoutCount; ++i)
    {
        test->m_Layouts[i] = desc->vertex.buffers[i];
        assert(test->m_Layouts[i].attributeCount <= DM_ARRAY_SIZE(test->m_Attributes[i]));
        memcpy(test->m_Attributes[i], test->m_Layouts[i].attributes, sizeof(WGPUVertexAttribute) * test->m_Layouts[i].attributeCount);
        test->m_Layouts[i].attributes = test->m_Attributes[i];
    }

    return (WGPURenderPipeline)(uintptr_t)test->m_PipelineCount;
}

// Resolve by name, retain declaration order and exclude fragment-only/unmatched inputs.
TEST_F(WebGPUVertexDeclarationTest, ResolvesVertexInputs)
{
    VertexDeclaration* resolved = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(2u, resolved->m_StreamCount);
    ASSERT_EQ(0, resolved->m_Streams[0].m_Location);
    ASSERT_EQ(1, resolved->m_Streams[1].m_Location);
    ASSERT_EQ(12u, resolved->m_Streams[1].m_Offset);
    ASSERT_EQ(TYPE_UNSIGNED_BYTE, resolved->m_Streams[1].m_Type);
    ASSERT_TRUE(resolved->m_Streams[1].m_Normalize);
    ASSERT_EQ(m_Declaration->m_Stride, resolved->m_Stride);
}

// Disable/enable and changing base offsets must reuse resolution while refreshing live bindings.
TEST_F(WebGPUVertexDeclarationTest, ReusesLayoutAcrossRebinds)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(1u, m_ResolutionCount);
    ASSERT_EQ(first, Bind(m_Declaration, 0, 0, m_Program));
    ASSERT_EQ(1u, m_ResolutionCount);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    WebGPUDisableVertexDeclaration((HContext)m_Context, m_Declaration);
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[0]);
    ASSERT_EQ(first, Bind(m_Declaration, 0, 64, m_Program));
    ASSERT_EQ(64u, m_Context->m_CurrentVertexBufferOffsets[0]);
    ASSERT_EQ(m_Declaration, m_Context->m_EnabledVertexDeclarations[0]);
    ASSERT_EQ(1u, m_ResolutionCount);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(1u, m_PipelineCount);
}

// Program switches must select the right locations and only resolve again if their entries collide.
TEST_F(WebGPUVertexDeclarationTest, AlternatingPrograms)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    const uint64_t first_hash = first->m_PipelineHash;
    VertexDeclaration* other = Bind(m_Declaration, 0, 0, m_OtherProgram);
    ASSERT_EQ(4, other->m_Streams[0].m_Location);
    ASSERT_NE(first_hash, other->m_PipelineHash);
    ASSERT_EQ(2u, m_ResolutionCount);
    ASSERT_EQ(0, Bind(m_Declaration, 0, 0, m_Program)->m_Streams[0].m_Location);
    ASSERT_EQ(4, Bind(m_Declaration, 0, 0, m_OtherProgram)->m_Streams[0].m_Location);
    ASSERT_EQ(first == other ? 4u : 2u, m_ResolutionCount);
}

// Switching between two occupied slots must reuse both entries, not just the most recently used one.
TEST_F(WebGPUVertexDeclarationTest, ReusesNonCollidingLayouts)
{
    WebGPUVertexDeclarationCache& cache = m_Context->m_VertexDeclarationCache[0];
    VertexDeclaration declarations[DM_ARRAY_SIZE(cache.m_Entries) + 1];
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    HVertexDeclaration other = NULL;
    // Find a different slot through actual binds, without reproducing the address hash.
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(declarations); ++i)
    {
        declarations[i] = *m_Declaration;
        declarations[i].m_Stride += (i + 1) * 4;
        if (Bind(&declarations[i], 0, 0, m_Program) != first)
        {
            other = &declarations[i];
            break;
        }
    }

    ASSERT_NE((HVertexDeclaration)NULL, other);
    // Earlier candidates may have replaced the first slot. Warm both selected layouts.
    first = Bind(m_Declaration, 0, 0, m_Program);
    VertexDeclaration* second = Bind(other, 0, 0, m_Program);
    ASSERT_NE(first, second);
    const uint32_t resolutions = m_ResolutionCount;
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(first, Bind(m_Declaration, 0, i * 16, m_Program));
        ASSERT_EQ(m_Declaration->m_Stride, first->m_Stride);
        ASSERT_EQ(second, Bind(other, 0, i * 32, m_Program));
        ASSERT_EQ(other->m_Stride, second->m_Stride);
        ASSERT_EQ(resolutions, m_ResolutionCount);
    }
}

// More programs than cache slots guarantees a collision; replacements must resolve the correct locations.
TEST_F(WebGPUVertexDeclarationTest, CollidingProgramsResolveOnEachSwitch)
{
    WebGPUVertexDeclarationCache& cache = m_Context->m_VertexDeclarationCache[0];
    WebGPUProgram programs[DM_ARRAY_SIZE(cache.m_Entries) + 1];
    int32_t occupants[DM_ARRAY_SIZE(cache.m_Entries)];
    uint64_t hashes[DM_ARRAY_SIZE(programs)];
    memset(occupants, -1, sizeof(occupants));
    int32_t first = -1;
    uint32_t second = 0;
    uint32_t slot = 0;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(programs); ++i)
    {
        SetInputs(programs[i], i * 2);
        hashes[i] = Bind(m_Declaration, 0, 0, programs[i])->m_PipelineHash;
        slot = cache.m_LastUsed;
        if (occupants[slot] >= 0)
        {
            first = occupants[slot];
            second = i;
            break;
        }

        occupants[slot] = i;
    }

    ASSERT_NE(-1, first);
    ASSERT_NE(hashes[first], hashes[second]);
    uint32_t resolutions = m_ResolutionCount;
    for (uint32_t i = 0; i < 6; ++i)
    {
        uint32_t p = i % 2 == 0 ? first : second;
        VertexDeclaration* resolved = Bind(m_Declaration, 0, i * 16, programs[p]);
        ++resolutions;
        ASSERT_EQ(slot, cache.m_LastUsed);
        ASSERT_EQ(resolutions, m_ResolutionCount);
        ASSERT_EQ(2u, resolved->m_StreamCount);
        ASSERT_EQ(p * 2, resolved->m_Streams[0].m_Location);
        ASSERT_EQ(p * 2 + 1, resolved->m_Streams[1].m_Location);
        ASSERT_EQ(hashes[p], resolved->m_PipelineHash);
        ASSERT_EQ(resolved, Bind(m_Declaration, 0, i * 32, programs[p]));
        ASSERT_EQ(resolutions, m_ResolutionCount);
    }
}

// Identical contents can share a cache entry regardless of declaration addresses.
TEST_F(WebGPUVertexDeclarationTest, MatchesDeclarationContents)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    VertexDeclaration copy = *m_Declaration;
    ASSERT_EQ(first, Bind(&copy, 0, 128, m_Program));
    ASSERT_EQ(&copy, m_Context->m_EnabledVertexDeclarations[0]);
    ASSERT_EQ(128u, m_Context->m_CurrentVertexBufferOffsets[0]);
    ASSERT_EQ(1u, m_ResolutionCount);
}

// SetStreamOffset must invalidate both resolution and the pipeline, despite the unchanged source hash.
TEST_F(WebGPUVertexDeclarationTest, OffsetMutationChangesPipeline)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    const uint64_t first_hash = first->m_PipelineHash;
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(12u, m_Attributes[0][1].offset);
    ASSERT_TRUE(SetStreamOffset(m_Declaration, 1, 16));
    VertexDeclaration* changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(16u, changed->m_Streams[1].m_Offset);
    ASSERT_NE(first_hash, changed->m_PipelineHash);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(2u, m_PipelineCount);
    ASSERT_EQ(16u, m_Attributes[0][1].offset);
    ASSERT_EQ(2u, m_ResolutionCount);
    ASSERT_EQ(changed, Bind(m_Declaration, 0, 0, m_Program));
    ASSERT_EQ(2u, m_ResolutionCount);
}

// Format, normalization, stream names/count, stride and instance rate all affect cached layouts.
TEST_F(WebGPUVertexDeclarationTest, DetectsDeclarationChanges)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    uint64_t hash = first->m_PipelineHash;
    m_Declaration->m_Streams[1].m_Normalize = false;
    VertexDeclaration* changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_FALSE(changed->m_Streams[1].m_Normalize);
    ASSERT_NE(hash, changed->m_PipelineHash);
    m_Declaration->m_Streams[1].m_Type = TYPE_UNSIGNED_SHORT;
    m_Declaration->m_Streams[1].m_Size = 2;
    changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(TYPE_UNSIGNED_SHORT, changed->m_Streams[1].m_Type);
    ASSERT_EQ(2u, changed->m_Streams[1].m_Size);
    m_Declaration->m_Streams[1].m_NameHash = dmHashString64("missing");
    changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(1u, changed->m_StreamCount);
    m_Declaration->m_StreamCount = 0;
    changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(0u, changed->m_StreamCount);
    m_Declaration->m_StreamCount = 3;
    m_Declaration->m_Stride = 48;
    m_Declaration->m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
    changed = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(48u, changed->m_Stride);
    ASSERT_EQ(VERTEX_STEP_FUNCTION_INSTANCE, changed->m_StepFunction);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(48u, m_Layouts[0].arrayStride);
    ASSERT_EQ(WGPUVertexStepMode_Instance, m_Layouts[0].stepMode);
}

// Cache eviction on one binding must not overwrite another binding's active resolved streams.
TEST_F(WebGPUVertexDeclarationTest, EvictionPreservesOtherBindings)
{
    VertexDeclaration* other = Bind(m_Declaration, 1, 32, m_Program);
    for (uint32_t i = 0; i < 20; ++i)
    {
        SetStreamOffset(m_Declaration, 0, i * 4);
        VertexDeclaration* current = Bind(m_Declaration, 0, i * 16, m_Program);
        ASSERT_EQ(i * 4, current->m_Streams[0].m_Offset);
        ASSERT_EQ(other, m_Context->m_CurrentVertexDeclaration[1]);
        ASSERT_EQ(0u, other->m_Streams[0].m_Offset);
        ASSERT_EQ(32u, m_Context->m_CurrentVertexBufferOffsets[1]);
    }

    SetStreamOffset(m_Declaration, 0, 0);
    ASSERT_EQ(0u, Bind(m_Declaration, 0, 0, m_Program)->m_Streams[0].m_Offset);
}

// Cached data must outlive its source declaration, and new declarations must resolve from their values.
TEST_F(WebGPUVertexDeclarationTest, OwnsStreamSnapshots)
{
    VertexDeclaration* first = Bind(m_Declaration, 0, 0, m_Program);
    DeleteVertexDeclaration(m_Declaration);
    m_Declaration = NULL;
    ASSERT_EQ(12u, first->m_Streams[1].m_Offset);
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration((HContext)m_Context);
    AddVertexStream(streams, "color", 4, TYPE_UNSIGNED_BYTE, true);
    m_Declaration = WebGPUNewVertexDeclaration((HContext)m_Context, streams);
    DeleteVertexStreamDeclaration(streams);
    VertexDeclaration* next = Bind(m_Declaration, 0, 0, m_Program);
    ASSERT_EQ(1u, next->m_StreamCount);
    ASSERT_EQ(0u, next->m_Streams[0].m_Offset);
    ASSERT_EQ(1, next->m_Streams[0].m_Location);
}

// Reload and deletion use DestroyProgram; invalidate all of that program's entries before reflection changes.
TEST_F(WebGPUVertexDeclarationTest, ProgramDestructionInvalidatesAllBindings)
{
    Bind(m_Declaration, 0, 0, m_Program);
    Bind(m_Declaration, 1, 0, m_Program);
    VertexDeclaration* other = Bind(m_Declaration, 2, 0, m_OtherProgram);
    ASSERT_EQ(3u, m_ResolutionCount);
    m_Program.m_VertexModule = NULL;
    m_Program.m_FragmentModule = NULL;
    WebGPUDestroyProgram(m_Context, &m_Program);
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[0]);
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[1]);
    ASSERT_EQ(other, m_Context->m_CurrentVertexDeclaration[2]);
    SetInputs(m_Program, 8);
    ASSERT_EQ(8, Bind(m_Declaration, 0, 0, m_Program)->m_Streams[0].m_Location);
    ASSERT_EQ(8, Bind(m_Declaration, 1, 0, m_Program)->m_Streams[0].m_Location);
    ASSERT_EQ(other, Bind(m_Declaration, 2, 0, m_OtherProgram));
    ASSERT_EQ(5u, m_ResolutionCount);
}

// Failed shader selection must invalidate layouts too: the shared reload path already cleared reflection.
TEST_F(WebGPUVertexDeclarationTest, FailedReloadInvalidatesLayouts)
{
    ASSERT_EQ(2u, Bind(m_Declaration, 0, 0, m_Program)->m_StreamCount);
    ASSERT_EQ(1u, m_ResolutionCount);
    DestroyProgram(&m_Program.m_BaseProgram);
    ShaderDesc unavailable = {};
    ASSERT_FALSE(WebGPUReloadProgram((HContext)m_Context, (HProgram) & m_Program, &unavailable, NULL, 0));
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[0]);
    ASSERT_EQ(0u, Bind(m_Declaration, 0, 0, m_Program)->m_StreamCount);
    ASSERT_EQ(2u, m_ResolutionCount);
}

// Empty declarations must be cacheable without reading null stream storage or creating vertex layouts.
TEST_F(WebGPUVertexDeclarationTest, EmptyDeclaration)
{
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration((HContext)m_Context);
    HVertexDeclaration empty = WebGPUNewVertexDeclaration((HContext)m_Context, streams);
    DeleteVertexStreamDeclaration(streams);
    ASSERT_EQ(0u, Bind(empty, 0, 0, m_Program)->m_StreamCount);
    ASSERT_EQ(0u, Bind(empty, 0, 16, m_Program)->m_StreamCount);
    ASSERT_EQ(1u, m_ResolutionCount);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(0u, m_LayoutCount);
    DeleteVertexDeclaration(empty);
}

// Deleting a program must remove cached references before the allocator can reuse its address.
TEST_F(WebGPUVertexDeclarationTest, DeleteProgramClearsCachedReferences)
{
    WebGPUProgram* program = new WebGPUProgram();
    SetInputs(*program, 3);
    Bind(m_Declaration, 0, 0, *program);
    Bind(m_Declaration, 1, 0, *program);
    m_Context->m_CurrentProgram = program;
    WebGPUDeleteProgram((HContext)m_Context, (HProgram)program);
    ASSERT_EQ((WebGPUProgram*)NULL, m_Context->m_CurrentProgram);
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[0]);
    ASSERT_EQ((VertexDeclaration*)NULL, m_Context->m_CurrentVertexDeclaration[1]);
    for (uint32_t i = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        WebGPUVertexDeclarationCache& cache = m_Context->m_VertexDeclarationCache[i];
        for (uint32_t j = 0; j < DM_ARRAY_SIZE(cache.m_Entries); ++j)
        {
            ASSERT_EQ((WebGPUProgram*)NULL, cache.m_Entries[j].m_Program);
        }
    }
}

// The production context teardown must release owned snapshots even though its context is malloc-allocated.
TEST_F(WebGPUVertexDeclarationTest, ContextDestructionReleasesStreamStorage)
{
    for (uint32_t i = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        Bind(m_Declaration, i, 0, m_Program);
    }

    WebGPUDestroyContext(m_Context);
    for (uint32_t i = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        WebGPUVertexDeclarationCache& cache = m_Context->m_VertexDeclarationCache[i];
        for (uint32_t j = 0; j < DM_ARRAY_SIZE(cache.m_Entries); ++j)
        {
            ASSERT_EQ(0u, cache.m_Entries[j].m_SourceStreams.Capacity());
            ASSERT_EQ(0u, cache.m_Entries[j].m_ResolvedStreams.Capacity());
        }
    }
}

// Matrix streams must still expand into consecutive shader locations and column offsets on cache hits.
TEST_F(WebGPUVertexDeclarationTest, MatrixAndInstanceDescriptors)
{
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration((HContext)m_Context, VERTEX_STEP_FUNCTION_INSTANCE);
    AddVertexStream(streams, "position", 9, TYPE_FLOAT, false);
    AddVertexStream(streams, "color", 16, TYPE_FLOAT, false);
    HVertexDeclaration matrix = WebGPUNewVertexDeclaration((HContext)m_Context, streams);
    DeleteVertexStreamDeclaration(streams);
    m_Program.m_BaseProgram.m_ShaderMeta.m_Inputs[0].m_Binding = 3;
    Bind(matrix, 0, 0, m_Program);
    WebGPUDisableVertexDeclaration((HContext)m_Context, matrix);
    Bind(matrix, 0, 256, m_Program);
    ASSERT_EQ(1u, m_ResolutionCount);
    WebGPUGetOrCreateRenderPipeline(m_Context);
    ASSERT_EQ(1u, m_LayoutCount);
    ASSERT_EQ(7u, m_Layouts[0].attributeCount);
    ASSERT_EQ(100u, m_Layouts[0].arrayStride);
    ASSERT_EQ(WGPUVertexStepMode_Instance, m_Layouts[0].stepMode);
    for (uint32_t i = 0; i < 7; ++i)
    {
        ASSERT_EQ(i, m_Attributes[0][i].shaderLocation);
        ASSERT_EQ(i < 3 ? i * 12 : 36 + (i - 3) * 16, m_Attributes[0][i].offset);
        ASSERT_EQ(i < 3 ? WGPUVertexFormat_Float32x3 : WGPUVertexFormat_Float32x4, m_Attributes[0][i].format);
    }

    DeleteVertexDeclaration(matrix);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
