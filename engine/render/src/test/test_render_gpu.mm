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

#include <stdio.h>
#if defined(DM_TEST_GPU_VULKAN)
#include <dmsdk/graphics/graphics_vulkan.h>
#include "../../../graphics/src/vulkan/graphics_vulkan_private.h"
#elif defined(DM_TEST_GPU_WEBGPU)
#include <dmsdk/graphics/graphics_webgpu.h>
#else
#include <QuartzCore/QuartzCore.h>
#include <objc/runtime.h>
#include "../../../graphics/src/metal/graphics_metal_private.h"
#endif
#include <dlib/time.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <testmain/testmain.h>
#include <dmsdk/dlib/intersection.h>
#include <dmsdk/dlib/jobsystem.h>
#include <graphics/graphics.h>
#include <platform/window.hpp>
#include <script/script.h>
#include "render/render.h"
#include "render/render_private.h"

using namespace dmVMath;

#if defined(DM_TEST_GPU_VULKAN)
static uint32_t g_FailedBufferLength;
static uint32_t g_FailedBufferAllocations;
static bool     g_FailVulkanMemoryAllocation;
static bool     g_NextVulkanMemoryAllocation;

// The Vulkan device implementation is linked into this test with these two
// native calls redirected. All other calls still use the real Vulkan driver.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL TestVkCreateBuffer(VkDevice device, const VkBufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkBuffer* buffer)
{
    bool fail = info->size == g_FailedBufferLength;
    if (fail && !g_FailVulkanMemoryAllocation)
    {
        ++g_FailedBufferAllocations;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    VkResult result = vkCreateBuffer(device, info, allocator, buffer);
    g_NextVulkanMemoryAllocation = fail && result == VK_SUCCESS;
    return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL TestVkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory)
{
    if (g_NextVulkanMemoryAllocation)
    {
        g_NextVulkanMemoryAllocation = false;
        ++g_FailedBufferAllocations;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return vkAllocateMemory(device, info, allocator, memory);
}
#elif !defined(DM_TEST_GPU_WEBGPU)
static Method     g_BufferAllocationMethod;
static IMP        g_BufferAllocationImplementation;
static NSUInteger g_FailedBufferLength;
static uint32_t   g_FailedBufferAllocations;

static id         FailMetalBufferAllocation(id device, SEL selector, NSUInteger length, NSUInteger options)
{
    if (length == g_FailedBufferLength)
    {
        ++g_FailedBufferAllocations;
        return nil;
    }
    return ((id(*)(id, SEL, NSUInteger, NSUInteger))g_BufferAllocationImplementation)(device, selector, length, options);
}

static void RestoreMetalBufferAllocation()
{
    if (g_BufferAllocationMethod)
        method_setImplementation(g_BufferAllocationMethod, g_BufferAllocationImplementation);
    g_BufferAllocationMethod = 0;
}

static void FailMetalBufferLength(MTL::Device* device, NSUInteger length)
{
    if (!g_BufferAllocationMethod)
    {
        g_BufferAllocationMethod = class_getInstanceMethod(object_getClass((id)device), sel_registerName("newBufferWithLength:options:"));
        g_BufferAllocationImplementation = method_setImplementation(g_BufferAllocationMethod, (IMP)FailMetalBufferAllocation);
        g_FailedBufferAllocations = 0;
    }
    g_FailedBufferLength = length;
}
#endif

static void NoopDispatch(const dmRender::RenderListDispatchParams& params) {}
static void LegacyVisibility(const dmRender::RenderListVisibilityParams& params)
{
    uint32_t* calls = (uint32_t*)params.m_UserData;
    *calls += params.m_NumEntries;
    for (uint32_t i = 0; i < params.m_NumEntries; ++i)
        params.m_Entries[i].m_Visibility = dmRender::VISIBILITY_FULL;
}

struct GpuBatchCapture
{
    dmRender::HRenderItem m_Items[6];
    uint32_t              m_Batches;
    uint32_t              m_GpuItems;
    uint32_t              m_CpuItems;
    uint32_t              m_VisibilityCalls;
    bool                  m_MixedBatch;
};

static void CaptureGpuBatches(const dmRender::RenderListDispatchParams& params)
{
    if (params.m_Operation != dmRender::RENDER_LIST_OPERATION_BATCH)
        return;
    GpuBatchCapture* capture = (GpuBatchCapture*)params.m_UserData;
    bool             gpu = dmRender::IsRenderItemGpuDriven(params.m_Context, capture->m_Items[params.m_Buf[*params.m_Begin].m_UserData]);
    ++capture->m_Batches;
    for (const uint32_t* i = params.m_Begin; i != params.m_End; ++i)
    {
        bool current = dmRender::IsRenderItemGpuDriven(params.m_Context, capture->m_Items[params.m_Buf[*i].m_UserData]);
        capture->m_MixedBatch |= current != gpu;
        if (current)
            ++capture->m_GpuItems;
        else
            ++capture->m_CpuItems;
    }
}

static void CaptureGpuVisibility(const dmRender::RenderListVisibilityParams& params)
{
    GpuBatchCapture* capture = (GpuBatchCapture*)params.m_UserData;
    capture->m_VisibilityCalls += params.m_NumEntries;
    for (uint32_t i = 0; i < params.m_NumEntries; ++i)
        params.m_Entries[i].m_Visibility = dmRender::VISIBILITY_FULL;
}

struct PreparedBatchCapture
{
    uint32_t                      m_Count;
    uint32_t                      m_Batches;
    uint32_t                      m_GpuItems;
    uint64_t                      m_Ids[6];
    dmRender::HRenderListDispatch m_Dispatches[6];
};

static void CapturePreparedBatches(const dmRender::RenderListDispatchParams& params)
{
    PreparedBatchCapture* capture = (PreparedBatchCapture*)params.m_UserData;
    if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BEGIN)
        memset(capture, 0, sizeof(*capture));
    if (params.m_Operation != dmRender::RENDER_LIST_OPERATION_BATCH)
        return;
    ++capture->m_Batches;
    for (const uint32_t* i = params.m_Begin; i != params.m_End; ++i)
    {
        const dmRender::RenderListEntry& entry = params.m_Buf[*i];
        capture->m_Dispatches[capture->m_Count] = entry.m_Dispatch;
        capture->m_Ids[capture->m_Count++] = entry.m_UserData;
        capture->m_GpuItems += entry.m_FrustumHash == dmRender::FRUSTUM_HASH_GPU_CULLING;
    }
}

static void BeginPreparedFrame(dmRender::HRenderContext context, dmRender::HRenderItemDispatch dispatch, dmRender::HRenderItemDispatch first = 0)
{
    dmRender::RenderListBegin(context);
    if (first)
        dmRender::SubmitRenderListDispatch(context, first);
    dmRender::SubmitRenderListDispatch(context, dispatch);
}

#if defined(DM_TEST_GPU_WEBGPU)
static void QueueWorkDone(WGPUQueueWorkDoneStatus status, WGPUStringView message, void* userdata, void*)
{
    *(bool*)userdata = status == WGPUQueueWorkDoneStatus_Success;
}
#endif

struct OrderedPixelBatch
{
    dmRender::RenderObject    m_Object;
    dmGraphics::HVertexBuffer m_Instances;
    bool                      m_Gpu;
};

static void DrawOrderedPixelBatch(const dmRender::RenderListDispatchParams& params)
{
    if (params.m_Operation != dmRender::RENDER_LIST_OPERATION_BATCH)
        return;
    OrderedPixelBatch* batch = (OrderedPixelBatch*)params.m_UserData;
    batch->m_Gpu = params.m_Buf[*params.m_Begin].m_FrustumHash == dmRender::FRUSTUM_HASH_GPU_CULLING;
    if (batch->m_Gpu)
        return;
    Matrix4  worlds[2];
    uint32_t count = params.m_End - params.m_Begin;
    ASSERT_EQ(2u, count);
    for (uint32_t i = 0; i < count; ++i)
        worlds[i] = params.m_Context->m_PersistentRenderItems[params.m_Begin[i]].m_WorldTransform;
    dmGraphics::SetVertexBufferData(batch->m_Instances, sizeof(worlds), worlds, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    batch->m_Object.m_InstanceCount = count;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(params.m_Context, &batch->m_Object));
}

class GpuIndirectTest : public jc_test_base_class
{
    protected:
    HWindow                  m_Window;
    dmGraphics::HContext     m_Graphics;
    HJobContext              m_Jobs;
    dmScript::HContext       m_Script;
    dmRender::HRenderContext m_Render;

    void                     SetUp() override
    {
#if defined(DM_TEST_GPU_VULKAN)
        ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_VULKAN));
#elif defined(DM_TEST_GPU_WEBGPU)
        ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_WEBGPU));
#else
        ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_METAL));
#endif
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
#if defined(DM_TEST_GPU_VULKAN)
        window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
#elif defined(DM_TEST_GPU_WEBGPU)
        window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_WEBGPU;
#else
        window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
