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
#include <testmain/testmain.h>
#include <dlib/atomic.h>
#include <dlib/condition_variable.h>
#include <dlib/jobsystem.h>
#include <platform/window.hpp>
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>
#include <objc/runtime.h>

#include "graphics_adapter.h"
#include "metal/graphics_metal_private.h"

extern "C" void GraphicsAdapterMetal();

// Observe actual native-object destruction without relying on Metal's retain counts.
@interface MetalRetirementProbe : NSObject
{
    @public
    int32_atomic_t* m_Destroyed;
}
@end

@implementation MetalRetirementProbe
- (void)dealloc
{
    dmAtomicIncrement32(m_Destroyed);
    [super dealloc];
}
@end

static void TrackTextureDestruction(MTL::Texture* texture, int32_atomic_t* destroyed)
{
    static const char key = 0;
    MetalRetirementProbe* probe = [[MetalRetirementProbe alloc] init];
    probe->m_Destroyed = destroyed;
    objc_setAssociatedObject((id)texture, &key, probe, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [probe release];
}

// A single worker gives the upload and sentinel a deterministic order. Callback
// delivery remains on the test thread, just as it does on the engine's game owner.
struct MetalResourceLifetimeTest : public jc_test_base_class
{
    NS::AutoreleasePool*                    m_Pool;
    dmGraphics::MetalContext*               m_Context;
    MTL::Buffer*                            m_Readback;
    dmGraphics::HTexture                    m_Texture;
    HWindow                                 m_Window;
    HJobContext                             m_Jobs;
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    int32_atomic_t                          m_DestroyedTextures;
    uint32_t                                m_Callbacks;
    uint8_t                                 m_Pixels[4 * 4 * 4];
    // Full bytes: the worker and test thread update these independently under the mutex.
    uint8_t                                 m_WorkerEntered;
    uint8_t                                 m_WorkerReleased;
    uint8_t                                 m_UploadProcessed;

    void SetUp()
    {
        m_Pool = NS::AutoreleasePool::alloc()->init();
        m_Context = 0;
        m_Readback = 0;
        m_Texture = 0;
        m_Jobs = 0;
        m_Window = dmPlatform::NewWindow();
        m_Mutex = dmMutex::New();
        m_Condition = dmConditionVariable::New();
        dmAtomicStore32(&m_DestroyedTextures, 0);
        m_Callbacks = 0;
        m_WorkerEntered = m_WorkerReleased = m_UploadProcessed = 0;
        memset(m_Pixels, 0x39, sizeof(m_Pixels));

        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        window_params.m_Title = "Metal resource lifetime";
        window_params.m_Width = window_params.m_Height = 16;
        window_params.m_Hidden = 1;
        window_params.m_FocusOnShow = 0;
        window_params.m_Samples = 1;
        window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, window_params));

        JobSystemCreateParams jobs = {};
        jobs.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&jobs);
        ASSERT_NE((HJobContext)0, m_Jobs);

        dmGraphics::ContextParams context_params;
        context_params.m_Window = m_Window;
        context_params.m_JobContext = m_Jobs;
        context_params.m_Width = context_params.m_Height = 16;
        context_params.m_VerifyGraphicsCalls = 1;
        m_Context = (dmGraphics::MetalContext*)dmGraphics::NewContext(context_params);
        ASSERT_NE((dmGraphics::MetalContext*)0, m_Context);
        ASSERT_TRUE(m_Context->m_AsyncProcessingSupport);
    }

    void ReleaseWorker()
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        m_WorkerReleased = 1;
        dmConditionVariable::Broadcast(m_Condition);
    }

    void RequestTextureDeletion()
    {
        dmGraphics::DeleteTexture((dmGraphics::HContext)m_Context, m_Texture);
        m_Texture = 0;
    }

    void DeleteGraphics()
    {
        if (m_Texture)
        {
            RequestTextureDeletion();
        }
        dmGraphics::DeleteContext((dmGraphics::HContext)m_Context);
        m_Context = 0;
        if (m_Readback)
        {
            m_Readback->release();
            m_Readback = 0;
        }
        m_Pool->release();
        m_Pool = NS::AutoreleasePool::alloc()->init();
    }

    void TearDown()
    {
        ReleaseWorker();
        if (m_Context)
        {
            DeleteGraphics();
        }
        JobSystemDestroy(m_Jobs);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
        dmConditionVariable::Delete(m_Condition);
        dmMutex::Delete(m_Mutex);
        m_Pool->release();
    }

    static int BlockWorker(HJobContext, HJob, void*, void* data)
    {
        MetalResourceLifetimeTest* test = (MetalResourceLifetimeTest*)data;
        DM_MUTEX_SCOPED_LOCK(test->m_Mutex);
        test->m_WorkerEntered = 1;
        dmConditionVariable::Broadcast(test->m_Condition);
        while (!test->m_WorkerReleased)
        {
            dmConditionVariable::Wait(test->m_Condition, test->m_Mutex);
        }
        return 0;
    }

    static int MarkUploadProcessed(HJobContext, HJob, void*, void* data)
    {
        MetalResourceLifetimeTest* test = (MetalResourceLifetimeTest*)data;
        DM_MUTEX_SCOPED_LOCK(test->m_Mutex);
        test->m_UploadProcessed = 1;
        dmConditionVariable::Broadcast(test->m_Condition);
        return 0;
    }

    static void UploadComplete(dmGraphics::HTexture, void* data)
    {
        MetalResourceLifetimeTest* test = (MetalResourceLifetimeTest*)data;
        ++test->m_Callbacks;
    }

    void PushJob(FJobProcess process)
    {
        Job job = {};
        job.m_Process = process;
        job.m_Data = this;
        ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job)));
    }

    void HoldWorker()
    {
        PushJob(BlockWorker);
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_WorkerEntered)
        {
            dmConditionVariable::Wait(m_Condition, m_Mutex);
        }
    }

    dmGraphics::HTexture UploadTexture()
    {
        dmGraphics::TextureCreationParams creation;
        creation.m_Width = creation.m_Height = 4;
        dmGraphics::HContext context = (dmGraphics::HContext)m_Context;
        dmGraphics::HTexture texture = dmGraphics::NewTexture(context, creation);
        m_Texture = texture;
        dmGraphics::TextureParams params;
        params.m_Width = params.m_Height = 4;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        params.m_Data = m_Pixels;
        params.m_DataSize = sizeof(m_Pixels);
        dmGraphics::SetTextureAsync(context, texture, params, UploadComplete, this);
        return texture;
    }

    MTL::Texture* GetNativeTexture(dmGraphics::HTexture texture)
    {
        return dmGraphics::GetAssetFromContainer<dmGraphics::MetalTexture>(
            m_Context->m_BaseContext.m_AssetHandleContainer, texture)->m_Texture;
    }

    void FinishUpload()
    {
        PushJob(MarkUploadProcessed);
        ReleaseWorker();
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (!m_UploadProcessed)
            {
                dmConditionVariable::Wait(m_Condition, m_Mutex);
            }
        }
        JobSystemUpdate(m_Jobs, 0);
        ASSERT_EQ(1u, m_Callbacks);
        ASSERT_EQ(0, dmAtomicGet32(&m_Context->m_PendingAsyncTextureJobs));
    }
};

