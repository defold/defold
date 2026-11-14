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

// Checks that updates to GPU data buffers preserve their contents while reusing
// upload memory. It fills a buffer with known bytes, changes selected regions,
// then reads the buffer back after the GPU has finished.
//
// The test checks that:
// - Only the requested bytes change, and overlapping writes leave the latest value.
// - Small in-frame updates, including exactly 64 KiB, use the fast upload path.
// - Larger updates reuse temporary upload (staging) memory, even as the pool grows.
// - The caller can immediately overwrite its source data without corrupting uploads.
// - Repeated frames eventually reuse existing allocations instead of creating more.
//
// It also exercises uploads before rendering starts, zero-filling, several frames
// in flight, and swapchain recreation. This checks correctness and reuse; upload
// speed is measured by separate benchmarks.

#ifndef DM_TEST_VULKAN_STORAGE_BUFFER_UPLOAD_H
#define DM_TEST_VULKAN_STORAGE_BUFFER_UPLOAD_H

// Guards #13706: SSBO uploads preserve byte ranges and caller-data lifetime, use
// inline updates through 64 KiB, and reuse fenced staging storage for larger copies.
struct VulkanStorageBufferSubDataTest : ITest
{
    enum
    {
        BUFFER_SIZE         = 2 * 1024 * 1024 + 8,
        INLINE_UPLOAD_LIMIT = 64 * 1024,
        STAGED_UPLOAD_SIZE  = INLINE_UPLOAD_LIMIT + 4,
        WARMUP_FRAMES       = 30,
        TOTAL_FRAMES        = 60
    };
    enum UploadPath
    {
        EXPECT_INLINE_UPLOAD,
        EXPECT_STAGED_UPLOAD
    };

    enum UploadSource
    {
        SOURCE_DATA,
        SOURCE_ZERO
    };

    dmGraphics::HStorageBuffer m_Buffer = 0;
    uint32_t                   m_Frame = 0;
    uint8_t                    m_Data[BUFFER_SIZE];
    uint8_t                    m_Expected[BUFFER_SIZE];

    void Initialize(EngineCtx* engine) override
    {
        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
            dmLogError("SSBO sub-data regression requires Vulkan");
            engine->m_Failed = true;
            return;
        }
        memset(m_Expected, 0x5a, sizeof(m_Expected));
        m_Buffer = dmGraphics::NewStorageBuffer(engine->m_GraphicsContext, BUFFER_SIZE, m_Expected, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        // Synchronous uploads reuse their own pool without touching in-flight frame pools.
        Upload(engine, 4, STAGED_UPLOAD_SIZE, 0x12, EXPECT_STAGED_UPLOAD);
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        if (context->m_SynchronousStorageBufferUploadPool.m_Blocks.Empty())
        {
            dmLogError("Synchronous SSBO upload did not use the staging pool");
            engine->m_Failed = true;
            return;
        }
        VkBuffer staging = context->m_SynchronousStorageBufferUploadPool.m_Blocks[0].m_Allocation.m_Handle.m_Buffer;
        Upload(engine, 4, STAGED_UPLOAD_SIZE, 0x13, EXPECT_STAGED_UPLOAD);
        Upload(engine, BUFFER_SIZE - 4, 4, 0x11, EXPECT_STAGED_UPLOAD);
        if (context->m_SynchronousStorageBufferUploadPool.m_Blocks.Size() != 1 ||
            staging != context->m_SynchronousStorageBufferUploadPool.m_Blocks[0].m_Allocation.m_Handle.m_Buffer)
        {
            dmLogError("Synchronous SSBO uploads did not reuse staging storage");
            engine->m_Failed = true;
        }
    }

    void Upload(EngineCtx* engine, uint32_t offset, uint32_t size, uint8_t value, UploadPath expected_path, UploadSource source = SOURCE_DATA)
    {
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        uint32_t retired_before = context->m_MainResourcesToDestroy[context->m_CurrentFrameInFlight]->Size();
        bool zero = source == SOURCE_ZERO;
        memset(m_Data, value, size);
        dmGraphics::SetStorageBufferSubData(engine->m_GraphicsContext, m_Buffer, offset, size, zero ? 0 : m_Data);
        memset(m_Expected + offset, zero ? 0 : value, size);
        // The caller may immediately reuse its memory, even before command submission.
        memset(m_Data, 0xee, size);
        if (expected_path == EXPECT_INLINE_UPLOAD && retired_before != context->m_MainResourcesToDestroy[context->m_CurrentFrameInFlight]->Size())
        {
            dmLogError("Small SSBO sub-data upload allocated a staging buffer");
            engine->m_Failed = true;
        }
    }

