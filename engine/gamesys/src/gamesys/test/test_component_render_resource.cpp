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

#include <dlib/thread.h>
#include <render/render_frame_queue.h>
#include <testmain/testmain.h>

#include "gamesys/component_render_resource.h"

using namespace dmRender;
using namespace dmGameSystem;

// A fake resource can own another factory resource, like a material owning a program.
struct CapturedResource
{
    CapturedResource* m_Dependency;
    uint32_t          m_Value;
};

struct CapturedResourceSource
{
    const char* m_DependencyPath;
    uint32_t    m_Value;
};

// Records resource destruction and the values seen by a consumer. The test joins
// a threaded consumer before inspecting its writes; no timing assumptions apply.
struct ResourceCaptureProbe
{
    uint32_t          m_DestroyedValues[8];
    HRenderExecution  m_Execution;
    HRenderFrameQueue m_MutationQueue;
    RenderFrameResult m_LastBeginResult;
    uint32_t          m_DestroyedCount;
    uint32_t          m_ConsumedValue;
    uint32_t          m_ConsumedDependency;
    uint32_t          m_BeginCount;
    uint32_t          m_EndCount;
    uint32_t          m_RecreateCount;
    uint8_t           m_WrongDestroyOwner : 1;
    uint8_t           m_WrongConsumeOwner : 1;
    uint8_t           m_FailRecreate : 1;
};

struct ResourceCaptureFixture
{
    ResourceCaptureProbe m_Probe;
    dmResource::HFactory m_Factory;
    HRenderExecution     m_Execution;
    HRenderFrameStorage  m_Frame;
};

static dmResource::Result CreateCapturedResource(const dmResource::ResourceCreateParams* params)
{
    const CapturedResourceSource* source = (const CapturedResourceSource*)params->m_Buffer;
    CapturedResource* resource = new CapturedResource;
    resource->m_Dependency = 0;
    resource->m_Value = source->m_Value;
    if (source->m_DependencyPath)
    {
        dmResource::Result result = dmResource::Get(params->m_Factory, source->m_DependencyPath, (void**)&resource->m_Dependency);
        if (result != dmResource::RESULT_OK)
        {
            delete resource;
            return result;
        }
    }
    ResourceDescriptorSetResource(params->m_Resource, resource);
    return dmResource::RESULT_OK;
}

static dmResource::Result DestroyCapturedResource(const dmResource::ResourceDestroyParams* params)
{
    ResourceCaptureProbe* probe = (ResourceCaptureProbe*)params->m_Context;
    CapturedResource* resource = (CapturedResource*)ResourceDescriptorGetResource(params->m_Resource);
    probe->m_WrongDestroyOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_GAME);
    probe->m_DestroyedValues[probe->m_DestroyedCount++] = resource->m_Value;
    if (resource->m_Dependency)
    {
        dmResource::Release(params->m_Factory, resource->m_Dependency);
    }
    delete resource;
    return dmResource::RESULT_OK;
}

static dmResource::Result RecreateCapturedResource(const dmResource::ResourceRecreateParams* params)
{
    ResourceCaptureProbe* probe = (ResourceCaptureProbe*)params->m_Context;
    ++probe->m_RecreateCount;
    if (probe->m_FailRecreate)
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
    CapturedResource* resource = (CapturedResource*)ResourceDescriptorGetResource(params->m_Resource);
    const CapturedResourceSource* source = (const CapturedResourceSource*)params->m_Buffer;
    resource->m_Value = source->m_Value;
    return dmResource::RESULT_OK;
}

static dmResource::Result BeginCapturedResourceMutation(void* context)
{
    ResourceCaptureProbe* probe = (ResourceCaptureProbe*)context;
    ++probe->m_BeginCount;
    if (!probe->m_MutationQueue)
    {
        return dmResource::RESULT_OK;
    }
    probe->m_LastBeginResult = BeginRenderFrameMutation(probe->m_MutationQueue);
    if (probe->m_LastBeginResult == RENDER_FRAME_OK)
    {
        return dmResource::RESULT_OK;
    }
    if (probe->m_LastBeginResult == RENDER_FRAME_WOULD_BLOCK)
    {
        // This test caller retries explicitly. A rejected synchronous reload must
        // not leave the queue waiting for an operation that was never accepted.
        EndRenderFrameMutation(probe->m_MutationQueue);
    }
    return dmResource::RESULT_INVAL;
}