#endif
        window_params.m_Width = 64;
        window_params.m_Height = 64;
        window_params.m_Title = "GPU culling test";
        m_Window = dmPlatform::NewWindow();
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, window_params));
        dmPlatform::ShowWindow(m_Window);
        dmGraphics::ContextParams graphics_params = {};
        JobSystemCreateParams     job_params = {};
        job_params.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&job_params);
        graphics_params.m_JobContext = m_Jobs;
        graphics_params.m_VerifyGraphicsCalls = 1;
        graphics_params.m_Window = m_Window;
        graphics_params.m_Width = 64;
        graphics_params.m_Height = 64;
        m_Graphics = dmGraphics::NewContext(graphics_params);
        ASSERT_NE((dmGraphics::HContext)0, m_Graphics);
        dmScript::ContextParams script_params = {};
        script_params.m_GraphicsContext = m_Graphics;
        m_Script = dmScript::NewContext(script_params);

        FILE* file = fopen(INDIRECT_SHADER_FILE, "rb");
        ASSERT_NE((FILE*)0, file);
        fseek(file, 0, SEEK_END);
        uint32_t size = ftell(file);
        rewind(file);
        uint8_t* bytes = new uint8_t[size];
        ASSERT_EQ(size, fread(bytes, 1, size, file));
        fclose(file);
        dmRender::RenderContextParams render_params;
        render_params.m_ScriptContext = m_Script;
        render_params.m_MaxInstances = 4;
        render_params.m_MaxCharacters = 1;
        render_params.m_MaxBatches = 1;
        ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes, size, &render_params.m_InstanceCullingShaderProgramDesc));
        m_Render = dmRender::NewRenderContext(m_Graphics, render_params);
        dmDDF::FreeMessage(render_params.m_InstanceCullingShaderProgramDesc);
        delete[] bytes;
        ASSERT_TRUE(dmRender::IsGpuIndirectSupported(m_Render));
        ASSERT_EQ(32u, sizeof(dmRender::GpuCullingBounds));
        // Classification-only fixtures also submit entries without a material.
        dmRender::RegisterMaterialTagList(m_Render, 0, 0);
        dmGraphics::BeginFrame(m_Graphics);
        dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
        dmGraphics::SetDepthMask(m_Graphics, true);
        dmGraphics::SetDepthFunc(m_Graphics, dmGraphics::COMPARE_FUNC_LEQUAL);
    }
    void CompleteFrame()
    {
#if defined(DM_TEST_GPU_VULKAN)
        dmGraphics::Flip(m_Graphics);
        ASSERT_EQ(VK_SUCCESS, vkQueueWaitIdle(dmGraphics::VulkanGetGraphicsQueue(m_Graphics)));
#elif defined(DM_TEST_GPU_WEBGPU)
        dmGraphics::Flip(m_Graphics);
        bool                          done = false;
        WGPUQueueWorkDoneCallbackInfo callback = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_WaitAnyOnly;
        callback.callback = QueueWorkDone;
        callback.userdata1 = &done;
        WGPUFuture         future = wgpuQueueOnSubmittedWorkDone(dmGraphics::WebGPUGetQueue(m_Graphics), callback);
        WGPUFutureWaitInfo wait = { future, false };
        ASSERT_EQ(WGPUWaitStatus_Success, wgpuInstanceWaitAny(dmGraphics::WebGPUGetInstance(m_Graphics), 1, &wait, UINT64_MAX));
        ASSERT_TRUE(done);
#else
        dmGraphics::MetalContext* metal = (dmGraphics::MetalContext*)m_Graphics;
        MTL::CommandBuffer*       commands = metal->m_FrameResources[metal->m_CurrentFrameInFlight].m_CommandBuffer;
        commands->retain();
        dmGraphics::Flip(m_Graphics);
        commands->waitUntilCompleted();
        ASSERT_EQ(MTL::CommandBufferStatusCompleted, commands->status());
        commands->release();
#endif
    }

    bool ReadSnapshot(dmGraphics::HAsyncBufferReadback snapshot, void* data)
    {
#if defined(DM_TEST_GPU_WEBGPU)
        // Completion is signaled by the mapping callback. The deadline only
        // prevents a broken callback from hanging the native test runner.
        uint64_t deadline = dmTime::GetMonotonicTime() + 30000000;
        while (!dmGraphics::PollStorageBufferReadback(snapshot, data))
        {
            if (dmTime::GetMonotonicTime() > deadline)
                return false;
            wgpuInstanceProcessEvents(dmGraphics::WebGPUGetInstance(m_Graphics));
        }
        return true;
#else
        return dmGraphics::PollStorageBufferReadback(snapshot, data);
#endif
    }

    bool ReadBuffer(dmGraphics::HContext graphics, dmGraphics::HStorageBuffer buffer, uint32_t offset, uint32_t size, void* data)
    {
        dmGraphics::HAsyncBufferReadback snapshot = dmGraphics::RequestStorageBufferReadback(graphics, buffer, offset, size);
        CompleteFrame();
        bool ready = ReadSnapshot(snapshot, data);
        dmGraphics::DeleteStorageBufferReadback(snapshot);
        dmGraphics::BeginFrame(graphics);
        return ready;
    }

    void TearDown() override
    {
#if defined(DM_TEST_GPU_VULKAN)
        g_FailedBufferLength = 0;
        g_NextVulkanMemoryAllocation = false;
#elif !defined(DM_TEST_GPU_WEBGPU)
        RestoreMetalBufferAllocation();
#endif
        CompleteFrame();
        dmRender::DeleteRenderContext(m_Render, 0);
        dmScript::DeleteContext(m_Script);
        dmGraphics::CloseWindow(m_Graphics);
        dmGraphics::DeleteContext(m_Graphics);
        JobSystemDestroy(m_Jobs);
#if !defined(DM_TEST_GPU_WEBGPU)
        dmPlatform::CloseWindow(m_Window);
#endif
        dmPlatform::DeleteWindow(m_Window);
    }
};

// Verifies tag ranges do not invoke unrelated callbacks and mixed GPU/CPU items
// remain in separate batches across blended, overridden and unculled draws.
// Unrelated unculled draws must preserve cached model callback visibility.
TEST_F(GpuIndirectTest, PredicateRangesAndMixedBatches)
{
    FILE* file = fopen(DEBUG_SHADER_FILE, "rb");
    ASSERT_NE((FILE*)0, file);
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(size, fread(bytes, 1, size, file));
    fclose(file);
    dmGraphics::ShaderDesc* desc = 0;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes, size, dmGraphics::ShaderDesc::m_DDFDescriptor, (void**)&desc));
    delete[] bytes;
    char                 error[512] = {};
    dmGraphics::HProgram material_program = dmGraphics::NewProgram(m_Graphics, desc, error, sizeof(error));
    dmDDF::FreeMessage(desc);
    ASSERT_NE((dmGraphics::HProgram)0, material_program);
    GpuBatchCapture               capture = {};
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, CaptureGpuBatches, CaptureGpuVisibility, &capture);
    dmRender::Material            models;
    dmRender::Material            sprites;
    models.m_RenderContext = sprites.m_RenderContext = m_Render;
    models.m_Program = sprites.m_Program = material_program;
    dmhash_t model_tag = dmHashString64("model");
    dmhash_t sprite_tag = dmHashString64("tile");
    dmRender::SetMaterialTags(&models, 1, &model_tag);
    dmRender::SetMaterialTags(&sprites, 1, &sprite_tag);
    dmRender::HPredicate model_predicate = dmRender::NewPredicate();
    dmRender::HPredicate sprite_predicate = dmRender::NewPredicate();
    dmRender::HPredicate gui_predicate = dmRender::NewPredicate();
    dmRender::AddPredicateTag(model_predicate, model_tag);
    dmRender::AddPredicateTag(sprite_predicate, sprite_tag);
    dmRender::AddPredicateTag(gui_predicate, dmHashString64("gui"));
    dmRender::RenderListBegin(m_Render);
    for (uint32_t i = 0; i < 6; ++i)
    {
        capture.m_Items[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        dmRender::SetMaterial(m_Render, capture.m_Items[i], i < 4 ? &models : &sprites);
        if (i != 3 && i != 4)
            dmRender::SetBoundingSphere(m_Render, capture.m_Items[i], Point3(i == 0 ? 100 : 0, 0, 0), 0.1f);
        dmRender::SetRenderItemGpuDriven(m_Render, capture.m_Items[i], i == 0 || i == 2 || i == 3);
        dmRender::Enable(m_Render, capture.m_Items[i], true);
    }
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, capture.m_VisibilityCalls);
    ASSERT_EQ(4u, capture.m_Batches);
    ASSERT_EQ(2u, capture.m_GpuItems);
    ASSERT_EQ(2u, capture.m_CpuItems);
    ASSERT_FALSE(capture.m_MixedBatch);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, gui_predicate, 0, 0, dmRender::SORT_NONE));
    ASSERT_EQ(0u, capture.m_Batches);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, capture.m_VisibilityCalls);
    ASSERT_EQ(4u, capture.m_Batches);
    ASSERT_EQ(2u, capture.m_GpuItems);
    ASSERT_EQ(2u, capture.m_CpuItems);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_BLEND);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, sprite_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(2u, capture.m_VisibilityCalls);
    ASSERT_EQ(1u, capture.m_Batches);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_EQ(2u, capture.m_CpuItems);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, capture.m_Batches);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_CpuItems);
    ASSERT_FALSE(capture.m_MixedBatch);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_BLEND);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(4u, capture.m_Batches);
    ASSERT_EQ(2u, capture.m_GpuItems);
    ASSERT_EQ(2u, capture.m_CpuItems);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    m_Render->m_Material = &models;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, capture.m_Batches);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_CpuItems);
    m_Render->m_Material = 0;

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, 0, dmRender::SORT_NONE));
    ASSERT_EQ(4u, capture.m_Batches);
    ASSERT_EQ(2u, capture.m_GpuItems);
    ASSERT_EQ(2u, capture.m_CpuItems);
    ASSERT_FALSE(capture.m_MixedBatch);

    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    dmRender::SetBoundingAABB(m_Render, capture.m_Items[2], Vector3(1), Vector3(-1));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(2u, capture.m_Batches);
    ASSERT_EQ(1u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_CpuItems);
    ASSERT_FALSE(capture.m_MixedBatch);

    dmGraphics::HProgram program = m_Render->m_GpuIndirect.m_Program;
    m_Render->m_GpuIndirect.m_Program = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    uint32_t visibility_calls = capture.m_VisibilityCalls;
    capture.m_Batches = capture.m_GpuItems = capture.m_CpuItems = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, gui_predicate, 0, 0, dmRender::SORT_NONE));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(visibility_calls, capture.m_VisibilityCalls);
    ASSERT_EQ(1u, capture.m_Batches);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_CpuItems);
    m_Render->m_GpuIndirect.m_Program = program;
    for (uint32_t i = 0; i < 6; ++i)
        dmRender::DeleteRenderItem(m_Render, capture.m_Items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
    dmRender::DeletePredicate(model_predicate);
    dmRender::DeletePredicate(sprite_predicate);
    dmRender::DeletePredicate(gui_predicate);
    dmGraphics::DeleteProgram(m_Graphics, material_program);
}

// Verifies reused opaque candidate batches across camera/transform changes and
// rebuilds for dispatch order, enable/delete/reuse, sort keys and GPU eligibility.
TEST_F(GpuIndirectTest, PersistentCandidateBatches)
{
    PreparedBatchCapture          capture = {};
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, CapturePreparedBatches, 0, &capture);
    dmRender::HRenderItemDispatch other = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         items[6];
    for (uint32_t i = 0; i < 6; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        dmRender::SetOrder(m_Render, items[i], dmRender::RENDER_ORDER_BEFORE_WORLD, 0);
        dmRender::SetBatchKey(m_Render, items[i], i / 2);
        dmRender::SetBoundingSphere(m_Render, items[i], Point3(i == 0 ? 100 : 0, 0, 0), 0.1f);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_EQ(6u, capture.m_Count);
    ASSERT_EQ(6u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_Batches);
    dmRender::GpuRenderListCache* cache = m_Render->m_GpuIndirect.m_DrawCaches[0];
    ASSERT_TRUE(cache->m_Prepared);
    uint64_t generation = cache->m_Generation;

    BeginPreparedFrame(m_Render, dispatch);
    options.m_Matrix = Matrix4::orthographic(-2, 2, -2, 2, -1, 1);
    dmRender::SetWorldTransform(m_Render, items[1], Matrix4::translation(Vector3(2, 0, 0)));
    dmRender::SetBoundingSphere(m_Render, items[2], Point3(1, 0, 0), 0.2f);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_EQ(generation, cache->m_Generation);
    ASSERT_EQ(6u, capture.m_Count);
    ASSERT_EQ(6u, capture.m_GpuItems);
    ASSERT_EQ(3u, capture.m_Batches);
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ((uint64_t)i, capture.m_Ids[i]);

    BeginPreparedFrame(m_Render, dispatch, other);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_GT(cache->m_Generation, generation);
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ(1u, capture.m_Dispatches[i]);

    dmRender::Enable(m_Render, items[1], false);
    dmRender::DeleteRenderItem(m_Render, items[2]);
    items[2] = dmRender::NewRenderItem(m_Render, dispatch, 42);
    dmRender::SetOrder(m_Render, items[2], dmRender::RENDER_ORDER_BEFORE_WORLD, 0);
    dmRender::SetBatchKey(m_Render, items[2], 3);
    dmRender::SetBoundingSphere(m_Render, items[2], Point3(0), 0.1f);
    dmRender::SetRenderItemGpuDriven(m_Render, items[2], true);
    dmRender::Enable(m_Render, items[2], true);
    dmRender::SetBatchKey(m_Render, items[0], 2);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_EQ(5u, capture.m_Count);
    ASSERT_EQ(3u, capture.m_Batches);
    const uint64_t expected[] = { 3, 0, 4, 5, 42 };
    for (uint32_t i = 0; i < 5; ++i)
        ASSERT_EQ(expected[i], capture.m_Ids[i]);
    generation = cache->m_Generation;

    dmRender::SetOrder(m_Render, items[0], dmRender::RENDER_ORDER_BEFORE_WORLD, 1);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_GT(cache->m_Generation, generation);
    ASSERT_EQ(0u, capture.m_Ids[4]);
    dmRender::SetOrder(m_Render, items[0], dmRender::RENDER_ORDER_WORLD, 0);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_FALSE(cache->m_Eligible);
    ASSERT_EQ(5u, capture.m_Count);

    dmRender::SetOrder(m_Render, items[0], dmRender::RENDER_ORDER_BEFORE_WORLD, 0);
    dmRender::SetBoundingAABB(m_Render, items[3], Vector3(1), Vector3(-1));
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_FALSE(cache->m_Eligible);
    ASSERT_EQ(4u, capture.m_GpuItems);
    dmRender::SetBoundingSphere(m_Render, items[3], Point3(0), 0.1f);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, 0, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_TRUE(cache->m_Eligible);
    ASSERT_EQ(5u, capture.m_GpuItems);

    dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_BLEND);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_EQ(4u, capture.m_Count);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_FALSE(cache->m_Prepared);
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_BLEND);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_BACK_TO_FRONT));
    ASSERT_EQ(5u, capture.m_Count);
    ASSERT_EQ(5u, capture.m_GpuItems);

    for (uint32_t i = 0; i < 6; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, other);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies predicate changes, material overrides and legacy entries cannot reuse