// Upload completion must progress without a rendered frame, and deleting its handle
// while the upload is queued must retain the native texture until idle shutdown.
TEST_F(MetalResourceLifetimeTest, UploadAndDeferredDeleteWithoutFrames)
{
    using namespace dmGraphics;
    HoldWorker();
    HContext context = (HContext)m_Context;
    HTexture texture = UploadTexture();
    MTL::Texture* native_texture = GetNativeTexture(texture);
    TrackTextureDestruction(native_texture, &m_DestroyedTextures);
    RequestTextureDeletion();
    ASSERT_TRUE(IsAssetHandleValid(context, texture));
    ASSERT_TRUE(GetTextureStatusFlags(context, texture) & TEXTURE_STATUS_DATA_PENDING);

    FinishUpload();
    ASSERT_EQ(0u, GetTextureStatusFlags(context, texture) & TEXTURE_STATUS_DATA_PENDING);

    // Read back the actual upload through a non-presenting command buffer. Waiting
    // for this test-only readback is an explicit GPU completion, not a timing delay.
    m_Readback = m_Context->m_Device->newBuffer(256 * 4, MTL::ResourceStorageModeShared);
    MTL::CommandBuffer* command = m_Context->m_CommandQueue->commandBuffer();
    MTL::BlitCommandEncoder* blit = command->blitCommandEncoder();
    blit->copyFromTexture(native_texture, 0, 0, MTL::Origin::Make(0, 0, 0),
                          MTL::Size::Make(4, 4, 1), m_Readback, 0, 256, 256 * 4);
    blit->endEncoding();
    command->commit();
    command->waitUntilCompleted();
    ASSERT_EQ(MTL::CommandBufferStatusCompleted, command->status());
    for (uint32_t row = 0; row < 4; ++row)
    {
        ASSERT_EQ(0, memcmp((const uint8_t*)m_Readback->contents() + row * 256, m_Pixels + row * 16, 16));
    }
    m_Readback->release();
    m_Readback = 0;

    ASSERT_FALSE(m_Context->m_FrameBegun);
    ASSERT_EQ(0u, m_Context->m_CurrentFrameInFlight);
    CloseWindow(context);
    ASSERT_FALSE(IsAssetHandleValid(context, texture));
    ASSERT_EQ(1u, m_Context->m_ResourceRetirementQueue->m_Pending.Size());
    ASSERT_EQ(0, dmAtomicGet32(&m_DestroyedTextures));
    DeleteGraphics();
    ASSERT_EQ(1, dmAtomicGet32(&m_DestroyedTextures));
}

