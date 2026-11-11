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

#ifndef DM_TEST_TEXTURE_UPLOAD_POOL_H
#define DM_TEST_TEXTURE_UPLOAD_POOL_H
// Compiled with glslangValidator -V from:
// #version 450
// layout(location=0) in vec2 position;
// layout(location=0) out vec2 uv;
// void main() { gl_Position=vec4(position,0,1); uv=position*0.5+0.5; }
static const uint32_t texture_upload_vertex_spv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000022, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00000012, 0x0000001c,
    0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00060005,
    0x0000000b, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x0000000b, 0x00000000,
    0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006, 0x0000000b, 0x00000001, 0x505f6c67, 0x746e696f,
    0x657a6953, 0x00000000, 0x00070006, 0x0000000b, 0x00000002, 0x435f6c67, 0x4470696c, 0x61747369,
    0x0065636e, 0x00070006, 0x0000000b, 0x00000003, 0x435f6c67, 0x446c6c75, 0x61747369, 0x0065636e,
    0x00030005, 0x0000000d, 0x00000000, 0x00050005, 0x00000012, 0x69736f70, 0x6e6f6974, 0x00000000,
    0x00030005, 0x0000001c, 0x00007675, 0x00050048, 0x0000000b, 0x00000000, 0x0000000b, 0x00000000,
    0x00050048, 0x0000000b, 0x00000001, 0x0000000b, 0x00000001, 0x00050048, 0x0000000b, 0x00000002,
    0x0000000b, 0x00000003, 0x00050048, 0x0000000b, 0x00000003, 0x0000000b, 0x00000004, 0x00030047,
    0x0000000b, 0x00000002, 0x00040047, 0x00000012, 0x0000001e, 0x00000000, 0x00040047, 0x0000001c,
    0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016,
    0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040015, 0x00000008,
    0x00000020, 0x00000000, 0x0004002b, 0x00000008, 0x00000009, 0x00000001, 0x0004001c, 0x0000000a,
    0x00000006, 0x00000009, 0x0006001e, 0x0000000b, 0x00000007, 0x00000006, 0x0000000a, 0x0000000a,
    0x00040020, 0x0000000c, 0x00000003, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000003,
    0x00040015, 0x0000000e, 0x00000020, 0x00000001, 0x0004002b, 0x0000000e, 0x0000000f, 0x00000000,
    0x00040017, 0x00000010, 0x00000006, 0x00000002, 0x00040020, 0x00000011, 0x00000001, 0x00000010,
    0x0004003b, 0x00000011, 0x00000012, 0x00000001, 0x0004002b, 0x00000006, 0x00000014, 0x00000000,
    0x0004002b, 0x00000006, 0x00000015, 0x3f800000, 0x00040020, 0x00000019, 0x00000003, 0x00000007,
    0x00040020, 0x0000001b, 0x00000003, 0x00000010, 0x0004003b, 0x0000001b, 0x0000001c, 0x00000003,
    0x0004002b, 0x00000006, 0x0000001e, 0x3f000000, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x00000010, 0x00000013, 0x00000012, 0x00050051,
    0x00000006, 0x00000016, 0x00000013, 0x00000000, 0x00050051, 0x00000006, 0x00000017, 0x00000013,
    0x00000001, 0x00070050, 0x00000007, 0x00000018, 0x00000016, 0x00000017, 0x00000014, 0x00000015,
    0x00050041, 0x00000019, 0x0000001a, 0x0000000d, 0x0000000f, 0x0003003e, 0x0000001a, 0x00000018,
    0x0004003d, 0x00000010, 0x0000001d, 0x00000012, 0x0005008e, 0x00000010, 0x0000001f, 0x0000001d,
    0x0000001e, 0x00050050, 0x00000010, 0x00000020, 0x0000001e, 0x0000001e, 0x00050081, 0x00000010,
    0x00000021, 0x0000001f, 0x00000020, 0x0003003e, 0x0000001c, 0x00000021, 0x000100fd, 0x00010038,
};
// Compiled with glslangValidator -V from:
// #version 450
// layout(location=0) in vec2 uv;
// layout(location=0) out vec4 color;
// layout(set=0,binding=0) uniform sampler2D tex;
// void main() { color=texture(tex,uv); }
static const uint32_t texture_upload_fragment_spv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000014, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x00000011, 0x00030010,
    0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
    0x00000000, 0x00040005, 0x00000009, 0x6f6c6f63, 0x00000072, 0x00030005, 0x0000000d, 0x00786574,
    0x00030005, 0x00000011, 0x00007675, 0x00040047, 0x00000009, 0x0000001e, 0x00000000, 0x00040047,
    0x0000000d, 0x00000022, 0x00000000, 0x00040047, 0x0000000d, 0x00000021, 0x00000000, 0x00040047,
    0x00000011, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020,
    0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00090019,
    0x0000000a, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x0003001b, 0x0000000b, 0x0000000a, 0x00040020, 0x0000000c, 0x00000000, 0x0000000b, 0x0004003b,
    0x0000000c, 0x0000000d, 0x00000000, 0x00040017, 0x0000000f, 0x00000006, 0x00000002, 0x00040020,
    0x00000010, 0x00000001, 0x0000000f, 0x0004003b, 0x00000010, 0x00000011, 0x00000001, 0x00050036,
    0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x0004003d, 0x0000000b,
    0x0000000e, 0x0000000d, 0x0004003d, 0x0000000f, 0x00000012, 0x00000011, 0x00050057, 0x00000007,
    0x00000013, 0x0000000e, 0x00000012, 0x0003003e, 0x00000009, 0x00000013, 0x000100fd, 0x00010038,
};
// Verifies rendered upload contents, source ownership, fence-protected reuse,
// mixed-size cache eviction, the idle budget, and oversized transient fallback.
struct VulkanTextureUploadPoolTest : ITest
{
    enum IdleCacheExpectation
    {
        IDLE_CACHE_UNCHECKED,
        IDLE_CACHE_ONE_LARGE_BLOCK,
        IDLE_CACHE_THREE_MEDIUM_BLOCKS
    };

