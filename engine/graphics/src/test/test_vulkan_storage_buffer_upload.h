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
// - Creation and full replacement keep shader storage in device-local memory.
//
// It also exercises uploads before rendering starts, zero-filling, several frames
// in flight, and swapchain recreation. This checks correctness and reuse; upload
// speed is measured by separate benchmarks.

#ifndef DM_TEST_VULKAN_STORAGE_BUFFER_UPLOAD_H
#define DM_TEST_VULKAN_STORAGE_BUFFER_UPLOAD_H

// Guards #13706: SSBO uploads preserve byte ranges and caller-data lifetime, use
// inline updates through 64 KiB, and reuse fenced staging storage for larger copies.
// Guards #13700: creation and full replacement retain device-local backing and
// preserve initialized/zero-filled contents without mapping the storage buffer.
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

    dmGraphics::HStorageBuffer         m_Buffer;
    dmGraphics::DeviceBufferAllocation m_Readback;
    uint32_t                           m_Frame;
    uint8_t                            m_Data[BUFFER_SIZE];
    uint8_t                            m_Expected[BUFFER_SIZE];

    bool CheckDeviceLocal(EngineCtx* engine)
    {
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        dmGraphics::DeviceBuffer* buffer = (dmGraphics::DeviceBuffer*) m_Buffer;
        if (!buffer)
        {
            dmLogError("Failed to create storage buffer.");
            engine->m_Failed = true;
            return false;
        }

        VkPhysicalDeviceMemoryProperties properties;
        vkGetPhysicalDeviceMemoryProperties(context->m_PhysicalDevice.m_Device, &properties);
        if (!(properties.memoryTypes[buffer->m_Allocation.m_MemoryTypeIndex].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            dmLogError("Storage buffer is not backed by device-local memory.");
            engine->m_Failed = true;
            return false;
        }

        return true;
    }

    void RecordReadback(VkCommandBuffer command_buffer)
    {
        dmGraphics::DeviceBuffer* buffer = (dmGraphics::DeviceBuffer*) m_Buffer;
        VkBufferMemoryBarrier barriers[2] = {};
        for (uint32_t i = 0; i < 2; ++i)
        {
            barriers[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[i].size = VK_WHOLE_SIZE;
        }

        barriers[0].buffer = buffer->m_Allocation.m_Handle.m_Buffer;
        barriers[0].srcAccessMask = buffer->m_StorageAccess;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        // The readback allocation is reused across submitted frames.
        barriers[1].buffer = m_Readback.m_Handle.m_Buffer;
        barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command_buffer, buffer->m_StorageStages | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 2, barriers, 0, 0);
        VkBufferCopy copy = { 0, 0, buffer->m_Base.m_Size };
        vkCmdCopyBuffer(command_buffer, buffer->m_Allocation.m_Handle.m_Buffer, m_Readback.m_Handle.m_Buffer, 1, &copy);
        barriers[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                             0, 0, 0, 1, &barriers[1], 0, 0);
        // Account for the test's read before the next production upload.
        buffer->m_StorageAccess = VK_ACCESS_TRANSFER_READ_BIT;
        buffer->m_StorageStages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    bool CheckReadback(EngineCtx* engine)
    {
        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        VkDevice device = context->m_LogicalDevice.m_Device;
        if (m_Readback.MapMemory(device) != VK_SUCCESS)
        {
            dmLogError("Failed to map SSBO readback buffer.");
            engine->m_Failed = true;
            return false;
        }

        const uint8_t* actual = (const uint8_t*) m_Readback.m_MappedDataPtr;
        uint32_t size = dmGraphics::GetStorageBufferSize(engine->m_GraphicsContext, m_Buffer);
        for (uint32_t i = 0; i < size; ++i)
        {
            if (actual[i] != m_Expected[i])
            {
                dmLogError("SSBO frame %u byte %u: expected %u, got %u", m_Frame, i, m_Expected[i], actual[i]);
                engine->m_Failed = true;
                break;
            }
        }

        m_Readback.UnmapMemory(device);
        return !engine->m_Failed;
    }

    bool VerifyOutsideFrame(EngineCtx* engine)
    {
        if (!CheckDeviceLocal(engine))
        {
            return false;
        }

        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        VkDevice device = context->m_LogicalDevice.m_Device;
        VkCommandBuffer command_buffer = dmGraphics::BeginSingleTimeCommands(device, context->m_LogicalDevice.m_CommandPool);
        RecordReadback(command_buffer);
        VkFence fence;
        VkResult result = dmGraphics::SubmitCommandBuffer(&context->m_LogicalDevice, command_buffer, &fence);
        if (result == VK_SUCCESS)
        {
            result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(device, fence, 0);
        }

        vkFreeCommandBuffers(device, context->m_LogicalDevice.m_CommandPool, 1, &command_buffer);
        if (result != VK_SUCCESS)
        {
            dmLogError("Failed to complete SSBO readback: %d", result);
            engine->m_Failed = true;
            return false;
        }

        return CheckReadback(engine);
    }

    void Initialize(EngineCtx* engine) override
    {
        m_Buffer = 0;
        m_Frame  = 0;

        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_VULKAN)
        {
            dmLogError("SSBO sub-data regression requires Vulkan");
            engine->m_Failed = true;
            return;
        }

        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        VkResult result = dmGraphics::CreateDeviceBufferAllocation(context->m_PhysicalDevice.m_Device,
                                                                   context->m_LogicalDevice.m_Device, BUFFER_SIZE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT, &m_Readback);
        if (result != VK_SUCCESS)
        {
            dmLogError("Failed to create SSBO readback buffer: %d", result);
            engine->m_Failed = true;
            return;
        }

        // Exercise the GPU-only creation path from #13700, then shrink and grow it
        // through full replacements. Usage hints must not change memory placement.
        m_Buffer = dmGraphics::NewStorageBuffer(engine->m_GraphicsContext, BUFFER_SIZE, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        if (!CheckDeviceLocal(engine))
        {
            return;
        }

        memset(m_Expected, 0x12, sizeof(m_Expected));
        dmGraphics::SetStorageBufferData(engine->m_GraphicsContext, m_Buffer, 4, m_Expected, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        if (dmGraphics::GetStorageBufferSize(engine->m_GraphicsContext, m_Buffer) != 4)
        {
            dmLogError("Full SSBO replacement did not shrink the logical size.");
            engine->m_Failed = true;
            return;
        }

        if (!VerifyOutsideFrame(engine))
        {
            return;
        }

        dmGraphics::SetStorageBufferData(engine->m_GraphicsContext, m_Buffer, BUFFER_SIZE, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        memset(m_Expected, 0, sizeof(m_Expected));
        if (dmGraphics::GetStorageBufferSize(engine->m_GraphicsContext, m_Buffer) != BUFFER_SIZE)
        {
            dmLogError("Full SSBO replacement did not grow the logical size.");
            engine->m_Failed = true;
            return;
        }

        if (!VerifyOutsideFrame(engine))
        {
            return;
        }

        dmGraphics::DeleteStorageBuffer(engine->m_GraphicsContext, m_Buffer);
        memset(m_Expected, 0x5a, sizeof(m_Expected));
        m_Buffer = dmGraphics::NewStorageBuffer(engine->m_GraphicsContext, BUFFER_SIZE, m_Expected, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        if (!VerifyOutsideFrame(engine))
        {
            return;
        }

        // Synchronous uploads reuse their own pool without touching in-flight frame pools.
        Upload(engine, 4, STAGED_UPLOAD_SIZE, 0x12, EXPECT_STAGED_UPLOAD);
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
            return;
        }

        // Verify synchronous upload contents before Execute replaces the buffer.
        VerifyOutsideFrame(engine);
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

        if (m_Frame < 3)
        {
            // Verify in-frame full uploads, zero-filling, and creation with data.
            // Overwrite the caller's bytes before the recorded transfer executes.
            memset(m_Data, 0x80 + m_Frame, sizeof(m_Data));
            memset(m_Expected, m_Frame == 1 ? 0 : 0x80 + m_Frame, sizeof(m_Expected));
            if (m_Frame == 2)
            {
                dmGraphics::DeleteStorageBuffer(graphics_context, m_Buffer);
                m_Buffer = dmGraphics::NewStorageBuffer(graphics_context, BUFFER_SIZE, m_Data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            }
            else
            {
                dmGraphics::SetStorageBufferData(graphics_context, m_Buffer, BUFFER_SIZE, m_Frame == 1 ? 0 : m_Data, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
            }

            memset(m_Data, 0xee, sizeof(m_Data));
            if (!CheckDeviceLocal(engine))
            {
                return;
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

        // Uploads suspend the render pass; copy to host-visible memory outside it.
        RecordReadback(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight]);
    }

    void AfterFlip(EngineCtx* engine) override
    {
        // Recreate the swapchain after the staging pools have been populated.
        if (m_Frame == 15 || m_Frame == 16)
        {
            dmGraphics::SetSwapInterval(engine->m_GraphicsContext, m_Frame == 16 ? 1 : 0);
        }

        // Check each full replacement, then submit several frames before waiting
        // so different staging pools stay in flight.
        ++m_Frame;
        if (m_Frame > 3 && m_Frame % dmGraphics::DM_MAX_FRAMES_IN_FLIGHT != 0 && !engine->m_Failed)
        {
            return;
        }

        dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
        VkDevice device = context->m_LogicalDevice.m_Device;
        if (vkDeviceWaitIdle(device) != VK_SUCCESS)
        {
            dmLogError("Failed to read back SSBO sub-data uploads");
            engine->m_Failed = true;
        }
        else if (!engine->m_Failed)
        {
            CheckReadback(engine);
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

        if (m_Readback.m_Handle.m_Buffer != VK_NULL_HANDLE)
        {
            dmGraphics::VulkanContext* context = (dmGraphics::VulkanContext*) engine->m_GraphicsContext;
            vkDeviceWaitIdle(context->m_LogicalDevice.m_Device);
            dmGraphics::DestroyDeviceBufferAllocation(context->m_LogicalDevice.m_Device, &m_Readback);
        }
    }

};

#endif