// an opaque GPU candidate cache with different membership or CPU visibility.
TEST_F(GpuIndirectTest, CachedPredicatesAndFallback)
{
    FILE* file = fopen(DEBUG_SHADER_FILE, "rb");
    ASSERT_NE((FILE*)0, file);
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(size, fread(bytes, 1, size, file));
    fclose(file);
    dmGraphics::ShaderDesc* desc = 0;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes, size, dmGraphics::ShaderDesc::m_DDFDescriptor, (void**)&desc));
    delete[] bytes;
    char                 error[512] = {};
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Graphics, desc, error, sizeof(error));
    dmDDF::FreeMessage(desc);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    dmRender::Material models;
    dmRender::Material others;
    models.m_RenderContext = others.m_RenderContext = m_Render;
    models.m_Program = others.m_Program = program;
    dmhash_t model_tag = dmHashString64("model");
    dmhash_t other_tag = dmHashString64("other");
    dmRender::SetMaterialTags(&models, 1, &model_tag);
    dmRender::SetMaterialTags(&others, 1, &other_tag);
    dmRender::HPredicate model_predicate = dmRender::NewPredicate();
    dmRender::HPredicate other_predicate = dmRender::NewPredicate();
    dmRender::AddPredicateTag(model_predicate, model_tag);
    dmRender::AddPredicateTag(other_predicate, other_tag);
    PreparedBatchCapture          capture = {};
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, CapturePreparedBatches, 0, &capture);
    dmRender::HRenderItem         items[6];
    for (uint32_t i = 0; i < 6; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        dmRender::SetMaterial(m_Render, items[i], i < 4 ? &models : &others);
        dmRender::SetOrder(m_Render, items[i], dmRender::RENDER_ORDER_BEFORE_WORLD, 0);
        dmRender::SetBoundingSphere(m_Render, items[i], Point3(i == 0 ? 100 : 0, 0, 0), 0.1f);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(4u, capture.m_GpuItems);
    dmRender::GpuRenderListCache* cache = m_Render->m_GpuIndirect.m_DrawCaches[0];
    uint64_t                      generation = cache->m_Generation;

    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, other_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(2u, capture.m_GpuItems);
    ASSERT_EQ(4u, capture.m_Ids[0]);
    ASSERT_EQ(5u, capture.m_Ids[1]);
    ASSERT_GT(cache->m_Generation, generation);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(4u, capture.m_GpuItems);
    dmRender::SetMaterial(m_Render, items[3], &others);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(3u, capture.m_GpuItems);

    dmRender::HRenderItemDispatch unrelated = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         unrelated_item = dmRender::NewRenderItem(m_Render, unrelated, 0);
    dmRender::SetMaterial(m_Render, unrelated_item, &others);
    dmRender::Enable(m_Render, unrelated_item, true);
    BeginPreparedFrame(m_Render, dispatch, unrelated);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    generation = cache->m_Generation;
    dmRender::SetBatchKey(m_Render, unrelated_item, 99);
    BeginPreparedFrame(m_Render, dispatch, unrelated);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(generation, cache->m_Generation);
    ASSERT_EQ(3u, capture.m_GpuItems);

    BeginPreparedFrame(m_Render, dispatch);
    m_Render->m_Material = &models;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(2u, capture.m_Count);
    ASSERT_EQ(0u, capture.m_GpuItems);
    ASSERT_FALSE(cache->m_Prepared);
    m_Render->m_Material = 0;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(3u, capture.m_Count);
    ASSERT_EQ(3u, capture.m_GpuItems);
    generation = cache->m_Generation;
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(generation, cache->m_Generation);

    BeginPreparedFrame(m_Render, dispatch);
    dmRender::HRenderListDispatch legacy = dmRender::RenderListMakeDispatch(m_Render, CapturePreparedBatches, &capture);
    dmRender::RenderListEntry*    entry = dmRender::RenderListAlloc(m_Render, 1);
    memset(entry, 0, sizeof(*entry));
    entry->m_Dispatch = legacy;
    entry->m_UserData = 99;
    entry->m_TagListKey = dmRender::GetMaterialTagListKey(&models);
    entry->m_MajorOrder = dmRender::RENDER_ORDER_BEFORE_WORLD;
    dmRender::RenderListSubmit(m_Render, entry, entry + 1);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_FALSE(cache->m_Eligible);
    ASSERT_EQ(4u, capture.m_Count);
    ASSERT_EQ(3u, capture.m_GpuItems);
    BeginPreparedFrame(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, model_predicate, 0, &options, dmRender::SORT_NONE));
    ASSERT_TRUE(cache->m_Eligible);
    ASSERT_EQ(3u, capture.m_Count);
    ASSERT_EQ(3u, capture.m_GpuItems);

    for (uint32_t i = 0; i < 6; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::DeleteRenderItem(m_Render, unrelated_item);
    dmRender::UnregisterRenderListDispatch(m_Render, unrelated);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
    dmRender::DeletePredicate(model_predicate);
    dmRender::DeletePredicate(other_predicate);
    dmGraphics::DeleteProgram(m_Graphics, program);
}