    enum
    {
        FRAMES_PER_CASE       = 4,
        WARMUP_FRAMES         = 2,
        UNTOUCHED_PIXEL_VALUE = 17
    };

    struct Case
    {
        const char*          m_Name;
        uint32_t             m_Width;
        uint32_t             m_Height;
        uint32_t             m_Count;
        bool                 m_SubUpdate;
        bool                 m_Zero;
        IdleCacheExpectation m_ExpectedIdleCache;
    };

    dmGraphics::HTexture           m_Textures[3] = {};
    dmGraphics::HProgram           m_Program = 0;
    dmGraphics::HVertexBuffer      m_Vertices = 0;
    dmGraphics::HVertexDeclaration m_Declaration = 0;
    uint8_t*                       m_Data = 0;
    uint32_t                       m_Case = 0;
    uint32_t                       m_Frame = 0;

    void Check(EngineCtx* engine, bool condition, const char* message)
    {
        if (!condition)
        {
            dmLogError("Texture upload pool case %u frame %u: %s", m_Case, m_Frame, message);
            engine->m_Failed = true;
        }
    }

    void DrainUploads(EngineCtx* engine)
    {
        dmGraphics::VulkanContext* vk = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        Check(engine, vkDeviceWaitIdle(vk->m_LogicalDevice.m_Device) == VK_SUCCESS, "failed to wait for uploads");
        dmGraphics::FlushFenceResourcesToDestroy(vk);
    }