// Context deletion must deliver every accepted upload callback before freeing its
// state, even if no frame or explicit job-system update ever took place.
TEST_F(MetalResourceLifetimeTest, ShutdownDeliversPendingUploadCompletion)
{
    using namespace dmGraphics;
    HoldWorker();
    HTexture texture = UploadTexture();
    TrackTextureDestruction(GetNativeTexture(texture), &m_DestroyedTextures);
    RequestTextureDeletion();
    ASSERT_EQ(1, dmAtomicGet32(&m_Context->m_PendingAsyncTextureJobs));
    ASSERT_EQ(0u, m_Callbacks);

    ReleaseWorker();
    DeleteGraphics();
    ASSERT_EQ(1u, m_Callbacks);
    ASSERT_EQ(1, dmAtomicGet32(&m_DestroyedTextures));
    JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_Callbacks);
}

// Replacement and deletion before the first frame must retire both native texture
// allocations at teardown instead of waiting for a Flip that will never happen.
TEST_F(MetalResourceLifetimeTest, ReplacementBeforeFirstFrameRetiresBothAllocations)
{
    using namespace dmGraphics;
    HoldWorker();
    HTexture texture = UploadTexture();
    FinishUpload();
    MTL::Texture* first = GetNativeTexture(texture);
    TrackTextureDestruction(first, &m_DestroyedTextures);

    TextureParams replacement;
    replacement.m_Width = replacement.m_Height = 2;
    replacement.m_Format = TEXTURE_FORMAT_RGBA;
    replacement.m_Data = m_Pixels;
    replacement.m_DataSize = 2 * 2 * 4;
    SetTexture((HContext)m_Context, texture, replacement);
    MTL::Texture* second = GetNativeTexture(texture);
    ASSERT_NE(first, second);
    TrackTextureDestruction(second, &m_DestroyedTextures);
    RequestTextureDeletion();

    ASSERT_EQ(2u, m_Context->m_ResourceRetirementQueue->m_Pending.Size());
    ASSERT_EQ(0, dmAtomicGet32(&m_DestroyedTextures));
    ASSERT_FALSE(m_Context->m_FrameBegun);
    DeleteGraphics();
    ASSERT_EQ(2, dmAtomicGet32(&m_DestroyedTextures));
}

int main(int argc, char** argv)
{
    TestMainPlatformInit();
    GraphicsAdapterMetal();
    if (!dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_METAL))
    {
        return 1;
    }
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmGraphics::Finalize();
    return result;
}