static void EndCapturedResourceMutation(void* context)
{
    ResourceCaptureProbe* probe = (ResourceCaptureProbe*)context;
    ++probe->m_EndCount;
    if (probe->m_MutationQueue)
    {
        EXPECT_EQ(RENDER_FRAME_OK, EndRenderFrameMutation(probe->m_MutationQueue));
    }
}

static bool CreateResourceCaptureFixture(ResourceCaptureFixture* fixture, ExecutionMode mode, uint32_t references)
{
    memset(fixture, 0, sizeof(*fixture));
    ExecutionSelection selection;
    fixture->m_Execution = NewRenderExecution(mode, true, &selection);
    if (!fixture->m_Execution)
    {
        return false;
    }
    fixture->m_Probe.m_Execution = fixture->m_Execution;

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 8;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    params.m_BeginMutation = BeginCapturedResourceMutation;
    params.m_EndMutation = EndCapturedResourceMutation;
    params.m_MutationContext = &fixture->m_Probe;
#if defined(DM_PLATFORM_VENDOR)
    const char* mount_dir = "file:";
#else
    const char* mount_dir = ".";
#endif
    fixture->m_Factory = dmResource::NewFactory(&params, mount_dir);
    if (!fixture->m_Factory)
    {
        return false;
    }
    dmResource::Result result = dmResource::RegisterType(fixture->m_Factory, "capture", &fixture->m_Probe, 0,
                                                         CreateCapturedResource, 0, DestroyCapturedResource, RecreateCapturedResource);
    RenderFrameLimits limits = { 64, 2, references };
    fixture->m_Frame = NewRenderFrame(limits);
    return result == dmResource::RESULT_OK && fixture->m_Frame;
}

static void DeleteResourceCaptureFixture(ResourceCaptureFixture* fixture)
{
    if (fixture->m_Probe.m_MutationQueue)
    {
        StopRenderFrameQueue(fixture->m_Probe.m_MutationQueue);
        DeleteRenderFrameQueue(fixture->m_Probe.m_MutationQueue);
    }
    DeleteRenderFrame(fixture->m_Frame);
    dmResource::DeleteFactory(fixture->m_Factory);
    DeleteRenderExecution(fixture->m_Execution);
}

static dmResource::Result CreateTestResource(ResourceCaptureFixture* fixture, const char* path, uint32_t value, const char* dependency_path, CapturedResource** resource)
{
    CapturedResourceSource source = { dependency_path, value };
    return dmResource::CreateResource(fixture->m_Factory, path, &source, sizeof(source), (void**)resource);
}

static void ConsumeCapturedResource(void* context, const RenderFrameView& frame)
{
    ResourceCaptureProbe* probe = (ResourceCaptureProbe*)context;
    probe->m_WrongConsumeOwner |= !IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER);
    CapturedResource* resource = *(CapturedResource* const*)GetRenderFrameTableData(frame, 0);
    probe->m_ConsumedValue = resource->m_Value;
    if (resource->m_Dependency)
    {
        probe->m_ConsumedDependency = resource->m_Dependency->m_Value;
    }
}

struct ResourceCaptureExecutor
{
    HRenderFrameQueue      m_Queue;
    HRenderExecution       m_Execution;
    RenderFramePumpResult  m_PumpResult;
    uint8_t                m_Bound : 1;
};

static void ConsumeOnRenderThread(void* context)
{
    ResourceCaptureExecutor* executor = (ResourceCaptureExecutor*)context;
    executor->m_Bound = BindRenderExecutionOwner(executor->m_Execution);
    if (executor->m_Bound)
    {
        executor->m_PumpResult = PumpRenderFrameQueue(executor->m_Queue, true);
        ReleaseRenderExecutionOwner(executor->m_Execution);
    }
}