    void Initialize(EngineCtx* engine) override
    {
        dmGraphics::HContext context = engine->m_GraphicsContext;
        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
            Check(engine, false, "requires the Vulkan adapter");
            engine->m_Running = 0;
            return;
        }
        m_Data = (uint8_t*) malloc(64 * 1024 * 1024);
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Textures); ++i)
        {
            dmGraphics::TextureCreationParams creation;
            creation.m_Width = 32;
            creation.m_Height = 32;
            m_Textures[i] = dmGraphics::NewTexture(context, creation);
        }
        DrainUploads(engine);

        dmGraphics::ShaderDesc desc = {};
        AddShaderWithType(&desc, dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_SPIRV,
                          (uint8_t*) texture_upload_vertex_spv, sizeof(texture_upload_vertex_spv));
        AddShaderWithType(&desc, dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_SPIRV,
                          (uint8_t*) texture_upload_fragment_spv, sizeof(texture_upload_fragment_spv));
        AddShaderResource(&desc, "position", dmGraphics::ShaderDesc::SHADER_TYPE_VEC2, 0, 0, BINDING_TYPE_INPUT, dmGraphics::SHADER_STAGE_FLAG_VERTEX);
        AddShaderResource(&desc, "tex", dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D, 0, 0, BINDING_TYPE_TEXTURE, dmGraphics::SHADER_STAGE_FLAG_FRAGMENT);
        char error[1024] = {};
        m_Program = dmGraphics::NewProgram(context, &desc, error, sizeof(error));
        DeleteShaderDesc(&desc);
        Check(engine, m_Program != 0, error);
        if (!m_Program)
        {
            engine->m_Running = 0;
            return;
        }
        const float vertices[] = { -1, -1, 3, -1, -1, 3 };
        m_Vertices = dmGraphics::NewVertexBuffer(context, sizeof(vertices), vertices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(context);
        dmGraphics::AddVertexStream(streams, "position", 2, dmGraphics::TYPE_FLOAT, false);
        m_Declaration = dmGraphics::NewVertexDeclaration(context, streams);
        dmGraphics::DeleteVertexStreamDeclaration(streams);
    }

    void Execute(EngineCtx* engine) override
    {
        // Growth, shrinkage, oversized fallback, and revisiting cached sizes. The
        // final cases exercise a real sub-rectangle and a burst exceeding the budget.
        const Case cases[] = {
            // Name, dimensions, upload count, sub-update, zero-fill, idle-cache expectation.
            { "small burst",        256,  256, 3, false, false, IDLE_CACHE_UNCHECKED },
            { "medium burst",      1024, 1024, 3, false, false, IDLE_CACHE_UNCHECKED },
            { "grow to budget",    2048, 2048, 1, false, false, IDLE_CACHE_ONE_LARGE_BLOCK },
            { "shrink to medium",  1024, 1024, 3, false, false, IDLE_CACHE_THREE_MEDIUM_BLOCKS },
            { "reuse for small",    256,  256, 3, false, false, IDLE_CACHE_THREE_MEDIUM_BLOCKS },
            { "oversized fallback", 4096, 2048, 1, false, false, IDLE_CACHE_THREE_MEDIUM_BLOCKS },
            { "reuse after fallback", 1024, 1024, 3, false, false, IDLE_CACHE_THREE_MEDIUM_BLOCKS },
            { "grow again",        2048, 2048, 1, false, false, IDLE_CACHE_ONE_LARGE_BLOCK },
            { "sub-rectangle",     1024, 1024, 3, true,  false, IDLE_CACHE_UNCHECKED },
            { "burst over budget", 2048, 2048, 3, false, false, IDLE_CACHE_UNCHECKED },
            { "null source",        256,  256, 1, false, true,  IDLE_CACHE_UNCHECKED }
        };
        const Case& test = cases[m_Case];
        if (m_Frame == 0)
        {
            dmLogInfo("Texture upload pool: %s", test.m_Name);
        }
        dmGraphics::HContext context = engine->m_GraphicsContext;
        dmGraphics::VulkanContext* vk = (dmGraphics::VulkanContext*) context;
        dmGraphics::TextureUploadPool* pool = &vk->m_TextureUploadPool;
        const uint32_t bytes = test.m_Width * test.m_Height * 4;
        dmGraphics::TextureParams params;
        params.m_Width = test.m_Width;
        params.m_Height = test.m_Height;
        params.m_DataSize = bytes;
        params.m_Data = test.m_Zero ? 0 : m_Data;
        params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        // Seed both halves before the sub-update touches only the left half.
        if (test.m_SubUpdate && m_Frame == 0)
        {
            params.m_Width *= 2;
            params.m_DataSize *= 2;
            memset(m_Data, UNTOUCHED_PIXEL_VALUE, params.m_DataSize);
            for (uint32_t i = 0; i < test.m_Count; ++i)
            {
                dmGraphics::SetTexture(context, m_Textures[i], params);
            }
            DrainUploads(engine);
            params.m_Width /= 2;
            params.m_DataSize /= 2;
        }
        params.m_SubUpdate = test.m_SubUpdate;
        uint8_t expected[3] = {};
        HOpaqueHandle pending[3] = {};
        dmGraphics::TextureUploadBlock* blocks[3] = {};
        bool retainable = true;
        for (uint32_t i = 0; i < test.m_Count; ++i)
        {
            dmGraphics::TextureUploadBlock* reusable = 0;
            for (uint32_t j = 0; j < pool->m_Blocks.Size(); ++j)
            {
                dmGraphics::TextureUploadBlock* block = pool->m_Blocks[j];
                if (!block->m_InUse && block->m_Buffer.m_Base.m_Size >= bytes &&
                    (!reusable || block->m_Buffer.m_Base.m_Size < reusable->m_Buffer.m_Base.m_Size))
                {
                    reusable = block;
                }
            }
            uint64_t idle_before = pool->m_IdleBytes;
            uint32_t block_count_before = pool->m_Blocks.Size();
            expected[i] = test.m_Zero ? 0 : (uint8_t) (32 + (m_Case * 29 + m_Frame * 7 + i * 53) % 192);
            memset(m_Data, expected[i], bytes);
            dmGraphics::SetTexture(context, m_Textures[i], params);
            // The upload must own its source bytes before this call returns.
            memset(m_Data, 0xee, bytes);

            dmGraphics::VulkanTexture* texture = dmGraphics::GetAssetFromContainer<dmGraphics::VulkanTexture>(vk->m_BaseContext.m_AssetHandleContainer, m_Textures[i]);
            pending[i] = texture->m_PendingUpload;
            dmGraphics::FenceResourcesToDestroy* upload = vk->m_FenceResourcesToDestroy.Get(pending[i]);
            Check(engine, upload != 0, "missing pending upload fence");
            if (!upload)
            {
                engine->m_Running = 0;
                return;
            }
            blocks[i] = upload->m_TextureUploadBlock;
            if (bytes > dmGraphics::TEXTURE_UPLOAD_IDLE_BUDGET)
            {
                Check(engine, blocks[i] == 0 && upload->m_ResourcesCount == 2, "oversized upload did not use transient storage");
                Check(engine, pool->m_IdleBytes == idle_before && pool->m_Blocks.Size() == block_count_before, "oversized upload changed the cache");
            }
            else
            {
                Check(engine, blocks[i] && blocks[i]->m_InUse && blocks[i]->m_Buffer.m_MappedDataPtr, "upload is not using a mapped pool block");
                if (!blocks[i])
                {
                    engine->m_Running = 0;
                    return;
                }
                retainable = retainable && blocks[i]->m_AllocationSize <= dmGraphics::TEXTURE_UPLOAD_IDLE_BUDGET;
                if (reusable)
                {
                    Check(engine, blocks[i] == reusable, "failed to reuse the best-fit idle block");
                }
                if (m_Frame >= WARMUP_FRAMES && blocks[i]->m_AllocationSize * test.m_Count <= dmGraphics::TEXTURE_UPLOAD_IDLE_BUDGET)
                {
                    Check(engine, reusable != 0, "cache did not stabilize after warmup");
                }
                for (uint32_t j = 0; j < i; ++j)
                {
                    Check(engine, blocks[i] != blocks[j] && blocks[j]->m_InUse, "reused or evicted an upload before its fence retired");
                }
            }
        }

        RenderAndCheckTextures(engine, test, expected, pending);
        CheckIdleCache(engine, test, bytes, retainable);

        ++m_Frame;
        if (m_Frame == FRAMES_PER_CASE)
        {
            m_Frame = 0;
            ++m_Case;
            if (m_Case == DM_ARRAY_SIZE(cases))
            {
                dmLogInfo("Texture upload pool: rendered contents, fence ownership, reuse, eviction and fallback checks passed");
                engine->m_Running = 0;
            }
        }
        if (engine->m_Failed)
        {
            engine->m_Running = 0;
        }
    }

    void RenderAndCheckTextures(EngineCtx* engine, const Case& test, const uint8_t* expected, const HOpaqueHandle* pending)
    {
        dmGraphics::HContext context = engine->m_GraphicsContext;
        dmGraphics::VulkanContext* vk = (dmGraphics::VulkanContext*) context;
        dmGraphics::Clear(context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1.0f, 0);
        dmGraphics::DisableState(context, dmGraphics::STATE_DEPTH_TEST);
        dmGraphics::DisableState(context, dmGraphics::STATE_CULL_FACE);
        dmGraphics::EnableProgram(context, m_Program);
        dmGraphics::SetSampler(context, GetUniformLocation(m_Program, "tex"), 0);
        dmGraphics::EnableVertexBuffer(context, m_Vertices, 0);
        dmGraphics::EnableVertexDeclaration(context, m_Declaration, 0, 0, m_Program);
        uint32_t cell_width = dmGraphics::GetWindowWidth(context) / test.m_Count;
        for (uint32_t i = 0; i < test.m_Count; ++i)
        {
            dmGraphics::SetViewport(context, i * cell_width, 0, cell_width, dmGraphics::GetWindowHeight(context));
            dmGraphics::EnableTexture(context, 0, 0, m_Textures[i]);
            dmGraphics::Draw(context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 3, 1);
            dmGraphics::DisableTexture(context, 0, m_Textures[i]);
            Check(engine, vk->m_FenceResourcesToDestroy.Get(pending[i]) == 0, "consumed upload fence was not retired");
        }
        dmGraphics::DisableVertexDeclaration(context, m_Declaration);
        dmGraphics::DisableVertexBuffer(context, m_Vertices);
        dmGraphics::DisableProgram(context);
        for (uint32_t i = 0; i < test.m_Count; ++i)
        {
            for (uint32_t half = 0; half < 2; ++half)
            {
                uint8_t pixel[4] = {};
                uint32_t x = i * cell_width + cell_width * (half ? 3 : 1) / 4;
                dmGraphics::ReadPixels(context, x, dmGraphics::GetWindowHeight(context) / 2, 1, 1, pixel, sizeof(pixel));
                uint8_t value = test.m_SubUpdate && half ? UNTOUCHED_PIXEL_VALUE : expected[i];
                Check(engine, pixel[0] == value && pixel[1] == value && pixel[2] == value && pixel[3] == value, "rendered upload or untouched region differs");
            }
        }
    }

    void CheckIdleCache(EngineCtx* engine, const Case& test, uint32_t bytes, bool retainable)
    {
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        dmGraphics::TextureUploadPool* pool = &context->m_TextureUploadPool;
        uint64_t idle_bytes = 0;
        uint64_t idle_capacity = 0;
        for (uint32_t i = 0; i < pool->m_Blocks.Size(); ++i)
        {
            Check(engine, !pool->m_Blocks[i]->m_InUse, "consumed upload is still in use");
            idle_bytes += pool->m_Blocks[i]->m_AllocationSize;
            idle_capacity += pool->m_Blocks[i]->m_Buffer.m_Base.m_Size;
        }
        Check(engine, idle_bytes == pool->m_IdleBytes && idle_bytes <= dmGraphics::TEXTURE_UPLOAD_IDLE_BUDGET, "invalid idle accounting or budget overflow");
        if (test.m_ExpectedIdleCache == IDLE_CACHE_ONE_LARGE_BLOCK && m_Frame >= WARMUP_FRAMES && retainable)
        {
            Check(engine, pool->m_Blocks.Size() == 1 && pool->m_Blocks[0]->m_Buffer.m_Base.m_Size == bytes, "obsolete small blocks prevented cache growth");
        }
        if (test.m_ExpectedIdleCache == IDLE_CACHE_THREE_MEDIUM_BLOCKS && m_Frame >= WARMUP_FRAMES)
        {
            Check(engine, pool->m_Blocks.Size() == 3 && idle_capacity == 12 * 1024 * 1024, "cache did not adapt to smaller bursts or survive fallback");
        }
    }

    void OnGraphicsClosing(EngineCtx* engine) override
    {
        // Leave an unconsumed upload for shutdown to retire through its own fence.
        if (m_Data && m_Textures[0])
        {
            dmGraphics::TextureParams params;
            params.m_Width = params.m_Height = 32;
            params.m_DataSize = 32 * 32 * 4;
            params.m_Data = m_Data;
            dmGraphics::SetTexture(engine->m_GraphicsContext, m_Textures[0], params);
        }
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Textures); ++i)
        {
            if (m_Textures[i])
            {
                dmGraphics::DeleteTexture(engine->m_GraphicsContext, m_Textures[i]);
            }
        }
        if (m_Program)
        {
            dmGraphics::DeleteProgram(engine->m_GraphicsContext, m_Program);
        }
        if (m_Declaration)
        {
            dmGraphics::DeleteVertexDeclaration(m_Declaration);
        }
        if (m_Vertices)
        {
            dmGraphics::DeleteVertexBuffer(m_Vertices);
        }
        free(m_Data);
    }
};

#endif