// Verifies queued instance draws get independent GPU buffers and clearing the
// draw list reuses those buffers with the newly submitted instance membership.
TEST_F(GpuIndirectTest, RendererOwnedBatchBuffers)
{
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem items[2];
    uint32_t slots[2];
    Matrix4 worlds[2] = { Matrix4::identity(), Matrix4::translation(Vector3(0.25f, 0, 0)) };
    dmRender::RenderObject objects[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, 0);
        slots[i] = (uint32_t)items[i];
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.1f), Vector3(0.1f));
        dmRender::SetWorldTransform(m_Render, items[i], worlds[i]);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
        objects[i].m_VertexCount = 3;
        objects[i].m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &objects[i], slots + i, 1, 64, 0, -1));
    }
    ASSERT_EQ(2u, m_Render->m_GpuIndirect.m_Batches.Size());
    dmRender::HGpuRenderBatch batches[2] = { m_Render->m_GpuIndirect.m_RenderBatches[0], m_Render->m_GpuIndirect.m_RenderBatches[1] };
    ASSERT_NE(batches[0], batches[1]);
    ASSERT_NE(batches[0]->m_InstanceBuffer, batches[1]->m_InstanceBuffer);
    ASSERT_NE(batches[0]->m_ArgumentBuffer, batches[1]->m_ArgumentBuffer);
    dmGraphics::HStorageBuffer outputs[2] = { batches[0]->m_InstanceBuffer, batches[1]->m_InstanceBuffer };
    for (uint32_t i = 0; i < 2; ++i)
    {
        Matrix4 actual;
        ASSERT_TRUE(ReadBuffer(m_Graphics, outputs[i], 0, sizeof(actual), &actual));
        ASSERT_EQ(0, memcmp(&actual, &worlds[i], sizeof(actual)));
    }
    dmRender::ClearRenderObjects(m_Render);
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &objects[i], slots + 1 - i, 1, 64, 0, -1));
        ASSERT_EQ(batches[i], m_Render->m_GpuIndirect.m_RenderBatches[i]);
        ASSERT_EQ(outputs[i], batches[i]->m_InstanceBuffer);
        Matrix4 actual;
        ASSERT_TRUE(ReadBuffer(m_Graphics, outputs[i], 0, sizeof(actual), &actual));
        ASSERT_EQ(0, memcmp(&actual, &worlds[1 - i], sizeof(actual)));
    }
    ASSERT_EQ(2u, m_Render->m_GpuIndirect.m_Batches.Size());
    for (uint32_t i = 0; i < 2; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies compacted mixed bounds against CPU visibility and checks every matrix;
// guards against omitted workgroups, stale instance data and duplicate instances.
TEST_F(GpuIndirectTest, CompactionAndInstanceMatrices)
{
    m_Render->m_GpuIndirect.m_StatisticsEnabled = 1;
    const uint32_t                count = 389;
    dmRender::HRenderItem         items[count];
    uint32_t                      slots[count];
    Matrix4                       worlds[count];
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::RenderListBegin(m_Render);
    for (uint32_t i = 0; i < count; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        slots[i] = (uint32_t)items[i];
        Vector3 center((i % 17) * 0.31f - 2.5f, ((i / 17) % 17) * 0.27f - 2.1f, (i % 7) * 0.43f - 1.3f);
        worlds[i] = Matrix4::rotationZ(i * 0.17f) * Matrix4::scale(Vector3(1.5f, -0.4f, 0.8f));
        worlds[i].setTranslation(center);
        worlds[i].setCol(1, worlds[i].getCol(1) + worlds[i].getCol(0) * 0.3f);
        dmRender::SetMaterial(m_Render, items[i], 0);
        if (i % 3 == 0)
            dmRender::SetBoundingSphere(m_Render, items[i], Point3(center), 0.5f);
        else if (i % 3 == 1)
            dmRender::SetBoundingAABB(m_Render, items[i], center - Vector3(0.2f), center + Vector3(0.2f));
        else
            dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.2f), Vector3(0.2f));
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        ASSERT_TRUE(dmRender::SetWorldTransform(m_Render, items[i], worlds[i]));
        ASSERT_TRUE(dmRender::Enable(m_Render, items[i], true));
    }
    // Exact plane contact remains visible; point bounds are valid volumes.
    dmRender::SetBoundingSphere(m_Render, items[0], Point3(1.5f, 0, 0), 0.5f);
    dmRender::SetBoundingSphere(m_Render, items[1], Point3(1.5001f, 0, 0), 0.5f);
    dmRender::SetBoundingAABB(m_Render, items[2], Vector3(1, 0, 0), Vector3(2, 0, 0));
    dmRender::SetBoundingAABB(m_Render, items[3], Vector3(1.0001f, 0, 0), Vector3(2, 0, 0));
    dmRender::SetBoundingOBB(m_Render, items[4], Vector3(-0.5f), Vector3(0.5f));
    worlds[4] = Matrix4::translation(Vector3(1.5f, 0, 0));
    dmRender::SetWorldTransform(m_Render, items[4], worlds[4]);
    dmRender::SetBoundingOBB(m_Render, items[5], Vector3(-0.5f), Vector3(0.5f));
    worlds[5] = Matrix4::translation(Vector3(1.5001f, 0, 0));
    dmRender::SetWorldTransform(m_Render, items[5], worlds[5]);
    dmRender::SetBoundingAABB(m_Render, items[6], Vector3(0), Vector3(0));
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 36;
    object.m_VertexStart = 12;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
    dmRender::FrustumOptions options;
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    const Matrix4 matrices[] = {
        Matrix4::orthographic(-1, 1, -1, 1, -1, 1),
        Matrix4::perspective(1.2f, 1.5f, 0.1f, 5.0f) * Matrix4::lookAt(Point3(1, 2, 4), Point3(0, 0, 0), Vector3(0, 1, 0)),
        Matrix4::orthographic(100, 110, 100, 110, 100, 110),
        Matrix4::orthographic(-10, 10, -10, 10, -10, 10)
    };
    for (uint32_t phase = 0; phase < 10; ++phase)
    {
        options.m_Matrix = matrices[phase % 4];
        options.m_NumPlanes = phase % 2 ? dmRender::FRUSTUM_PLANES_SIDES : dmRender::FRUSTUM_PLANES_ALL;
        dmRender::FrustumOptions* frustum_options = phase == 8 ? 0 : &options;
        Matrix4                   view = Matrix4::rotationY(phase * 0.25f);
        dmRender::SetViewMatrix(m_Render, view);
        if (phase == 4)
        {
            ASSERT_TRUE(dmRender::Enable(m_Render, items[10], false));
            ASSERT_TRUE(dmRender::DeleteRenderItem(m_Render, items[11]));
            items[11] = dmRender::NewRenderItem(m_Render, dispatch, 11);
            ASSERT_EQ(slots[11], (uint32_t)items[11]);
            worlds[11] = Matrix4::translation(Vector3(0, 0, 0));
            dmRender::SetMaterial(m_Render, items[11], 0);
            dmRender::SetBoundingSphere(m_Render, items[11], Point3(0, 0, 0), 0.5f);
            dmRender::SetRenderItemGpuDriven(m_Render, items[11], true);
            ASSERT_TRUE(dmRender::SetWorldTransform(m_Render, items[11], worlds[11]));
            ASSERT_TRUE(dmRender::Enable(m_Render, items[11], true));
        }
        dmRender::ClearRenderObjects(m_Render);
        dmGraphics::HProgram program = m_Render->m_GpuIndirect.m_Program;
        m_Render->m_GpuIndirect.m_Program = 0;
        for (uint32_t i = 0; i < count; ++i)
            m_Render->m_RenderList[slots[i]].m_FrustumHash = dmRender::FRUSTUM_HASH_UNINITIALIZED;
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, frustum_options, dmRender::SORT_NONE));
        bool     expected[count];
        uint32_t expected_count = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            expected[i] = (phase < 4 || i != 10) && m_Render->m_RenderList[slots[i]].m_Visibility == dmRender::VISIBILITY_FULL;
            expected_count += expected[i];
        }
        m_Render->m_GpuIndirect.m_Program = program;
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, frustum_options, dmRender::SORT_NONE));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, count, 128, 0, 64));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        uint32_t arguments[5];
        ASSERT_FALSE(dmGraphics::PollStorageBufferReadback(batch->m_Readback, arguments));
        ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
        uint32_t snapshot[5];
        ASSERT_TRUE(ReadSnapshot(batch->m_Readback, snapshot));
        ASSERT_EQ(36u, arguments[0]);
        ASSERT_EQ(expected_count, arguments[1]);
        ASSERT_EQ(6u, arguments[2]);
        Matrix4 instances[count * 2];
        ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_InstanceBuffer, 0, sizeof(instances), instances));
        bool seen[count] = {};
        for (uint32_t j = 0; j < expected_count; ++j)
        {
            uint32_t match = count;
            for (uint32_t i = 0; i < count; ++i)
                if (memcmp(&worlds[i], &instances[j * 2], sizeof(Matrix4)) == 0)
                {
                    match = i;
                    break;
                }
            ASSERT_NE(count, match);
            ASSERT_TRUE(expected[match]);
            ASSERT_FALSE(seen[match]);
            seen[match] = true;
            Matrix4 normal = transpose(inverse(view * worlds[match]));
            for (uint32_t c = 0; c < 4; ++c)
                for (uint32_t r = 0; r < 4; ++r)
                    ASSERT_NEAR(normal.getElem(c, r), instances[j * 2 + 1].getElem(c, r), 0.0001f);
        }
        dmRender::ClearRenderObjects(m_Render);
        dmGraphics::HStorageBuffer instances_buffer = batch->m_InstanceBuffer;
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, frustum_options, dmRender::SORT_NONE));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, count, 128, 0, 64));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        ASSERT_EQ(instances_buffer, batch->m_InstanceBuffer);
        ASSERT_EQ(0u, m_Render->m_GpuCulling.m_DirtySlots.Size());
        ASSERT_EQ(0u, m_Render->m_GpuIndirect.m_DirtyWorldSlots.Size());
    }
    // Changing only the view still refreshes normal matrices, and changing the
    // indexed subrange cannot reuse stale indirect arguments.
    dmRender::ClearRenderObjects(m_Render);
    Matrix4 view = Matrix4::rotationY(2.0f);
    dmRender::SetViewMatrix(m_Render, view);
    object.m_VertexStart = 20;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, count, 128, 0, 64));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    uint32_t arguments[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
    ASSERT_EQ(10u, arguments[2]);
    ASSERT_NE(0u, arguments[1]);
    Matrix4 instance[2];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_InstanceBuffer, 0, sizeof(instance), instance));
    Matrix4 normal = transpose(inverse(view * instance[0]));
    for (uint32_t c = 0; c < 4; ++c)
        for (uint32_t r = 0; r < 4; ++r)
            ASSERT_NEAR(normal.getElem(c, r), instance[1].getElem(c, r), 0.0001f);
    dmRender::ClearRenderObjects(m_Render);
    dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_BLEND);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_FALSE(dmRender::IsRenderItemGpuDriven(m_Render, items[0]));
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_BLEND);
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_TRUE(dmRender::DeleteRenderItem(m_Render, items[i]));
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies rendering needs no visibility readback by default, and opt-in
// statistics catch up after a pending copy, without copying cached counts again.
TEST_F(GpuIndirectTest, OptionalVisibilityStatistics)
{
    ASSERT_FALSE(m_Render->m_GpuIndirect.m_StatisticsEnabled);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItem item = dmRender::NewRenderItem(m_Render, dispatch, 0);
    dmRender::SetBoundingOBB(m_Render, item, Vector3(-0.5f), Vector3(0.5f));
    dmRender::SetRenderItemGpuDriven(m_Render, item, true);
    dmRender::Enable(m_Render, item, true);
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, 0, dmRender::SORT_NONE));
    uint32_t                  slot = (uint32_t)item;
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 36;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ((dmGraphics::HAsyncBufferReadback)0, batch->m_Readback);
    uint32_t arguments[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
    ASSERT_EQ(1u, arguments[1]);

    m_Render->m_GpuIndirect.m_StatisticsEnabled = 1;
    dmRender::SetWorldTransform(m_Render, item, Matrix4::translation(Vector3(1, 0, 0)));
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_NE((dmGraphics::HAsyncBufferReadback)0, batch->m_Readback);
    ASSERT_FALSE(dmGraphics::PollStorageBufferReadback(batch->m_Readback, arguments));
    dmGraphics::HAsyncBufferReadback first = batch->m_Readback;
    dmRender::Enable(m_Render, item, false);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ(first, batch->m_Readback);
    ASSERT_TRUE(batch->m_StatisticsDirty);
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
    ASSERT_EQ(0u, arguments[1]);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_NE((dmGraphics::HAsyncBufferReadback)0, batch->m_Readback);
    ASSERT_FALSE(batch->m_StatisticsDirty);
    ASSERT_EQ(1u, batch->m_StatisticsCandidates);
    ASSERT_EQ(1u, batch->m_StatisticsVisible);
    CompleteFrame();
    dmGraphics::BeginFrame(m_Graphics);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ((dmGraphics::HAsyncBufferReadback)0, batch->m_Readback);
    ASSERT_EQ(0u, batch->m_StatisticsVisible);
    dmRender::DeleteRenderItem(m_Render, item);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Checks that asynchronous readback captures the requested GPU ordering point,
// rather than reading a source buffer subsequently overwritten by another draw.
TEST_F(GpuIndirectTest, AsynchronousSnapshot)
{
    uint32_t                         before[] = { 36, 7, 0, 0, 0 };
    uint32_t                         after[] = { 36, 0, 0, 0, 0 };
    uint32_t                         result[5];
    dmGraphics::HStorageBuffer       buffer = dmGraphics::NewStorageBuffer(m_Graphics, sizeof(before), before, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::HAsyncBufferReadback snapshot = dmGraphics::RequestStorageBufferReadback(m_Graphics, buffer, 0, sizeof(before));
    ASSERT_FALSE(dmGraphics::PollStorageBufferReadback(snapshot, result));
    dmGraphics::SetStorageBufferSubData(m_Graphics, buffer, 0, sizeof(after), after);
    ASSERT_TRUE(ReadBuffer(m_Graphics, buffer, 0, sizeof(result), result));
    ASSERT_EQ(0u, result[1]);
    ASSERT_TRUE(ReadSnapshot(snapshot, result));
    ASSERT_EQ(7u, result[1]);
    dmGraphics::DeleteStorageBufferReadback(snapshot);
    dmGraphics::DeleteStorageBuffer(m_Graphics, buffer);
}

// Verifies snapshots survive source replacement and frame-slot reuse without an
// explicit GPU wait between frames, including deletion of a pending mapping.
TEST_F(GpuIndirectTest, ReadbackLifetimeAndFrameReuse)
{
    uint32_t                         before[] = { 36, 7, 2, 0, 0 };
    uint32_t                         after[] = { 12, 0, 1, 0, 0 };
    uint32_t                         result[5];
    dmGraphics::HStorageBuffer       buffer = dmGraphics::NewStorageBuffer(m_Graphics, sizeof(before), before, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::HAsyncBufferReadback snapshot = dmGraphics::RequestStorageBufferReadback(m_Graphics, buffer, 0, sizeof(before));
    dmGraphics::HAsyncBufferReadback discarded = dmGraphics::RequestStorageBufferReadback(m_Graphics, buffer, 0, sizeof(before));
    dmGraphics::DeleteStorageBufferReadback(discarded);
    dmGraphics::SetStorageBufferData(m_Graphics, buffer, sizeof(after), after, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    dmGraphics::HAsyncBufferReadback mapped = dmGraphics::RequestStorageBufferReadback(m_Graphics, buffer, 0, sizeof(after));
    dmGraphics::Flip(m_Graphics);
    dmGraphics::BeginFrame(m_Graphics);
    dmGraphics::PollStorageBufferReadback(mapped, result);
    dmGraphics::DeleteStorageBufferReadback(mapped);
    dmGraphics::HAsyncBufferReadback frames[5];
    for (uint32_t frame = 0; frame < DM_ARRAY_SIZE(frames); ++frame)
    {
        uint32_t values[10] = { 12, frame, 1, 0, 0 };
        if (frame == 2)
            dmGraphics::SetStorageBufferData(m_Graphics, buffer, sizeof(values), values, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        else
            dmGraphics::SetStorageBufferSubData(m_Graphics, buffer, 0, sizeof(after), values);
        frames[frame] = dmGraphics::RequestStorageBufferReadback(m_Graphics, buffer, 0, sizeof(after));
        dmGraphics::Flip(m_Graphics);
        dmGraphics::BeginFrame(m_Graphics);
    }
    CompleteFrame();
    dmGraphics::BeginFrame(m_Graphics);
    for (uint32_t frame = 0; frame < DM_ARRAY_SIZE(frames); ++frame)
    {
        ASSERT_TRUE(ReadSnapshot(frames[frame], result));
        ASSERT_EQ(frame, result[1]);
        dmGraphics::DeleteStorageBufferReadback(frames[frame]);
    }
    ASSERT_TRUE(ReadSnapshot(snapshot, result));
    ASSERT_EQ(0, memcmp(before, result, sizeof(before)));
    ASSERT_TRUE(ReadBuffer(m_Graphics, buffer, 0, sizeof(result), result));
    after[1] = DM_ARRAY_SIZE(frames) - 1;
    ASSERT_EQ(0, memcmp(after, result, sizeof(after)));
    dmGraphics::DeleteStorageBufferReadback(snapshot);
    dmGraphics::DeleteStorageBuffer(m_Graphics, buffer);
}

#if defined(DM_TEST_GPU_VULKAN)
// Native buffer and memory allocation failures must reach CPU culling without
// losing the old batch storage or earlier GPU work. No memory is exhausted.
TEST_F(GpuIndirectTest, NativeAllocationFailureFallback)
{
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         items[2];
    uint32_t                      slots[2];
    Matrix4                       world = Matrix4::translation(Vector3(0.25f, 0, 0));
    for (uint32_t i = 0; i < 2; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, 0);
        slots[i] = (uint32_t)items[i];
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.1f), Vector3(0.1f));
        dmRender::SetWorldTransform(m_Render, items[i], i ? Matrix4::translation(Vector3(100, 0, 0)) : world);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmIntersection::CreateFrustumFromMatrix(Matrix4::identity(), true, 6, m_Render->m_GpuIndirect.m_Frustum);
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    dmGraphics::HStorageBuffer       old_membership = batch->m_SlotBuffer;
    dmGraphics::HAsyncBufferReadback snapshot = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_ArgumentBuffer, 0, 5 * sizeof(uint32_t));
    for (uint32_t failure = 0; failure < 2; ++failure)
    {
        g_FailedBufferLength = 8;
        g_FailedBufferAllocations = 0;
        g_FailVulkanMemoryAllocation = failure != 0;
        uint32_t data[2] = { 1, 2 };
        ASSERT_EQ((dmGraphics::HStorageBuffer)0, dmGraphics::NewStorageBuffer(m_Graphics, sizeof(data), 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW));
        ASSERT_EQ((dmGraphics::HStorageBuffer)0, dmGraphics::NewStorageBuffer(m_Graphics, sizeof(data), data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW));
        dmRender::ClearRenderObjects(m_Render);
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 2, 64, 0, -1));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        ASSERT_EQ(3u, g_FailedBufferAllocations);
        ASSERT_EQ(old_membership, batch->m_SlotBuffer);
        ASSERT_EQ(4u, dmGraphics::GetStorageBufferSize(m_Graphics, old_membership));
        ASSERT_EQ((dmRender::HGpuRenderBatch)0, m_Render->m_GpuIndirect.m_RenderBatches.Back());
        ASSERT_EQ(1u, object.m_InstanceCount);
        dmGraphics::DeviceBuffer* fallback = (dmGraphics::DeviceBuffer*)object.m_VertexBuffers[1];
        ASSERT_EQ(VK_SUCCESS, fallback->MapMemory(dmGraphics::VulkanGetDevice(m_Graphics)));
        ASSERT_EQ(0, memcmp(&world, fallback->m_MappedDataPtr, sizeof(world)));
        fallback->UnmapMemory(dmGraphics::VulkanGetDevice(m_Graphics));
    }
    g_FailedBufferLength = 0;
    uint32_t arguments[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, old_membership, 0, sizeof(uint32_t), arguments));
    ASSERT_EQ(0u, arguments[0]);
    ASSERT_TRUE(ReadSnapshot(snapshot, arguments));
    ASSERT_EQ(1u, arguments[1]);
    dmGraphics::DeleteStorageBufferReadback(snapshot);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 2, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_NE((dmRender::HGpuRenderBatch)0, m_Render->m_GpuIndirect.m_RenderBatches.Back());
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
    ASSERT_EQ(1u, arguments[1]);
    for (uint32_t i = 0; i < 2; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}