static void VerifyAcceptedResourceLifetime(ExecutionMode mode)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, mode, 2));
    CapturedResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/value.capture", 17, 0, &resource));

    RenderFrameQueueParams params = {};
    params.m_Execution = fixture.m_Execution;
    params.m_Limits.m_PayloadBytes = 64;
    params.m_Limits.m_MaxTables = 2;
    params.m_Limits.m_MaxReferences = 2;
    params.m_Consume = ConsumeCapturedResource;
    params.m_ConsumeContext = &fixture.m_Probe;
    HRenderFrameQueue queue = NewRenderFrameQueue(params);
    ASSERT_NE((HRenderFrameQueue)0, queue);

    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, ticket.m_Frame, resource));
    uint32_t table;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(ticket.m_Frame, 17, 1, sizeof(resource), sizeof(resource), &resource, &table, 0));
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(queue, ticket));

    dmResource::Release(fixture.m_Factory, resource);
    ASSERT_EQ(0u, fixture.m_Probe.m_DestroyedCount);
    if (mode == EXECUTION_MODE_INLINE)
    {
        ASSERT_EQ(RENDER_FRAME_PUMP_IDLE, PumpRenderFrameQueue(queue, false));
    }
    else
    {
        ResourceCaptureExecutor executor = {};
        executor.m_Queue = queue;
        executor.m_Execution = fixture.m_Execution;
        dmThread::Thread thread = dmThread::New(ConsumeOnRenderThread, 0x80000, &executor, "resource-consumer");
        dmThread::Join(thread);
        ASSERT_TRUE(executor.m_Bound);
        ASSERT_EQ(RENDER_FRAME_PUMP_CONSUMED, executor.m_PumpResult);
    }

    ASSERT_EQ(17u, fixture.m_Probe.m_ConsumedValue);
    ASSERT_EQ(0u, fixture.m_Probe.m_DestroyedCount);
    RetireRenderFrames(queue);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_EQ(17u, fixture.m_Probe.m_DestroyedValues[0]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    ASSERT_FALSE(fixture.m_Probe.m_WrongConsumeOwner);

    StopRenderFrameQueue(queue);
    DeleteRenderFrameQueue(queue);
    DeleteResourceCaptureFixture(&fixture);
}

// Inline consumption must leave factory releases to the explicit game-owner retirement boundary.
TEST(ComponentRenderResource, InlineFrameRetainsResourceUntilRetirement)
{
    VerifyAcceptedResourceLifetime(EXECUTION_MODE_INLINE);
}

// A render-thread consumer may read a resource after its game reference is released,
// but only game-owner retirement may destroy it.
TEST(ComponentRenderResource, ThreadedFrameRetainsResourceUntilGameOwnerRetirement)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (capabilities.m_BuildEnabled && capabilities.m_ThreadsSupported)
    {
        VerifyAcceptedResourceLifetime(EXECUTION_MODE_THREADED);
    }
}

// A failed reference insertion rolls back its factory retain without disturbing
// references already adopted by the partial capture.
TEST(ComponentRenderResource, ReferenceBudgetFailureRollsBackNewRetain)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 1));
    CapturedResource* first = 0;
    CapturedResource* second = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/first.capture", 1, 0, &first));
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/second.capture", 2, 0, &second));

    BeginRenderFrame(fixture.m_Frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, first));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, second));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(fixture.m_Frame));
    dmResource::Release(fixture.m_Factory, first);
    dmResource::Release(fixture.m_Factory, second);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_EQ(2u, fixture.m_Probe.m_DestroyedValues[0]);

    ResetRenderFrame(fixture.m_Frame);
    ASSERT_EQ(2u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedValues[1]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

// Repeated resources use separate slots and are released exactly once per retain
// when an unpublished capture is cancelled; later reset/delete must not release again.
TEST(ComponentRenderResource, CancelReleasesRepeatedReferencesOnce)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 2));
    CapturedResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/value.capture", 9, 0, &resource));
    BeginRenderFrame(fixture.m_Frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, resource));
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, resource));
    ASSERT_EQ(2u, GetRenderFrameStatistics(fixture.m_Frame).m_ReferenceCount);
    dmResource::Release(fixture.m_Factory, resource);
    ASSERT_EQ(0u, fixture.m_Probe.m_DestroyedCount);

    ResetRenderFrame(fixture.m_Frame);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ResetRenderFrame(fixture.m_Frame);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