    void Execute(EngineCtx* engine) override
    {
        dmGraphics::HContext graphics_context = engine->m_GraphicsContext;
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) graphics_context;
        dmGraphics::StorageBufferUploadPool* pool = &context->m_StorageBufferUploadPools[context->m_CurrentFrameInFlight];
        dmArray<VkBuffer> staging_before;
        staging_before.SetCapacity(pool->m_Blocks.Size());
        for (uint32_t i = 0; i < pool->m_Blocks.Size(); ++i)
        {
            staging_before.Push(pool->m_Blocks[i].m_Allocation.m_Handle.m_Buffer);
            if (pool->m_Blocks[i].m_Cursor != 0)
            {
                dmLogError("SSBO staging pool was not reset after the frame fence");
                engine->m_Failed = true;
            }
        }
        uint32_t retired_before = context->m_MainResourcesToDestroy[context->m_CurrentFrameInFlight]->Size();
        dmGraphics::Clear(graphics_context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1.0f, 0);
        switch (m_Frame % 5)
        {
            case 0:
            {
                // Repeated writes to the same range must retain command order.
                Upload(engine, 4, 4, 0x21, EXPECT_INLINE_UPLOAD);
                Upload(engine, 4, 4, 0x22, EXPECT_INLINE_UPLOAD);
                break;
            }
            case 1:
            {
                Upload(engine, 4, INLINE_UPLOAD_LIMIT, 0x33, EXPECT_INLINE_UPLOAD);
                break;
            }
            case 2:
            {
                Upload(engine, 4, STAGED_UPLOAD_SIZE, 0x44, EXPECT_STAGED_UPLOAD);
                Upload(engine, 4 + STAGED_UPLOAD_SIZE, STAGED_UPLOAD_SIZE, 0x45, EXPECT_STAGED_UPLOAD);
                break;
            }
            case 3:
            {
                Upload(engine, 8, 4, 0, EXPECT_STAGED_UPLOAD, SOURCE_ZERO);
                break;
            }
            case 4:
            {
                if ((m_Frame / 5) % 2 == 0)
                {
                    // Grow the pool mid-frame without invalidating earlier copy sources.
                    Upload(engine, 4, 512 * 1024, 0x61, EXPECT_STAGED_UPLOAD);
                    Upload(engine, 512 * 1024 + 4, 512 * 1024, 0x62, EXPECT_STAGED_UPLOAD);
                    Upload(engine, 1024 * 1024 + 4, 1024 * 1024, 0x63, EXPECT_STAGED_UPLOAD);
                }
                else
                {
                    // A larger single request may grow an unused block after its fence.
                    Upload(engine, 4, 1536 * 1024, 0x71, EXPECT_STAGED_UPLOAD);
                }
                break;
            }
        }
        if (retired_before != context->m_MainResourcesToDestroy[context->m_CurrentFrameInFlight]->Size())
        {
            dmLogError("SSBO upload retired staging storage instead of pooling it");
            engine->m_Failed = true;
        }
        // All frame slots have seen both large-upload patterns after 30 frames.
        if (m_Frame >= WARMUP_FRAMES)
        {
            if (staging_before.Size() != pool->m_Blocks.Size())
            {
                dmLogError("SSBO staging pool continued growing after warmup");
                engine->m_Failed = true;
            }
            for (uint32_t i = 0; i < staging_before.Size(); ++i)
            {
                if (staging_before[i] != pool->m_Blocks[i].m_Allocation.m_Handle.m_Buffer)
                {
                    dmLogError("SSBO staging buffer was replaced after warmup");
                    engine->m_Failed = true;
                }
            }
        }

        // Make transfer writes visible to the test's host read after GPU completion.
        // Each upload above suspends the render pass, so this barrier is outside it.
        dmGraphics::DeviceBuffer* buffer = (dmGraphics::DeviceBuffer*) m_Buffer;
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer->m_Allocation.m_Handle.m_Buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight],
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, 0, 1, &barrier, 0, 0);
    }

    void AfterFlip(EngineCtx* engine) override
    {
        // Recreate the swapchain after the staging pools have been populated.
        if (m_Frame == 15 || m_Frame == 16)
        {
            dmGraphics::SetSwapInterval(engine->m_GraphicsContext, m_Frame == 16 ? 1 : 0);
        }
        // Submit several frames before waiting, so different frame pools stay in flight.
        ++m_Frame;
        if (m_Frame % dmGraphics::DM_MAX_FRAMES_IN_FLIGHT != 0 && !engine->m_Failed)
        {
            return;
        }
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        dmGraphics::DeviceBuffer* buffer = (dmGraphics::DeviceBuffer*) m_Buffer;
        VkDevice device = context->m_LogicalDevice.m_Device;
        if (vkDeviceWaitIdle(device) != VK_SUCCESS || buffer->m_Allocation.MapMemory(device) != VK_SUCCESS)
        {
            dmLogError("Failed to read back SSBO sub-data uploads");
            engine->m_Failed = true;
        }
        else
        {
            const uint8_t* actual = (const uint8_t*) buffer->m_Allocation.m_MappedDataPtr;
            for (uint32_t i = 0; i < BUFFER_SIZE; ++i)
            {
                if (actual[i] != m_Expected[i])
                {
                    dmLogError("SSBO sub-data frame %u byte %u: expected %u, got %u", m_Frame, i, m_Expected[i], actual[i]);
                    engine->m_Failed = true;
                    break;
                }
            }
            buffer->m_Allocation.UnmapMemory(device);
        }
        // Repeat across frame slots to check reuse without timing assumptions.
        if (m_Frame == TOTAL_FRAMES || engine->m_Failed)
        {
            if (!engine->m_Failed)
            {
                dmLogInfo("SSBO sub-data: allocation and byte checks passed across %u frames", m_Frame);
            }
            engine->m_Running = 0;
        }
    }

    void OnGraphicsClosing(EngineCtx* engine) override
    {
        if (m_Buffer)
        {
            dmGraphics::DeleteStorageBuffer(engine->m_GraphicsContext, m_Buffer);
        }
    }
};

#endif