#elif !defined(DM_TEST_GPU_WEBGPU)
// A native allocation failure must preserve earlier GPU work and old storage,
// and recover visible model matrices through the ordinary CPU instance buffer.
TEST_F(GpuIndirectTest, NativeAllocationFailureFallback)
{
    dmGraphics::MetalContext*     metal = (dmGraphics::MetalContext*)m_Graphics;
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         items[2];
    uint32_t                      slots[2];
    Matrix4                       world = Matrix4::translation(Vector3(0.25f, 0, 0));
    for (uint32_t i = 0; i < 2; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, 0);
        slots[i] = (uint32_t)items[i];
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.1f), Vector3(0.1f));
        dmRender::SetWorldTransform(m_Render, items[i], i ? Matrix4::translation(Vector3(100, 0, 0)) : world);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmIntersection::CreateFrustumFromMatrix(Matrix4::identity(), true, 6, m_Render->m_GpuIndirect.m_Frustum);
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    dmGraphics::HStorageBuffer       old_membership = batch->m_SlotBuffer;
    dmGraphics::MetalStorageBuffer*  native = (dmGraphics::MetalStorageBuffer*)old_membership;
    MTL::Buffer*                     allocation = native->m_DeviceBuffer.m_Buffer;
    dmGraphics::HAsyncBufferReadback snapshot = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_ArgumentBuffer, 0, 5 * sizeof(uint32_t));

    FailMetalBufferLength(metal->m_Device, 2 * sizeof(uint32_t));
    ASSERT_EQ((dmGraphics::HStorageBuffer)0, dmGraphics::NewStorageBuffer(m_Graphics, 8, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW));
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 2, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ(old_membership, batch->m_SlotBuffer);
    ASSERT_EQ((dmRender::HGpuRenderBatch)0, m_Render->m_GpuIndirect.m_RenderBatches.Back());
    ASSERT_EQ(1u, object.m_InstanceCount);
    dmGraphics::MetalDeviceBuffer* fallback = (dmGraphics::MetalDeviceBuffer*)object.m_VertexBuffers[1];
    ASSERT_EQ(0, memcmp(&world, fallback->m_Buffer->contents(), sizeof(world)));
    uint32_t replacement[2] = { 1, 2 };
    dmGraphics::SetStorageBufferData(m_Graphics, old_membership, sizeof(replacement), replacement, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    ASSERT_EQ(3u, g_FailedBufferAllocations);
    ASSERT_EQ(4u, dmGraphics::GetStorageBufferSize(m_Graphics, old_membership));
    ASSERT_EQ(allocation, native->m_DeviceBuffer.m_Buffer);
    RestoreMetalBufferAllocation();

    uint32_t arguments[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, old_membership, 0, sizeof(uint32_t), arguments));
    ASSERT_EQ(0u, arguments[0]);
    ASSERT_TRUE(ReadSnapshot(snapshot, arguments));
    ASSERT_EQ(1u, arguments[1]);
    dmGraphics::DeleteStorageBufferReadback(snapshot);
    for (uint32_t i = 0; i < 2; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}
#endif

// Verifies the native indexed indirect draw, including firstIndex and zero
// instances, and that a storage buffer can be consumed directly as vertex data.
TEST_F(GpuIndirectTest, IndirectDrawPixels)
{
    FILE* file = fopen(DEBUG_SHADER_FILE, "rb");
    ASSERT_NE((FILE*)0, file);
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(size, fread(bytes, 1, size, file));
    fclose(file);
    dmGraphics::ShaderDesc* desc = 0;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes, size, dmGraphics::ShaderDesc::m_DDFDescriptor, (void**)&desc));
    delete[] bytes;
    char                 error[512] = {};
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Graphics, desc, error, sizeof(error));
    dmDDF::FreeMessage(desc);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    dmGraphics::HUniformLocation location = dmGraphics::INVALID_UNIFORM_LOCATION;
    for (uint32_t i = 0; i < dmGraphics::GetUniformCount(program); ++i)
    {
        dmGraphics::Uniform uniform;
        dmGraphics::GetUniform(program, i, &uniform);
        if (uniform.m_NameHash == dmHashString64("view_proj"))
            location = uniform.m_Location;
    }
    float                      vertices[] = { -0.8f, -0.8f, 0, 1, 1, 0, 0, 1, 0.8f, -0.8f, 0, 1, 1, 0, 0, 1, 0, 0.8f, 0, 1, 1, 0, 0, 1 };
    uint16_t                   indices[] = { 99, 99, 0, 1, 2 };
    dmGraphics::HStorageBuffer vertex_storage = dmGraphics::NewStorageBuffer(m_Graphics, sizeof(vertices), vertices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HVertexBuffer  vertex_buffer = dmGraphics::GetStorageBufferVertexBuffer(m_Graphics, vertex_storage);
    uint32_t                   indices32[] = { 99, 99, 0, 1, 2 };
    dmGraphics::HIndexBuffer   index_buffers[] = {
        dmGraphics::NewIndexBuffer(m_Graphics, sizeof(indices), indices, dmGraphics::BUFFER_USAGE_STATIC_DRAW),
        dmGraphics::NewIndexBuffer(m_Graphics, sizeof(indices32), indices32, dmGraphics::BUFFER_USAGE_STATIC_DRAW)
    };
    dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(m_Graphics);
    dmGraphics::AddVertexStream(streams, "position", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(streams, "color", 4, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::HVertexDeclaration declaration = dmGraphics::NewVertexDeclaration(m_Graphics, streams);
    dmGraphics::DeleteVertexStreamDeclaration(streams);
    dmGraphics::RenderTargetCreationParams target_params = {};
    target_params.m_ColorBufferCreationParams[0].m_Width = target_params.m_ColorBufferParams[0].m_Width = 64;
    target_params.m_ColorBufferCreationParams[0].m_Height = target_params.m_ColorBufferParams[0].m_Height = 64;
    target_params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    target_params.m_ColorBufferCreationParams[0].m_UsageHintBits = dmGraphics::TEXTURE_USAGE_FLAG_SAMPLE;
    target_params.m_ColorBufferStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;
    dmGraphics::HRenderTarget     target = dmGraphics::NewRenderTarget(m_Graphics, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_params);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         item = dmRender::NewRenderItem(m_Render, dispatch, 0);
    dmRender::SetRenderItemGpuDriven(m_Render, item, true);
    dmRender::Enable(m_Render, item, true);
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_VertexStart = 4;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    Matrix4 identity = Matrix4::identity();
    for (uint32_t format = 0; format < 2; ++format)
    {
        object.m_IndexType = format ? dmGraphics::TYPE_UNSIGNED_INT : dmGraphics::TYPE_UNSIGNED_SHORT;
        object.m_VertexStart = format ? 8 : 4;
        for (uint32_t visible = 0; visible < 2; ++visible)
        {
            dmRender::ClearRenderObjects(m_Render);
            dmRender::SetMaterial(m_Render, item, 0);
            dmRender::SetBoundingSphere(m_Render, item, Point3(visible ? 0 : 100, 0, 0), 0.1f);
            ASSERT_TRUE(dmRender::SetWorldTransform(m_Render, item, identity));
            ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
            uint32_t slot = (uint32_t)item;
            ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
            batch = m_Render->m_GpuIndirect.m_Batches[0];
            dmGraphics::RenderTargetBindingParams binding = {};
            dmGraphics::SetRenderTarget(m_Graphics, target, binding);
            dmGraphics::SetViewport(m_Graphics, 0, 0, 64, 64);
            dmGraphics::SetColorMask(m_Graphics, true, true, true, true);
            dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
            dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_CULL_FACE);
            dmGraphics::Clear(m_Graphics, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
            dmGraphics::EnableProgram(m_Graphics, program);
            dmGraphics::SetConstantM4(m_Graphics, &identity, 1, location);
            dmGraphics::EnableVertexBuffer(m_Graphics, vertex_buffer, 0);
            dmGraphics::EnableVertexDeclaration(m_Graphics, declaration, 0, 0, program);
            dmGraphics::DrawElementsIndirect(m_Graphics, dmGraphics::PRIMITIVE_TRIANGLES, object.m_IndexType, index_buffers[format], batch->m_ArgumentBuffer, 0);
            uint8_t pixel[4] = {};
            dmGraphics::ReadPixels(m_Graphics, 32, 32, 1, 1, pixel, sizeof(pixel));
            ASSERT_EQ(visible ? 255 : 0, pixel[2]);
            ASSERT_EQ(0u, pixel[0]);
            ASSERT_EQ(0u, pixel[1]);
            uint32_t arguments[5];
            ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments));
            ASSERT_EQ(visible, arguments[1]);
            ASSERT_EQ(2u, arguments[2]);

            dmGraphics::DisableVertexDeclaration(m_Graphics, declaration);
            dmGraphics::DisableVertexBuffer(m_Graphics, vertex_buffer);
            dmGraphics::DisableProgram(m_Graphics);
            dmGraphics::SetRenderTarget(m_Graphics, 0, binding);
        }
    }
    dmRender::ClearRenderObjects(m_Render);
    dmRender::HMaterial override_material = dmRender::NewMaterial(m_Render, program);
    m_Render->m_Material = override_material;
    dmRender::SetBoundingSphere(m_Render, item, Point3(100, 0, 0), 0.1f);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_FALSE(dmRender::IsRenderItemGpuDriven(m_Render, item));
    ASSERT_EQ(dmRender::VISIBILITY_NONE, m_Render->m_RenderList[(uint32_t)item].m_Visibility);
    m_Render->m_Material = 0;
    dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_TRUE(dmRender::IsRenderItemGpuDriven(m_Render, item));
    dmRender::DeleteMaterial(m_Render, override_material);
    dmRender::DeleteRenderItem(m_Render, item);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
    dmGraphics::DeleteRenderTarget(m_Graphics, target);
    dmGraphics::DeleteVertexDeclaration(declaration);
    dmGraphics::DeleteIndexBuffer(index_buffers[0]);
    dmGraphics::DeleteIndexBuffer(index_buffers[1]);
    dmGraphics::DeleteStorageBuffer(m_Graphics, vertex_storage);
    dmGraphics::DeleteProgram(m_Graphics, program);
}