// State errors must undo the provisional retain just like budget errors; a
// resource rejected by a sealed frame remains solely owned by its caller.
TEST(ComponentRenderResource, SealedFrameRejectsRetainWithoutLeaking)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 1));
    CapturedResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/value.capture", 12, 0, &resource));
    BeginRenderFrame(fixture.m_Frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(fixture.m_Frame));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, resource));
    ASSERT_EQ(0u, GetRenderFrameStatistics(fixture.m_Frame).m_ReferenceCount);

    dmResource::Release(fixture.m_Factory, resource);
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

// Tearing down storage containing an unfinished capture releases adopted factory
// references before the factory or its callback context can be deleted.
TEST(ComponentRenderResource, DeletingPartialStorageReleasesResources)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 1));
    CapturedResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/value.capture", 15, 0, &resource));
    BeginRenderFrame(fixture.m_Frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, resource));
    dmResource::Release(fixture.m_Factory, resource);
    ASSERT_EQ(0u, fixture.m_Probe.m_DestroyedCount);

    DeleteRenderFrame(fixture.m_Frame);
    fixture.m_Frame = 0;
    ASSERT_EQ(1u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

// Capturing a parent resource also keeps dependencies owned by that resource alive;
// the consumer does not need to retain interior pointers as factory resources.
TEST(ComponentRenderResource, RetainedParentPreservesNestedDependency)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 1));
    CapturedResourceSource source = { 0, 21 };
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(fixture.m_Factory, "/child.capture", sizeof(source), &source));
    CapturedResource* parent = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/parent.capture", 34, "/child.capture", &parent));
    ASSERT_EQ(21u, parent->m_Dependency->m_Value);

    BeginRenderFrame(fixture.m_Frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture.m_Factory, fixture.m_Frame, parent));
    uint32_t table;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(fixture.m_Frame, 17, 1, sizeof(parent), sizeof(parent), &parent, &table, 0));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(fixture.m_Frame));
    dmResource::Release(fixture.m_Factory, parent);
    ASSERT_EQ(0u, fixture.m_Probe.m_DestroyedCount);

    ConsumeCapturedResource(&fixture.m_Probe, *GetRenderFrameView(fixture.m_Frame));
    ASSERT_EQ(34u, fixture.m_Probe.m_ConsumedValue);
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ResetRenderFrame(fixture.m_Frame);
    ASSERT_EQ(2u, fixture.m_Probe.m_DestroyedCount);
    ASSERT_EQ(34u, fixture.m_Probe.m_DestroyedValues[0]);
    ASSERT_EQ(21u, fixture.m_Probe.m_DestroyedValues[1]);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

static bool CreateResourceMutationQueue(ResourceCaptureFixture* fixture)
{
    RenderFrameQueueParams params = {};
    params.m_Execution = fixture->m_Execution;
    params.m_Limits.m_PayloadBytes = 64;
    params.m_Limits.m_MaxTables = 2;
    params.m_Limits.m_MaxReferences = 2;
    params.m_Consume = ConsumeCapturedResource;
    params.m_ConsumeContext = &fixture->m_Probe;
    fixture->m_Probe.m_MutationQueue = NewRenderFrameQueue(params);
    return fixture->m_Probe.m_MutationQueue != 0;
}

static void CaptureParentAndDependency(ResourceCaptureFixture* fixture, CapturedResource* parent, RenderFrameTicket* ticket)
{
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(fixture->m_Probe.m_MutationQueue, ticket));
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture->m_Factory, ticket->m_Frame, parent));
    ASSERT_EQ(RENDER_FRAME_OK, RetainComponentRenderResource(fixture->m_Factory, ticket->m_Frame, parent->m_Dependency));
    uint32_t table;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(ticket->m_Frame, 17, 1, sizeof(parent), sizeof(parent), &parent, &table, 0));
}

// The factory guard retires an already-consumed inline frame before recreating a
// child resource, so old captured readers and their pins cannot outlive the change.
TEST(ComponentRenderResource, InlineDependencyReloadRetiresAcceptedFrame)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 2));
    ASSERT_TRUE(CreateResourceMutationQueue(&fixture));
    CapturedResourceSource child_source = { 0, 21 };
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(fixture.m_Factory, "/child.capture", sizeof(child_source), &child_source));
    CapturedResource* parent = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/parent.capture", 34, "/child.capture", &parent));

    RenderFrameTicket ticket;
    CaptureParentAndDependency(&fixture, parent, &ticket);
    HRenderFrameQueue queue = fixture.m_Probe.m_MutationQueue;
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(queue, ticket));
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ASSERT_EQ(0u, GetRenderFrameQueueStatus(queue).m_Retired);
    ASSERT_EQ(2u, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);

    child_source.m_Value = 99;
    HResourceDescriptor descriptor = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(fixture.m_Factory, "/child.capture", &descriptor));
    ASSERT_NE((HResourceDescriptor)0, descriptor);
    ASSERT_EQ(1u, GetRenderFrameQueueStatus(queue).m_Retired);
    ASSERT_EQ(0u, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);
    ASSERT_EQ(99u, parent->m_Dependency->m_Value);
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ASSERT_EQ(1u, fixture.m_Probe.m_RecreateCount);
    ASSERT_EQ(1u, fixture.m_Probe.m_EndCount);
    ASSERT_FALSE(GetRenderFrameQueueStatus(queue).m_MutationPending);

    dmResource::Release(fixture.m_Factory, parent);
    ASSERT_EQ(2u, fixture.m_Probe.m_DestroyedCount);
    DeleteResourceCaptureFixture(&fixture);
}

// A reload attempted before a native owner consumes an accepted parent/child
// snapshot is refused unchanged; explicitly retrying after retirement succeeds.
TEST(ComponentRenderResource, ThreadedDependencyReloadWaitsForCapturedReaders)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (!capabilities.m_BuildEnabled || !capabilities.m_ThreadsSupported)
    {
        return;
    }
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_THREADED, 2));
    ASSERT_TRUE(CreateResourceMutationQueue(&fixture));
    CapturedResourceSource child_source = { 0, 21 };
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(fixture.m_Factory, "/child.capture", sizeof(child_source), &child_source));
    CapturedResource* parent = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/parent.capture", 34, "/child.capture", &parent));
    HRenderFrameQueue queue = fixture.m_Probe.m_MutationQueue;
    RenderFrameTicket ticket;
    CaptureParentAndDependency(&fixture, parent, &ticket);
    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(queue, ticket));

    child_source.m_Value = 99;
    HResourceDescriptor descriptor = 0;
    ASSERT_EQ(dmResource::RESULT_INVAL, dmResource::ReloadResource(fixture.m_Factory, "/child.capture", &descriptor));
    ASSERT_EQ(RENDER_FRAME_WOULD_BLOCK, fixture.m_Probe.m_LastBeginResult);
    ASSERT_EQ((HResourceDescriptor)0, descriptor);
    ASSERT_EQ(21u, parent->m_Dependency->m_Value);
    ASSERT_EQ(0u, fixture.m_Probe.m_RecreateCount);
    ASSERT_EQ(0u, fixture.m_Probe.m_EndCount);
    ASSERT_FALSE(GetRenderFrameQueueStatus(queue).m_MutationPending);

    ResourceCaptureExecutor executor = {};
    executor.m_Queue = queue;
    executor.m_Execution = fixture.m_Execution;
    dmThread::Thread thread = dmThread::New(ConsumeOnRenderThread, 0x80000, &executor, "resource-consumer");
    dmThread::Join(thread);
    ASSERT_TRUE(executor.m_Bound);
    ASSERT_EQ(RENDER_FRAME_PUMP_CONSUMED, executor.m_PumpResult);
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ASSERT_EQ(1u, RetireRenderFrames(queue));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(fixture.m_Factory, "/child.capture", &descriptor));
    ASSERT_EQ(99u, parent->m_Dependency->m_Value);
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ASSERT_EQ(2u, fixture.m_Probe.m_BeginCount);
    ASSERT_EQ(1u, fixture.m_Probe.m_RecreateCount);
    ASSERT_EQ(1u, fixture.m_Probe.m_EndCount);
    ASSERT_FALSE(fixture.m_Probe.m_WrongConsumeOwner);
    dmResource::Release(fixture.m_Factory, parent);
    ASSERT_FALSE(fixture.m_Probe.m_WrongDestroyOwner);
    DeleteResourceCaptureFixture(&fixture);
}