// Verifies cleared/unset bounds and dispatch activation retain CPU callback behavior,
// while repeated setters, sparse updates and recycled slots invalidate GPU inputs once.
TEST_F(GpuIndirectTest, LifecycleAndFallback)
{
    uint32_t calls = 0;
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, LegacyVisibility, &calls);
    dmRender::HRenderItem         item = dmRender::NewRenderItem(m_Render, dispatch, 0);
    dmRender::SetMaterial(m_Render, item, 0);
    dmRender::Enable(m_Render, item, true);
    dmRender::SetRenderItemGpuDriven(m_Render, item, true);
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, calls);
    ASSERT_FALSE(dmRender::IsRenderItemGpuDriven(m_Render, item));
    dmRender::HPredicate other = dmRender::NewPredicate();
    dmRender::AddPredicateTag(other, dmHashString64("gui"));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, other, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(1u, calls);
    dmRender::DeletePredicate(other);
    dmRender::SetBoundingAABB(m_Render, item, Vector3(0), Vector3(0));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_TRUE(dmRender::IsRenderItemGpuDriven(m_Render, item));
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
    uint32_t slot = (uint32_t)item;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    uint32_t args[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(1u, args[1]);
    uint64_t revision = m_Render->m_GpuIndirect.m_Revision;
    dmRender::SetWorldTransform(m_Render, item, Matrix4::identity());
    dmRender::SetBoundingAABB(m_Render, item, Vector3(0), Vector3(0));
    ASSERT_EQ(revision, m_Render->m_GpuIndirect.m_Revision);
    ASSERT_TRUE(m_Render->m_GpuCulling.m_DirtySlots.Empty());
    ASSERT_TRUE(m_Render->m_GpuIndirect.m_DirtyWorldSlots.Empty());
    // Snapshot the earlier output before a later pass changes the same inputs.
    dmGraphics::HAsyncBufferReadback before = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args));
    dmRender::SetBoundingAABB(m_Render, item, Vector3(100), Vector3(100));
    dmRender::SetBoundingAABB(m_Render, item, Vector3(200), Vector3(200));
    ASSERT_EQ(1u, m_Render->m_GpuCulling.m_DirtySlots.Size());
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(0u, args[1]);
    ASSERT_TRUE(ReadSnapshot(before, args));
    ASSERT_EQ(1u, args[1]);
    dmGraphics::DeleteStorageBufferReadback(before);
    dmRender::ClearRenderObjects(m_Render);
    dmRender::SetBoundingAABB(m_Render, item, Vector3(1), Vector3(-1));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(2u, calls);
    ASSERT_FALSE(dmRender::IsRenderItemGpuDriven(m_Render, item));
    dmRender::RenderListBegin(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_TRUE(m_Render->m_RenderListSortBuffer.Empty());
    ASSERT_EQ(2u, calls);
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::SetBoundingSphere(m_Render, item, Point3(0), 0.5f);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    before = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args));
    dmRender::HRenderItem grown[300];
    uint32_t              slots[300];
    slots[0] = slot;
    grown[0] = item;
    for (uint32_t i = 1; i < 300; ++i)
    {
        grown[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        slots[i] = (uint32_t)grown[i];
        dmRender::SetBoundingSphere(m_Render, grown[i], Point3(0), 0.5f);
        dmRender::SetRenderItemGpuDriven(m_Render, grown[i], true);
        dmRender::Enable(m_Render, grown[i], true);
    }
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, 300, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(300u, args[1]);
    ASSERT_TRUE(ReadSnapshot(before, args));
    ASSERT_EQ(1u, args[1]);
    dmGraphics::DeleteStorageBufferReadback(before);
    for (uint32_t i = 0; i < 300; ++i)
        dmRender::DeleteRenderItem(m_Render, grown[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies span uploads preserve unchanged records between scattered changes and
// update both transformed OBBs and instance matrices without stale visibility.
TEST_F(GpuIndirectTest, ScatteredBoundsAndTransforms)
{
    const uint32_t        count = 389;
    dmRender::HRenderItem items[count];
    uint32_t              slots[count];
    Matrix4               worlds[count];
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, i);
        slots[i] = (uint32_t)items[i];
        worlds[i] = Matrix4::translation(Vector3(i * 0.001f, 0, 0));
        dmRender::SetMaterial(m_Render, items[i], 0);
        dmRender::SetWorldTransform(m_Render, items[i], worlds[i]);
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.01f), Vector3(0.01f));
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
    uint32_t args[5];
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        dmRender::ClearRenderObjects(m_Render);
        if (phase)
        {
            for (uint32_t i = 0; i < count; i += 4)
            {
                worlds[i] = Matrix4::translation(Vector3(100 + i, 0, 0));
                dmRender::SetWorldTransform(m_Render, items[i], worlds[i]);
            }
        }
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, count, 64, 0, -1));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
        uint32_t visible = phase ? count - (count + 3) / 4 : count;
        ASSERT_EQ(visible, args[1]);
        Matrix4 instances[count];
        ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_InstanceBuffer, 0, sizeof(instances), instances));
        bool seen[count] = {};
        for (uint32_t i = 0; i < visible; ++i)
        {
            uint32_t slot = (uint32_t)(instances[i].getTranslation().getX() * 1000.0f + 0.5f);
            ASSERT_LT(slot, count);
            ASSERT_FALSE(seen[slot]);
            ASSERT_TRUE(!phase || slot % 4 != 0);
            ASSERT_EQ(0, memcmp(&worlds[slot], &instances[i], sizeof(Matrix4)));
            seen[slot] = true;
        }
    }
    for (uint32_t i = 0; i < count; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies offset local OBBs reuse matrix uploads under shear and degenerate
// scale, without dirtying or rewriting bounds when only the transform changes.
TEST_F(GpuIndirectTest, LocalBoundsReuseWorldTransforms)
{
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         item = dmRender::NewRenderItem(m_Render, dispatch, 0);
    Vector3                       min(1, -2, -0.5f);
    Vector3                       max(3, 2, 1.5f);
    dmRender::SetMaterial(m_Render, item, 0);
    dmRender::SetBoundingOBB(m_Render, item, min, max);
    dmRender::SetRenderItemGpuDriven(m_Render, item, true);
    dmRender::Enable(m_Render, item, true);
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    dmIntersection::Frustum frustum;
    dmIntersection::CreateFrustumFromMatrix(options.m_Matrix, true, (int)options.m_NumPlanes, frustum);
    Matrix4 worlds[5];
    worlds[0] = Matrix4::identity();
    worlds[1] = Matrix4::translation(Vector3(0.0001f, 0, 0));
    worlds[2] = Matrix4::rotationZ(0.7f) * Matrix4::scale(Vector3(1.5f, -0.4f, 0.8f));
    worlds[2].setCol(1, worlds[2].getCol(1) + worlds[2].getCol(0) * 0.3f);
    worlds[2].setTranslation(-(worlds[2] * Point3(2, 0, 0.5f)).getXYZ());
    worlds[3] = Matrix4::scale(Vector3(0));
    worlds[3].setTranslation(Vector3(0.25f));
    worlds[4] = Matrix4::translation(Vector3(100, 0, 0));
    const bool                expected[] = { true, false, true, true, false };
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
    uint32_t                   slot = (uint32_t)item;
    uint32_t                   args[5];
    dmRender::GpuCullingBounds initial;
    for (uint32_t phase = 0; phase < 5; ++phase)
    {
        dmRender::ClearRenderObjects(m_Render);
        ASSERT_TRUE(dmRender::SetWorldTransform(m_Render, item, worlds[phase]));
        if (phase)
        {
            ASSERT_TRUE(m_Render->m_GpuCulling.m_DirtySlots.Empty());
            ASSERT_EQ(1u, m_Render->m_GpuIndirect.m_DirtyWorldSlots.Size());
        }
        ASSERT_EQ(expected[phase], dmIntersection::TestFrustumOBB(frustum, worlds[phase], min, max));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
        ASSERT_EQ(expected[phase] ? 1u : 0u, args[1]);
        dmRender::GpuCullingBounds bounds;
        uint32_t                   gpu_slot = m_Render->m_PersistentRenderItems[slot].m_GpuSlot;
        ASSERT_TRUE(ReadBuffer(m_Graphics, m_Render->m_GpuCulling.m_BoundsBuffer, gpu_slot * sizeof(bounds), sizeof(bounds), &bounds));
        if (phase == 0)
        {
            initial = bounds;
            ASSERT_EQ((uint32_t)dmRender::BOUNDING_VOLUME_OBB, bounds.m_Type);
            ASSERT_EQ(2.0f, bounds.m_Data[0]);
            ASSERT_EQ(0.0f, bounds.m_Data[1]);
            ASSERT_EQ(0.5f, bounds.m_Data[2]);
            ASSERT_EQ(1.0f, bounds.m_Data[3]);
            ASSERT_EQ(2.0f, bounds.m_Data[4]);
            ASSERT_EQ(1.0f, bounds.m_Data[5]);
        }
        else
            ASSERT_EQ(0, memcmp(&initial, &bounds, sizeof(bounds)));
        if (expected[phase])
        {
            Matrix4 instance;
            ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_InstanceBuffer, 0, sizeof(instance), &instance));
            ASSERT_EQ(0, memcmp(&worlds[phase], &instance, sizeof(instance)));
        }
    }
    // Changing the local shape still updates bounds and invalidates compaction.
    dmRender::ClearRenderObjects(m_Render);
    dmRender::SetBoundingOBB(m_Render, item, Vector3(-100.25f, -0.25f, -0.25f), Vector3(-99.75f, 0.25f, 0.25f));
    ASSERT_EQ(1u, m_Render->m_GpuCulling.m_DirtySlots.Size());
    ASSERT_TRUE(m_Render->m_GpuIndirect.m_DirtyWorldSlots.Empty());
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slot, 1, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(1u, args[1]);
    dmRender::DeleteRenderItem(m_Render, item);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies CPU-only items consume no GPU slots and dense-slot moves refresh