// A resource callback failure must still close the factory mutation scope and
// reopen frame admission; the unchanged resource remains available for retry.
TEST(ComponentRenderResource, FailedRecreateReopensFrameAdmission)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 2));
    ASSERT_TRUE(CreateResourceMutationQueue(&fixture));
    CapturedResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/value.capture", 17, 0, &resource));
    CapturedResourceSource replacement = { 0, 44 };
    fixture.m_Probe.m_FailRecreate = true;
    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::SetResource(fixture.m_Factory, dmHashString64("/value.capture"), &replacement, sizeof(replacement)));
    ASSERT_EQ(17u, resource->m_Value);
    ASSERT_EQ(1u, fixture.m_Probe.m_EndCount);
    HRenderFrameQueue queue = fixture.m_Probe.m_MutationQueue;
    ASSERT_FALSE(GetRenderFrameQueueStatus(queue).m_MutationPending);

    RenderFrameTicket ticket;
    ASSERT_EQ(RENDER_FRAME_OK, BeginRenderFrameCapture(queue, &ticket));
    ASSERT_EQ(RENDER_FRAME_OK, CancelRenderFrameCapture(queue, ticket));
    fixture.m_Probe.m_FailRecreate = false;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(fixture.m_Factory, dmHashString64("/value.capture"), &replacement, sizeof(replacement)));
    ASSERT_EQ(44u, resource->m_Value);
    ASSERT_EQ(2u, fixture.m_Probe.m_EndCount);
    dmResource::Release(fixture.m_Factory, resource);
    DeleteResourceCaptureFixture(&fixture);
}

// A populated Building frame cannot wait for itself. Refusing a conflicting
// dependency reload must preserve that capture and its pins, not cancel the frame.
TEST(ComponentRenderResource, BuildingCaptureRejectsMutationWithoutCancellation)
{
    ResourceCaptureFixture fixture;
    ASSERT_TRUE(CreateResourceCaptureFixture(&fixture, EXECUTION_MODE_INLINE, 2));
    ASSERT_TRUE(CreateResourceMutationQueue(&fixture));
    CapturedResourceSource child_source = { 0, 21 };
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(fixture.m_Factory, "/child.capture", sizeof(child_source), &child_source));
    CapturedResource* parent = 0;
    ASSERT_EQ(dmResource::RESULT_OK, CreateTestResource(&fixture, "/parent.capture", 34, "/child.capture", &parent));
    HRenderFrameQueue queue = fixture.m_Probe.m_MutationQueue;
    RenderFrameTicket ticket;
    CaptureParentAndDependency(&fixture, parent, &ticket);

    child_source.m_Value = 99;
    HResourceDescriptor descriptor = 0;
    ASSERT_EQ(dmResource::RESULT_INVAL, dmResource::ReloadResource(fixture.m_Factory, "/child.capture", &descriptor));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, fixture.m_Probe.m_LastBeginResult);
    ASSERT_EQ(0u, fixture.m_Probe.m_RecreateCount);
    ASSERT_EQ(21u, parent->m_Dependency->m_Value);
    ASSERT_EQ(2u, GetRenderFrameStatistics(ticket.m_Frame).m_ReferenceCount);
    ASSERT_EQ(0u, GetRenderFrameQueueStatus(queue).m_Cancelled);
    ASSERT_FALSE(GetRenderFrameQueueStatus(queue).m_MutationPending);

    ASSERT_EQ(RENDER_FRAME_OK, PublishRenderFrame(queue, ticket));
    ASSERT_EQ(21u, fixture.m_Probe.m_ConsumedDependency);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(fixture.m_Factory, "/child.capture", &descriptor));
    ASSERT_EQ(99u, parent->m_Dependency->m_Value);
    ASSERT_EQ(0u, GetRenderFrameQueueStatus(queue).m_Cancelled);
    dmResource::Release(fixture.m_Factory, parent);
    DeleteResourceCaptureFixture(&fixture);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