// unchanged batch membership without corrupting earlier submitted GPU work.
TEST_F(GpuIndirectTest, DenseModelSlotsAndRecycling)
{
    const uint32_t        count = 5;
    const uint32_t        gap = 100;
    dmRender::HRenderItem items[count];
    dmRender::HRenderItem cpu[count * gap + 1];
    uint32_t              slots[count];
    Matrix4               worlds[count];
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        for (uint32_t j = 0; j < gap; ++j)
        {
            cpu[i * gap + j] = dmRender::NewRenderItem(m_Render, dispatch, 0);
            dmRender::SetMaterial(m_Render, cpu[i * gap + j], 0);
            dmRender::Enable(m_Render, cpu[i * gap + j], true);
        }
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, 0);
        slots[i] = (uint32_t)items[i];
        worlds[i] = Matrix4::translation(Vector3(i * 0.1f, 0, 0));
        dmRender::SetMaterial(m_Render, items[i], 0);
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.01f), Vector3(0.01f));
        dmRender::SetWorldTransform(m_Render, items[i], worlds[i]);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
        ASSERT_EQ(i, m_Render->m_PersistentRenderItems[slots[i]].m_GpuSlot);
        ASSERT_NE(i, slots[i]);
    }
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::orthographic(-1, 1, -1, 1, -1, 1);
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::HGpuRenderBatch single = 0;
    dmRender::RenderObject    object;
    object.m_VertexCount = 3;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots, count, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ(count, m_Render->m_GpuIndirect.m_RenderSlots.Size());
    ASSERT_EQ(count, m_Render->m_GpuIndirect.m_Worlds.Size());
    ASSERT_EQ(count, m_Render->m_GpuCulling.m_Bounds.Size());
    ASSERT_LT(dmGraphics::GetStorageBufferSize(m_Graphics, m_Render->m_GpuIndirect.m_WorldBuffer), m_Render->m_PersistentRenderItems.Size() * sizeof(Matrix4));
    uint32_t mapping[count];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_SlotBuffer, 0, sizeof(mapping), mapping));
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_EQ(i, mapping[i]);
    uint32_t args[5];
    ASSERT_TRUE(ReadBuffer(m_Graphics, batch->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(count, args[1]);

    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slots[count - 1], 1, 64, 0, -1));
    single = m_Render->m_GpuIndirect.m_Batches[0];
    dmGraphics::HAsyncBufferReadback previous = dmGraphics::RequestStorageBufferReadback(m_Graphics, single->m_InstanceBuffer, 0, sizeof(Matrix4));
    uint64_t                         slot_revision = m_Render->m_GpuIndirect.m_SlotRevision;
    dmRender::HRenderItem            deleted = items[0];
    ASSERT_TRUE(dmRender::DeleteRenderItem(m_Render, deleted));
    items[0] = 0;
    ASSERT_EQ(0u, m_Render->m_PersistentRenderItems[slots[count - 1]].m_GpuSlot);
    ASSERT_NE(slot_revision, m_Render->m_GpuIndirect.m_SlotRevision);
    cpu[count * gap] = dmRender::NewRenderItem(m_Render, dispatch, 0);
    ASSERT_EQ(slots[0], (uint32_t)cpu[count * gap]);
    ASSERT_NE(deleted, cpu[count * gap]);
    ASSERT_FALSE(dmRender::SetWorldTransform(m_Render, deleted, Matrix4::identity()));
    dmRender::SetMaterial(m_Render, cpu[count * gap], 0);
    dmRender::Enable(m_Render, cpu[count * gap], true);
    ASSERT_EQ(~0u, m_Render->m_PersistentRenderItems[slots[0]].m_GpuSlot);
    Matrix4 moved = Matrix4::translation(Vector3(0.6f, 0, 0));
    dmRender::SetWorldTransform(m_Render, items[count - 1], moved);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slots[count - 1], 1, 64, 0, -1));
    single = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_SlotBuffer, 0, sizeof(uint32_t), mapping));
    ASSERT_EQ(0u, mapping[0]);
    Matrix4 instance;
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_InstanceBuffer, 0, sizeof(instance), &instance));
    ASSERT_EQ(0, memcmp(&moved, &instance, sizeof(instance)));
    ASSERT_TRUE(ReadSnapshot(previous, &instance));
    ASSERT_EQ(0, memcmp(&worlds[count - 1], &instance, sizeof(instance)));
    dmGraphics::DeleteStorageBufferReadback(previous);
    ASSERT_EQ(count - 1, m_Render->m_GpuIndirect.m_Worlds.Size());
    ASSERT_TRUE(m_Render->m_GpuIndirect.m_DirtyWorldSlots.Empty());
    ASSERT_TRUE(m_Render->m_GpuCulling.m_DirtySlots.Empty());

    // Reuse the renderer slot as a GPU model, then move the surviving model
    // again through an eligibility change without changing its batch list.
    dmRender::SetBoundingOBB(m_Render, cpu[count * gap], Vector3(-0.01f), Vector3(0.01f));
    dmRender::SetRenderItemGpuDriven(m_Render, cpu[count * gap], true);
    dmRender::SetRenderItemGpuDriven(m_Render, items[count - 1], false);
    dmRender::SetRenderItemGpuDriven(m_Render, items[count - 1], true);
    ASSERT_EQ(count - 1, m_Render->m_PersistentRenderItems[slots[count - 1]].m_GpuSlot);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slots[count - 1], 1, 64, 0, -1));
    single = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_SlotBuffer, 0, sizeof(uint32_t), mapping));
    ASSERT_EQ(count - 1, mapping[0]);
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_InstanceBuffer, 0, sizeof(instance), &instance));
    ASSERT_EQ(0, memcmp(&moved, &instance, sizeof(instance)));
    dmRender::Enable(m_Render, items[count - 1], false);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slots[count - 1], 1, 64, 0, -1));
    single = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(0u, args[1]);
    ASSERT_EQ(count, m_Render->m_GpuIndirect.m_RenderSlots.Size());
    dmRender::Enable(m_Render, items[count - 1], true);
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, &slots[count - 1], 1, 64, 0, -1));
    single = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_TRUE(ReadBuffer(m_Graphics, single->m_ArgumentBuffer, 0, sizeof(args), args));
    ASSERT_EQ(1u, args[1]);
    for (uint32_t i = 1; i < count; ++i)
        dmRender::DeleteRenderItem(m_Render, items[i]);
    for (uint32_t i = 0; i <= count * gap; ++i)
        dmRender::DeleteRenderItem(m_Render, cpu[i]);
    ASSERT_TRUE(m_Render->m_GpuIndirect.m_RenderSlots.Empty());
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
}

// Verifies visible IDs fetch the same persistent matrices as ordinary instancing,
// including earlier draws across remaps and fallback/recreation on shader reload.
// Overlapping ordered passes must match CPU pixels with GPU culling enabled.
TEST_F(GpuIndirectTest, VisibleInstanceIdsAndPixels)
{
    FILE* file = fopen(VISIBLE_INSTANCE_SHADER_FILE, "rb");
    ASSERT_NE((FILE*)0, file);
    fseek(file, 0, SEEK_END);
    uint32_t size = ftell(file);
    rewind(file);
    uint8_t* bytes = new uint8_t[size];
    ASSERT_EQ(size, fread(bytes, 1, size, file));
    fclose(file);
    dmGraphics::ShaderDesc* desc = 0;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessage(bytes, size, &desc));
    delete[] bytes;
    char                 error[512] = {};
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Graphics, desc, error, sizeof(error));
    ASSERT_NE((dmGraphics::HProgram)0, program);
    dmhash_t world_attribute = dmHashString64("custom_world");
    ASSERT_NE((dmGraphics::HProgram)0, dmGraphics::GetVisibleInstanceProgram(m_Graphics, program, world_attribute));
    ASSERT_EQ((dmGraphics::HProgram)0, dmGraphics::GetVisibleInstanceProgram(m_Graphics, program, dmHashString64("different_attribute")));
    dmGraphics::HProgram companion = dmGraphics::GetVisibleInstanceProgram(m_Graphics, program, world_attribute);
    for (uint32_t i = 0; i < dmGraphics::GetUniformCount(program); ++i)
    {
        dmGraphics::Uniform uniform;
        dmGraphics::GetUniform(program, i, &uniform);
        ASSERT_EQ(uniform.m_Location, dmGraphics::FindUniformLocation(companion, uniform.m_NameHash));
    }
    dmRender::HMaterial material = dmRender::NewMaterial(m_Render, program);
    dmRender::SetMaterialTags(material, 0, 0);
    dmGraphics::VertexAttribute attribute = {};
    attribute.m_Name = (char*)"custom_world";
    attribute.m_NameHash = world_attribute;
    attribute.m_SemanticType = dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX;
    attribute.m_DataType = dmGraphics::VertexAttribute::TYPE_FLOAT;
    attribute.m_VectorType = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4;
    attribute.m_StepFunction = dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
    dmRender::SetMaterialProgramAttributes(material, &attribute, 1);
    dmRender::SetMaterialProgramConstantType(material, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
    Vector4 tint(1, 0.25f, 0, 1);
    dmRender::SetMaterialProgramConstant(material, dmHashString64("tint"), &tint, 1);
    dmRender::SetViewMatrix(m_Render, Matrix4::identity());
    dmRender::SetProjectionMatrix(m_Render, Matrix4::identity());
    float                                  vertices[] = { -0.2f, -0.2f, 0, 1, 0.2f, -0.2f, 0, 1, 0, 0.2f, 0, 1 };
    uint16_t                               indices[] = { 0, 1, 2 };
    dmGraphics::HStorageBuffer             vertex_storage = dmGraphics::NewStorageBuffer(m_Graphics, sizeof(vertices), vertices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::HIndexBuffer               index_buffer = dmGraphics::NewIndexBuffer(m_Graphics, sizeof(indices), indices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
    dmGraphics::RenderTargetCreationParams target_params = {};
    target_params.m_ColorBufferCreationParams[0].m_Width = target_params.m_ColorBufferParams[0].m_Width = 64;
    target_params.m_ColorBufferCreationParams[0].m_Height = target_params.m_ColorBufferParams[0].m_Height = 64;
    target_params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    target_params.m_ColorBufferCreationParams[0].m_UsageHintBits = dmGraphics::TEXTURE_USAGE_FLAG_SAMPLE;
    target_params.m_ColorBufferStoreOps[0] = dmGraphics::ATTACHMENT_OP_STORE;
    dmGraphics::HRenderTarget targets[3];
    for (uint32_t i = 0; i < 3; ++i)
        targets[i] = dmGraphics::NewRenderTarget(m_Graphics, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_params);
    dmRender::RenderListBegin(m_Render);
    dmRender::HRenderItemDispatch dispatch = dmRender::RegisterRenderListDispatch(m_Render, NoopDispatch, 0, 0);
    dmRender::HRenderItem         items[3];
    uint32_t                      slots[3];
    Matrix4                       worlds[2][3] = {
        { Matrix4::translation(Vector3(-0.45f, 0, 0)), Matrix4::translation(Vector3(0.45f, 0, 0)), Matrix4::translation(Vector3(8, 0, 0)) },
        { Matrix4::translation(Vector3(-0.3f, 0, 0)), Matrix4::translation(Vector3(0.35f, 0, 0)), Matrix4::identity() }
    };
    for (uint32_t i = 0; i < 3; ++i)
    {
        items[i] = dmRender::NewRenderItem(m_Render, dispatch, 0);
        slots[i] = (uint32_t)items[i];
        dmRender::SetMaterial(m_Render, items[i], material);
        dmRender::SetBoundingOBB(m_Render, items[i], Vector3(-0.2f), Vector3(0.2f));
        dmRender::SetWorldTransform(m_Render, items[i], worlds[0][i]);
        dmRender::SetRenderItemGpuDriven(m_Render, items[i], true);
        dmRender::Enable(m_Render, items[i], true);
    }
    dmRender::SubmitRenderListDispatch(m_Render, dispatch);
    dmRender::HGpuRenderBatch batch = 0;
    dmRender::RenderObject    object;
    object.m_Material = material;
    object.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
    object.m_VertexBuffers[0] = dmGraphics::GetStorageBufferVertexBuffer(m_Graphics, vertex_storage);
    object.m_VertexDeclarations[0] = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    object.m_IndexBuffer = index_buffer;
    object.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
    object.m_VertexCount = 3;
    dmRender::FrustumOptions options;
    options.m_Matrix = Matrix4::identity();
    options.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
    dmGraphics::HAsyncBufferReadback snapshots[2];
    dmGraphics::HAsyncBufferReadback counts[2];
    dmGraphics::SetViewport(m_Graphics, 0, 0, 64, 64);
    dmGraphics::SetColorMask(m_Graphics, true, true, true, true);
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_CULL_FACE);
    // Encode both GPU draws before waiting: the second changes the shared world
    // buffer, dense mapping, and the reused visible-ID buffer after the first draw.
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        if (phase)
        {
            dmRender::DeleteRenderItem(m_Render, items[0]);
            dmRender::SetWorldTransform(m_Render, items[1], worlds[1][1]);
            dmRender::SetWorldTransform(m_Render, items[2], worlds[1][0]);
        }
        dmRender::ClearRenderObjects(m_Render);
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots + phase, 3 - phase, phase ? 64 : 128, 0, -1));
        batch = m_Render->m_GpuIndirect.m_Batches[0];
        ASSERT_NE((dmGraphics::HProgram)0, batch->m_VisibleInstanceProgram);
        ASSERT_EQ(4u, batch->m_Stride);
        ASSERT_EQ(12u, dmGraphics::GetStorageBufferSize(m_Graphics, batch->m_InstanceBuffer));
        dmGraphics::SetRenderTarget(m_Graphics, targets[phase], dmGraphics::RenderTargetBindingParams());
        dmGraphics::Clear(m_Graphics, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::Draw(m_Render, 0, 0));
        snapshots[phase] = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_InstanceBuffer, 0, 8);
        counts[phase] = dmGraphics::RequestStorageBufferReadback(m_Graphics, batch->m_ArgumentBuffer, 0, 20);
    }
    CompleteFrame();
    dmGraphics::BeginFrame(m_Graphics);
    uint8_t actual[64 * 64 * 4], expected[64 * 64 * 4];
    for (uint32_t phase = 0; phase < 2; ++phase)
    {
        uint32_t ids[2], arguments[5];
        ASSERT_TRUE(ReadSnapshot(snapshots[phase], ids));
        ASSERT_TRUE(ReadSnapshot(counts[phase], arguments));
        ASSERT_EQ(2u, arguments[1]);
        ASSERT_NE(ids[0], ids[1]);
        Matrix4 matrices[2];
        for (uint32_t i = 0; i < 2; ++i)
        {
            ASSERT_LT(ids[i], 2u);
            matrices[i] = worlds[phase][ids[i]];
        }
        dmGraphics::SetRenderTarget(m_Graphics, targets[phase], dmGraphics::RenderTargetBindingParams());
        dmGraphics::ReadPixels(m_Graphics, 0, 0, 64, 64, actual, sizeof(actual));
        dmGraphics::HStorageBuffer instances = dmGraphics::NewStorageBuffer(m_Graphics, sizeof(matrices), matrices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        dmRender::RenderObject     reference = object;
        reference.m_VertexBuffers[1] = dmGraphics::GetStorageBufferVertexBuffer(m_Graphics, instances);
        reference.m_VertexDeclarations[1] = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
        reference.m_InstanceCount = 2;
        dmRender::ClearRenderObjects(m_Render);
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &reference));
        dmGraphics::SetRenderTarget(m_Graphics, targets[2], dmGraphics::RenderTargetBindingParams());
        dmGraphics::Clear(m_Graphics, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
        ASSERT_EQ(dmRender::RESULT_OK, dmRender::Draw(m_Render, 0, 0));
        dmGraphics::ReadPixels(m_Graphics, 0, 0, 64, 64, expected, sizeof(expected));
        ASSERT_EQ(0, memcmp(actual, expected, sizeof(actual)));
        uint32_t colored = 0;
        for (uint32_t i = 0; i < sizeof(actual); i += 4)
            colored += actual[i + 2] != 0;
        ASSERT_GT(colored, 0u);
        dmGraphics::DeleteStorageBuffer(m_Graphics, instances);
        dmGraphics::DeleteStorageBufferReadback(snapshots[phase]);
        dmGraphics::DeleteStorageBufferReadback(counts[phase]);
    }
    // Two overlapping instances have different colors through gl_InstanceIndex.
    // Compare each ordered pass against the same draw with GPU culling disabled.
    OrderedPixelBatch ordered;
    ordered.m_Object = object;
    ordered.m_Instances = dmGraphics::NewVertexBuffer(m_Graphics, 2 * sizeof(Matrix4), 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ordered.m_Object.m_VertexBuffers[1] = ordered.m_Instances;
    ordered.m_Object.m_VertexDeclarations[1] = dmRender::GetVertexDeclaration(material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
    dmRender::HRenderItemDispatch ordered_dispatch = dmRender::RegisterRenderListDispatch(m_Render, DrawOrderedPixelBatch, 0, &ordered);
    dmRender::HRenderItem         ordered_items[2];
    for (uint32_t i = 0; i < 2; ++i)
    {
        ordered_items[i] = dmRender::NewRenderItem(m_Render, ordered_dispatch, i);
        dmRender::SetMaterial(m_Render, ordered_items[i], material);
        dmRender::SetOrder(m_Render, ordered_items[i], dmRender::RENDER_ORDER_BEFORE_WORLD, 0);
        dmRender::SetBoundingOBB(m_Render, ordered_items[i], Vector3(-0.2f), Vector3(0.2f));
        dmRender::SetWorldTransform(m_Render, ordered_items[i], Matrix4::identity());
        dmRender::SetRenderItemGpuDriven(m_Render, ordered_items[i], true);
        dmRender::Enable(m_Render, ordered_items[i], true);
    }
    target_params.m_DepthBufferCreationParams.m_Width = target_params.m_DepthBufferParams.m_Width = 64;
    target_params.m_DepthBufferCreationParams.m_Height = target_params.m_DepthBufferParams.m_Height = 64;
    target_params.m_DepthBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_DEPTH;
    target_params.m_StencilBufferCreationParams.m_Width = target_params.m_StencilBufferParams.m_Width = 64;
    target_params.m_StencilBufferCreationParams.m_Height = target_params.m_StencilBufferParams.m_Height = 64;
    target_params.m_StencilBufferParams.m_Format = dmGraphics::TEXTURE_FORMAT_STENCIL;
    uint32_t                  target_flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::HRenderTarget ordered_target = dmGraphics::NewRenderTarget(m_Graphics, target_flags, target_params);
    for (uint32_t state = 0; state < 3; ++state)
    {
        dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
        dmGraphics::SetDepthMask(m_Graphics, true);
        dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_STENCIL_TEST);
        if (state == 0)
            dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
        if (state == 1)
            dmGraphics::SetDepthMask(m_Graphics, false);
        if (state == 2)
        {
            dmGraphics::EnableState(m_Graphics, dmGraphics::STATE_STENCIL_TEST);
            dmGraphics::SetStencilFunc(m_Graphics, dmGraphics::COMPARE_FUNC_ALWAYS, 1, 0xff);
            dmGraphics::SetStencilOp(m_Graphics, dmGraphics::STENCIL_OP_KEEP, dmGraphics::STENCIL_OP_KEEP, dmGraphics::STENCIL_OP_REPLACE);
        }
        dmGraphics::HProgram culling_program = m_Render->m_GpuIndirect.m_Program;
        for (uint32_t control = 0; control < 2; ++control)
        {
            m_Render->m_GpuIndirect.m_Program = control ? culling_program : 0;
            dmRender::RenderListBegin(m_Render);
            dmRender::SubmitRenderListDispatch(m_Render, ordered_dispatch);
            ordered.m_Gpu = true;
            dmGraphics::SetRenderTarget(m_Graphics, ordered_target, dmGraphics::RenderTargetBindingParams());
            dmGraphics::Clear(m_Graphics, target_flags, 0, 0, 0, 255, 1, 0);
            ASSERT_EQ(dmRender::RESULT_OK, dmRender::DrawRenderList(m_Render, 0, 0, &options, dmRender::SORT_NONE));
            ASSERT_FALSE(ordered.m_Gpu);
            ASSERT_EQ(1u, m_Render->m_RenderObjects.Size());
            dmGraphics::ReadPixels(m_Graphics, 0, 0, 64, 64, control ? actual : expected, sizeof(actual));
        }
        ASSERT_EQ(0, memcmp(actual, expected, sizeof(actual)));
        ASSERT_EQ(255u, actual[(32 * 64 + 32) * 4 + 2]);
    }
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_STENCIL_TEST);
    dmGraphics::SetDepthMask(m_Graphics, true);
    dmGraphics::DisableState(m_Graphics, dmGraphics::STATE_DEPTH_TEST);
    dmGraphics::SetRenderTarget(m_Graphics, 0, dmGraphics::RenderTargetBindingParams());
    dmGraphics::DeleteRenderTarget(m_Graphics, ordered_target);
    for (uint32_t i = 0; i < 2; ++i)
        dmRender::DeleteRenderItem(m_Render, ordered_items[i]);
    dmRender::UnregisterRenderListDispatch(m_Render, ordered_dispatch);
    dmGraphics::DeleteVertexBuffer(ordered.m_Instances);

    // A resource compiled by an older Bob has no companion and retains the
    // matrix-compaction path. Reloading the companion restores ID output.
    uint32_t companion_size = desc->m_VisibleInstanceProgram.m_Count;
    desc->m_VisibleInstanceProgram.m_Count = 0;
    ASSERT_TRUE(dmGraphics::ReloadProgram(m_Graphics, program, desc, error, sizeof(error)));
    ASSERT_EQ((dmGraphics::HProgram)0, dmGraphics::GetVisibleInstanceProgram(m_Graphics, program, world_attribute));
    dmRender::ClearRenderObjects(m_Render);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::AddToRender(m_Render, &object, slots + 1, 2, 64, 0, -1));
    batch = m_Render->m_GpuIndirect.m_Batches[0];
    ASSERT_EQ((dmGraphics::HProgram)0, batch->m_VisibleInstanceProgram);
    ASSERT_EQ(64u, batch->m_Stride);
    desc->m_VisibleInstanceProgram.m_Count = companion_size;
    ASSERT_TRUE(dmGraphics::ReloadProgram(m_Graphics, program, desc, error, sizeof(error)));
    ASSERT_NE((dmGraphics::HProgram)0, dmGraphics::GetVisibleInstanceProgram(m_Graphics, program, world_attribute));
    dmDDF::FreeMessage(desc);
    dmRender::ClearRenderObjects(m_Render);
    dmGraphics::SetRenderTarget(m_Graphics, 0, dmGraphics::RenderTargetBindingParams());
    dmRender::DeleteRenderItem(m_Render, items[1]);
    dmRender::DeleteRenderItem(m_Render, items[2]);
    dmRender::UnregisterRenderListDispatch(m_Render, dispatch);
    dmRender::DeleteMaterial(m_Render, material);
    dmGraphics::DeleteProgram(m_Graphics, program);
    dmGraphics::DeleteIndexBuffer(index_buffer);
    dmGraphics::DeleteStorageBuffer(m_Graphics, vertex_storage);
    for (uint32_t i = 0; i < 3; ++i)
        dmGraphics::DeleteRenderTarget(m_Graphics, targets[i]);
}

extern "C" void dmExportedSymbols();
int             main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
