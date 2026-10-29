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

#include "render.h"
#include "render_private.h"
#include <algorithm>
#include <dlib/math.h>
#include <dmsdk/dlib/static_assert.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <graphics/graphics.h>

DM_PROPERTY_EXTERN(rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectDraws, 0, PROFILE_PROPERTY_FRAME_RESET, "# GPU indirect draw submissions", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectCandidates, 0, PROFILE_PROPERTY_FRAME_RESET, "# model candidates submitted to GPU draws", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectVisible, 0, PROFILE_PROPERTY_FRAME_RESET, "# visible model instances from completed GPU snapshots", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectCulled, 0, PROFILE_PROPERTY_FRAME_RESET, "# culled model instances from completed GPU snapshots", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectStatsReady, 0, PROFILE_PROPERTY_FRAME_RESET, "# draws with completed asynchronous visibility statistics", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectPasses, 0, PROFILE_PROPERTY_FRAME_RESET, "# model compaction dispatches", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuInstanceOutputCapacityBytes, 0, PROFILE_PROPERTY_FRAME_RESET, "# visible instance output buffer bytes, summed per submitted draw", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuInstanceUploadedBytes, 0, PROFILE_PROPERTY_FRAME_RESET, "# persistent model matrix and membership bytes uploaded", &rmtp_Render);

DM_PROPERTY_U32(rmtp_GpuIndirectStatsCandidates, 0, PROFILE_PROPERTY_FRAME_RESET, "# candidates paired with completed GPU snapshots", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectStatsAge, 0, PROFILE_PROPERTY_FRAME_RESET, "Age in frames of the last completed GPU snapshot", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuIndirectStatsRequests, 0, PROFILE_PROPERTY_FRAME_RESET, "# asynchronous visibility statistics copies requested", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuBoundsUploadedBytes, 0, PROFILE_PROPERTY_FRAME_RESET, "# persistent culling bounds bytes uploaded", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuBoundsUploads, 0, PROFILE_PROPERTY_FRAME_RESET, "# persistent culling bounds upload ranges", &rmtp_Render);
DM_PROPERTY_U32(rmtp_GpuBufferResizes, 0, PROFILE_PROPERTY_FRAME_RESET, "# persistent GPU buffer allocations and capacity/layout changes", &rmtp_Render);

namespace dmRender
{
    using namespace dmVMath;

    DM_STATIC_ASSERT(sizeof(GpuCullingBounds) == 8 * sizeof(uint32_t), Invalid_GpuCullingBounds_Size);
    DM_STATIC_ASSERT(BOUNDING_VOLUME_NONE == 0 && BOUNDING_VOLUME_SPHERE == 1 &&
                     BOUNDING_VOLUME_AABB == 2 && BOUNDING_VOLUME_OBB == 3,
                     Invalid_GpuCullingBounds_Types);

    void InitializeGpuIndirect(HRenderContext context, const RenderContextParams& params)
    {
        GpuIndirectContext& gpu = context->m_GpuIndirect;
        gpu.m_Program = 0;
        gpu.m_WorldBuffer = 0;
        gpu.m_Revision = 1;
        gpu.m_SlotRevision = 1;
        gpu.m_BatchRevision = 1;
        gpu.m_DrawCursor = 0;
        gpu.m_BatchCursor = 0;
        gpu.m_DrawCache = 0;
        gpu.m_Frame = 0;
        context->m_GpuCulling.m_BoundsBuffer = 0;
        gpu.m_DrawEnabled = 0;
        gpu.m_StatisticsEnabled = params.m_GpuCullingStatistics;
        gpu.m_PlanesLocation = dmGraphics::INVALID_UNIFORM_LOCATION;
        gpu.m_ParamsLocation = dmGraphics::INVALID_UNIFORM_LOCATION;
        gpu.m_InstanceParamsLocation = dmGraphics::INVALID_UNIFORM_LOCATION;
        gpu.m_ViewLocation = dmGraphics::INVALID_UNIFORM_LOCATION;
        if (!params.m_InstanceCullingShaderProgramDesc ||
            !dmGraphics::IsContextFeatureSupported(context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_COMPUTE_SHADER) ||
            !dmGraphics::IsContextFeatureSupported(context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_STORAGE_BUFFER) ||
            !dmGraphics::IsContextFeatureSupported(context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_INDIRECT_DRAW))
            return;
        // The built-in culling program has only a compute stage.
        dmGraphics::GraphicsContextLimits limits;
        dmGraphics::GetGraphicsContextLimits(context->m_GraphicsContext, limits);
        if (params.m_InstanceCullingShaderProgramDesc->m_Reflection.m_StorageBuffers.m_Count > limits.m_MaxStorageBuffersPerStage)
            return;
        char error[512] = {};
        gpu.m_Program = dmGraphics::NewProgram(context->m_GraphicsContext, params.m_InstanceCullingShaderProgramDesc, error, sizeof(error));
        if (!gpu.m_Program)
        {
            dmLogWarning("Model GPU compaction unavailable: %s", error);
            return;
        }
        for (uint32_t i = 0; i < dmGraphics::GetUniformCount(gpu.m_Program); ++i)
        {
            dmGraphics::Uniform uniform;
            dmGraphics::GetUniform(gpu.m_Program, i, &uniform);
            if (uniform.m_NameHash == dmHashString64("frustum_planes"))
                gpu.m_PlanesLocation = uniform.m_Location;
            else if (uniform.m_NameHash == dmHashString64("culling_params"))
                gpu.m_ParamsLocation = uniform.m_Location;
            else if (uniform.m_NameHash == dmHashString64("instance_params"))
                gpu.m_InstanceParamsLocation = uniform.m_Location;
            else if (uniform.m_NameHash == dmHashString64("view"))
                gpu.m_ViewLocation = uniform.m_Location;
        }
        assert(gpu.m_PlanesLocation != dmGraphics::INVALID_UNIFORM_LOCATION);
        assert(gpu.m_ParamsLocation != dmGraphics::INVALID_UNIFORM_LOCATION);
        assert(gpu.m_InstanceParamsLocation != dmGraphics::INVALID_UNIFORM_LOCATION);
        assert(gpu.m_ViewLocation != dmGraphics::INVALID_UNIFORM_LOCATION);
        dmLogInfo("GPU model compaction and indirect draws enabled; other items use CPU culling.");
    }

    static void DeleteGpuRenderBatch(HRenderContext context, HGpuRenderBatch batch);

    void FinalizeGpuIndirect(HRenderContext context)
    {
        GpuIndirectContext& gpu = context->m_GpuIndirect;
        for (uint32_t i = 0; i < gpu.m_Batches.Size(); ++i)
            DeleteGpuRenderBatch(context, gpu.m_Batches[i]);
        for (uint32_t i = 0; i < gpu.m_DrawCaches.Size(); ++i)
            delete gpu.m_DrawCaches[i];
        if (context->m_GpuCulling.m_BoundsBuffer)
            dmGraphics::DeleteStorageBuffer(context->m_GraphicsContext, context->m_GpuCulling.m_BoundsBuffer);
        if (gpu.m_WorldBuffer)
            dmGraphics::DeleteStorageBuffer(context->m_GraphicsContext, gpu.m_WorldBuffer);
        if (gpu.m_Program)
            dmGraphics::DeleteProgram(context->m_GraphicsContext, gpu.m_Program);
    }

    bool IsGpuIndirectSupported(HRenderContext context)
    {
        return context->m_GpuIndirect.m_Program != 0;
    }

    void MarkGpuWorldDirty(HRenderContext context, uint32_t slot)
    {
        PersistentRenderItem& item = context->m_PersistentRenderItems[slot];
        if (!context->m_GpuIndirect.m_Program || !item.m_GpuDriven || item.m_GpuWorldDirty)
            return;
        dmArray<uint32_t>& dirty = context->m_GpuIndirect.m_DirtyWorldSlots;
        if (dirty.Full())
            dirty.OffsetCapacity(dmMath::Max(256u, dirty.Capacity() / 2));
        dirty.Push(slot);
        item.m_GpuWorldDirty = 1;
        ++context->m_GpuIndirect.m_Revision;
    }

    void SetRenderItemGpuDriven(HRenderContext context, HRenderItem handle, bool eligible)
    {
        uint32_t              slot = (uint32_t)handle;
        PersistentRenderItem& item = context->m_PersistentRenderItems[slot];
        assert(item.m_Alive && item.m_Generation == (uint32_t)(handle >> 32));
        eligible &= IsGpuIndirectSupported(context);
        if (item.m_GpuDriven == eligible)
            return;
        GpuIndirectContext& gpu = context->m_GpuIndirect;
        if (eligible)
        {
            if (gpu.m_RenderSlots.Full())
                gpu.m_RenderSlots.OffsetCapacity(dmMath::Max(256u, gpu.m_RenderSlots.Capacity() / 2));
            item.m_GpuSlot = gpu.m_RenderSlots.Size();
            gpu.m_RenderSlots.Push(slot);
        }
        else
        {
            uint32_t moved = gpu.m_RenderSlots.Back();
            gpu.m_RenderSlots[item.m_GpuSlot] = moved;
            gpu.m_RenderSlots.SetSize(gpu.m_RenderSlots.Size() - 1);
            if (moved != slot)
            {
                context->m_PersistentRenderItems[moved].m_GpuSlot = item.m_GpuSlot;
                MarkGpuBoundsDirty(context, moved);
                MarkGpuWorldDirty(context, moved);
            }
            item.m_GpuSlot = ~0u;
        }
        item.m_GpuDriven = eligible;
        if (eligible)
        {
            MarkGpuBoundsDirty(context, slot);
            MarkGpuWorldDirty(context, slot);
        }
        ++gpu.m_SlotRevision;
        context->m_RenderItemDispatches[item.m_Dispatch]->m_GpuBatchRevision = ++gpu.m_BatchRevision;
        ++gpu.m_Revision;
        context->m_RenderList[slot].m_FrustumHash = FRUSTUM_HASH_UNINITIALIZED;
    }

    bool IsRenderItemGpuDriven(HRenderContext context, HRenderItem handle)
    {
        const PersistentRenderItem& item = context->m_PersistentRenderItems[(uint32_t)handle];
        return context->m_GpuIndirect.m_DrawEnabled && item.m_GpuDriven && item.m_BoundingVolumeType != BOUNDING_VOLUME_NONE;
    }

    static HGpuRenderBatch NewGpuRenderBatch()
    {
        GpuRenderBatch* batch = new GpuRenderBatch;
        batch->m_FallbackBuffer = 0;
        batch->m_ListCache = 0;
        batch->m_CachedSlots = 0;
        batch->m_ListGeneration = 0;
        batch->m_SlotBuffer = 0;
        batch->m_InstanceBuffer = 0;
        batch->m_VisibleInstanceProgram = 0;
        batch->m_ArgumentBuffer = 0;
        batch->m_Readback = 0;
        batch->m_StatisticsCandidates = 0;
        batch->m_StatisticsVisible = 0;
        batch->m_StatisticsFrame = 0;
        batch->m_SlotRevision = 0;
        batch->m_Computed = 0;
        batch->m_StatisticsDirty = 0;
        return batch;
    }

    static void DeleteGpuRenderBatch(HRenderContext context, HGpuRenderBatch batch)
    {
        if (batch->m_FallbackBuffer)
            dmGraphics::DeleteVertexBuffer(batch->m_FallbackBuffer);
        if (batch->m_SlotBuffer)
            dmGraphics::DeleteStorageBuffer(context->m_GraphicsContext, batch->m_SlotBuffer);
        if (batch->m_InstanceBuffer)
            dmGraphics::DeleteStorageBuffer(context->m_GraphicsContext, batch->m_InstanceBuffer);
        if (batch->m_ArgumentBuffer)
            dmGraphics::DeleteStorageBuffer(context->m_GraphicsContext, batch->m_ArgumentBuffer);
        if (batch->m_Readback)
            dmGraphics::DeleteStorageBufferReadback(batch->m_Readback);
        delete batch;
    }

    void MarkGpuBoundsDirty(HRenderContext context, uint32_t slot)
    {
        PersistentRenderItem& item = context->m_PersistentRenderItems[slot];
        if (!context->m_GpuIndirect.m_Program || !item.m_GpuDriven || item.m_GpuBoundsDirty)
            return;
        dmArray<uint32_t>& dirty = context->m_GpuCulling.m_DirtySlots;
        if (dirty.Full())
            dirty.OffsetCapacity(dmMath::Max(256u, dirty.Capacity() / 2));
        dirty.Push(slot);
        item.m_GpuBoundsDirty = 1;
        ++context->m_GpuIndirect.m_Revision;
    }

    static void PackBounds(const PersistentRenderItem& item, GpuCullingBounds* bounds)
    {
        memset(bounds, 0, sizeof(*bounds));
        if (!item.m_Alive || !item.m_Enabled || !item.m_GpuDriven)
            return;
        bounds->m_Type = item.m_BoundingVolumeType;
        if (item.m_BoundingVolumeType == BOUNDING_VOLUME_SPHERE)
        {
            memcpy(bounds->m_Data, item.m_Bounds, 4 * sizeof(float));
            return;
        }
        if (item.m_BoundingVolumeType == BOUNDING_VOLUME_NONE)
            return;
        const float* b = item.m_Bounds;
        Vector3      min(b[0], b[1], b[2]);
        Vector3      max(b[3], b[4], b[5]);
        Point3       center((min + max) * 0.5f);
        Vector3      extents = (max - min) * 0.5f;
        for (uint32_t i = 0; i < 3; ++i)
        {
            bounds->m_Data[i] = center[i];
            bounds->m_Data[3 + i] = extents[i];
        }
    }

    static bool UseDirtySpan(const dmArray<uint32_t>& dirty)
    {
        // Scattered updates otherwise allocate a staging buffer and encoder for
        // every small range. Bound this CPU cost by copying the covering span.
        uint32_t ranges = 1;
        for (uint32_t i = 1; i < dirty.Size(); ++i)
        {
            if (dirty[i] > dirty[i - 1] + 2 && ++ranges > 64)
                return true;
        }
        return false;
    }

    static void UploadBoundsRange(HRenderContext context, uint32_t first, uint32_t count)
    {
        GpuCullingContext& gpu = context->m_GpuCulling;
        uint32_t           bytes = count * sizeof(GpuCullingBounds);
        dmGraphics::SetStorageBufferSubData(context->m_GraphicsContext, gpu.m_BoundsBuffer, first * sizeof(GpuCullingBounds), bytes, gpu.m_Bounds.Begin() + first);
        DM_PROPERTY_ADD_U32(rmtp_GpuBoundsUploadedBytes, bytes);
        DM_PROPERTY_ADD_U32(rmtp_GpuBoundsUploads, 1);
    }

    // Keep the previous allocation usable until its replacement succeeds.
    static bool ResizeStorageBuffer(HRenderContext context, dmGraphics::HStorageBuffer* buffer, uint64_t size, bool replace = false)
    {
        if (size > UINT32_MAX)
            return false;
        dmGraphics::HContext graphics = context->m_GraphicsContext;
        if (*buffer && !replace && dmGraphics::GetStorageBufferSize(graphics, *buffer) >= size)
            return true;
        dmGraphics::HStorageBuffer replacement = dmGraphics::NewStorageBuffer(graphics, (uint32_t)size, 0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        if (!replacement)
            return false;
        if (*buffer)
            dmGraphics::DeleteStorageBuffer(graphics, *buffer);
        *buffer = replacement;
        DM_PROPERTY_ADD_U32(rmtp_GpuBufferResizes, 1);
        return true;
    }

    static Result AddToRenderCpuBatch(HRenderContext context, RenderObject* object, HGpuRenderBatch batch, const uint32_t* slots, uint32_t count, uint32_t stride, int32_t world_offset, int32_t normal_offset)
    {
        // GPU classification has already bypassed CPU visibility and packing.
        // Recover both here if a storage-buffer limit or allocation prevents compaction.
        batch->m_Computed = 0;
        batch->m_SlotRevision = 0;
        batch->m_VisibleInstanceProgram = 0;
        if ((uint64_t)count * stride > UINT32_MAX)
            return RESULT_OUT_OF_RESOURCES;
        dmArray<uint8_t> instances;
        instances.SetCapacity(count * stride);
        instances.SetSize(count * stride);
        uint32_t visible = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            const PersistentRenderItem& item = context->m_PersistentRenderItems[slots[i]];
            if (context->m_GpuIndirect.m_Frustum.m_NumPlanes && !TestRenderItemBoundingVolume(context->m_GpuIndirect.m_Frustum, item))
                continue;
            uint8_t* instance = instances.Begin() + visible++ * stride;
            memset(instance, 0, stride);
            if (world_offset >= 0)
                memcpy(instance + world_offset, &item.m_WorldTransform, sizeof(Matrix4));
            if (normal_offset >= 0)
            {
                Matrix4 normal = GetNormalMatrix(context, item.m_WorldTransform);
                memcpy(instance + normal_offset, &normal, sizeof(Matrix4));
            }
        }
        if (!visible)
            return RESULT_OK;
        if (!batch->m_FallbackBuffer)
            batch->m_FallbackBuffer = dmGraphics::NewVertexBuffer(context->m_GraphicsContext, visible * stride, instances.Begin(), dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        else
            dmGraphics::SetVertexBufferData(batch->m_FallbackBuffer, visible * stride, instances.Begin(), dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        if (!batch->m_FallbackBuffer)
            return RESULT_OUT_OF_RESOURCES;
        object->m_VertexBuffers[1] = batch->m_FallbackBuffer;
        object->m_VertexBufferOffsets[1] = 0;
        object->m_InstanceCount = visible;
        return AddToRender(context, object);
    }

    static bool UploadGpuCullingBounds(HRenderContext context)
    {
        GpuCullingContext& gpu = context->m_GpuCulling;
        uint32_t           slots = context->m_GpuIndirect.m_RenderSlots.Size();
        if (gpu.m_Bounds.Capacity() < context->m_GpuIndirect.m_RenderSlots.Capacity())
            gpu.m_Bounds.SetCapacity(context->m_GpuIndirect.m_RenderSlots.Capacity());
        gpu.m_Bounds.SetSize(slots);
        dmGraphics::HContext graphics = context->m_GraphicsContext;
        uint64_t             capacity = (uint64_t)gpu.m_Bounds.Capacity() * sizeof(GpuCullingBounds);
        bool                 resized = !gpu.m_BoundsBuffer || dmGraphics::GetStorageBufferSize(graphics, gpu.m_BoundsBuffer) < capacity;
        if (!ResizeStorageBuffer(context, &gpu.m_BoundsBuffer, capacity))
            return false;
        uint32_t dirty_count = 0;
        for (uint32_t i = 0; i < gpu.m_DirtySlots.Size(); ++i)
        {
            PersistentRenderItem& item = context->m_PersistentRenderItems[gpu.m_DirtySlots[i]];
            item.m_GpuBoundsDirty = 0;
            if (item.m_GpuSlot == ~0u)
                continue;
            PackBounds(item, &gpu.m_Bounds[item.m_GpuSlot]);
            gpu.m_DirtySlots[dirty_count++] = item.m_GpuSlot;
        }
        gpu.m_DirtySlots.SetSize(dirty_count);
        if (resized || gpu.m_DirtySlots.Size() > slots / 2)
            UploadBoundsRange(context, 0, slots);
        else
        {
            if (gpu.m_DirtySlots.Size() > 1)
                std::sort(gpu.m_DirtySlots.Begin(), gpu.m_DirtySlots.End());
            if (UseDirtySpan(gpu.m_DirtySlots))
            {
                uint32_t first = gpu.m_DirtySlots[0];
                UploadBoundsRange(context, first, gpu.m_DirtySlots.Back() - first + 1);
            }
            else
            {
                for (uint32_t i = 0; i < gpu.m_DirtySlots.Size();)
                {
                    uint32_t first = gpu.m_DirtySlots[i++];
                    uint32_t end = first + 1;
                    // Merge a one-record gap to bound small-upload overhead.
                    while (i < gpu.m_DirtySlots.Size() && gpu.m_DirtySlots[i] <= end + 1)
                        end = gpu.m_DirtySlots[i++] + 1;
                    UploadBoundsRange(context, first, end - first);
                }
            }
        }
        gpu.m_DirtySlots.SetSize(0);
        return true;
    }

    static bool UploadWorlds(HRenderContext context)
    {
        DM_PROFILE("GpuInstanceUpload");
        GpuIndirectContext&  gpu = context->m_GpuIndirect;
        dmGraphics::HContext graphics = context->m_GraphicsContext;
        uint32_t             slots = gpu.m_RenderSlots.Size();
        if (gpu.m_Worlds.Capacity() < gpu.m_RenderSlots.Capacity())
            gpu.m_Worlds.SetCapacity(gpu.m_RenderSlots.Capacity());
        gpu.m_Worlds.SetSize(slots);
        uint64_t capacity = (uint64_t)gpu.m_Worlds.Capacity() * sizeof(Matrix4);
        bool     resized = !gpu.m_WorldBuffer || dmGraphics::GetStorageBufferSize(graphics, gpu.m_WorldBuffer) < capacity;
        if (!ResizeStorageBuffer(context, &gpu.m_WorldBuffer, capacity))
            return false;
        uint32_t dirty_count = 0;
        for (uint32_t i = 0; i < gpu.m_DirtyWorldSlots.Size(); ++i)
        {
            PersistentRenderItem& item = context->m_PersistentRenderItems[gpu.m_DirtyWorldSlots[i]];
            item.m_GpuWorldDirty = 0;
            if (item.m_GpuSlot == ~0u)
                continue;
            gpu.m_Worlds[item.m_GpuSlot] = item.m_WorldTransform;
            gpu.m_DirtyWorldSlots[dirty_count++] = item.m_GpuSlot;
        }
        gpu.m_DirtyWorldSlots.SetSize(dirty_count);
        dmArray<uint32_t>& dirty = gpu.m_DirtyWorldSlots;
        uint32_t           bytes = 0;
        if (resized || dirty.Size() > slots / 2)
        {
            bytes = slots * sizeof(Matrix4);
            dmGraphics::SetStorageBufferSubData(graphics, gpu.m_WorldBuffer, 0, bytes, gpu.m_Worlds.Begin());
        }
        else
        {
            if (dirty.Size() > 1)
                std::sort(dirty.Begin(), dirty.End());
            if (UseDirtySpan(dirty))
            {
                uint32_t first = dirty[0];
                bytes = (dirty.Back() - first + 1) * sizeof(Matrix4);
                dmGraphics::SetStorageBufferSubData(graphics, gpu.m_WorldBuffer, first * sizeof(Matrix4), bytes, gpu.m_Worlds.Begin() + first);
            }
            else
            {
                for (uint32_t i = 0; i < dirty.Size();)
                {
                    uint32_t first = dirty[i++];
                    uint32_t end = first + 1;
                    // Merge a one-record gap to bound small-upload overhead.
                    while (i < dirty.Size() && dirty[i] <= end + 1)
                        end = dirty[i++] + 1;
                    uint32_t size = (end - first) * sizeof(Matrix4);
                    dmGraphics::SetStorageBufferSubData(graphics, gpu.m_WorldBuffer, first * sizeof(Matrix4), size, gpu.m_Worlds.Begin() + first);
                    bytes += size;
                }
            }
        }
        dirty.SetSize(0);
        DM_PROPERTY_ADD_U32(rmtp_GpuInstanceUploadedBytes, bytes);
        return true;
    }

    Result AddToRender(HRenderContext context, RenderObject* object, const uint32_t* slots, uint32_t count, uint32_t stride, int32_t world_offset, int32_t normal_offset)
    {
        DM_PROFILE("GpuModelBatch");
        if (!context)
            return RESULT_INVALID_CONTEXT;
        assert(stride && stride % sizeof(float) == 0);
        if (count == 0)
            return RESULT_OK;
        if (context->m_RenderObjects.Full())
            return RESULT_OUT_OF_RESOURCES;
        GpuIndirectContext& gpu = context->m_GpuIndirect;
        // Each queued instance draw needs distinct outputs. Ordinary draws do
        // not advance this cursor, so CPU batching does not churn GPU caches.
        uint32_t index = gpu.m_BatchCursor;
        if (index == gpu.m_Batches.Size())
        {
            if (gpu.m_Batches.Full())
                gpu.m_Batches.OffsetCapacity(16);
            gpu.m_Batches.Push(NewGpuRenderBatch());
        }
        HGpuRenderBatch batch = gpu.m_Batches[index];
        dmGraphics::HContext graphics = context->m_GraphicsContext;
        uint32_t             instance_stride = stride;
        int32_t              instance_world_offset = world_offset;
        batch->m_VisibleInstanceProgram = 0;
        if (object->m_Material && world_offset >= 0 && normal_offset < 0)
            batch->m_VisibleInstanceProgram = GetMaterialVisibleInstanceProgram(object->m_Material);
        if (batch->m_VisibleInstanceProgram)
        {
            stride = sizeof(uint32_t);
            world_offset = -1;
        }
        if (batch->m_Readback)
        {
            uint32_t arguments[5];
            if (dmGraphics::PollStorageBufferReadback(batch->m_Readback, arguments))
            {
                batch->m_StatisticsCandidates = batch->m_ReadbackCandidates;
                batch->m_StatisticsVisible = arguments[1];
                batch->m_StatisticsFrame = batch->m_ReadbackFrame;
                dmGraphics::DeleteStorageBufferReadback(batch->m_Readback);
                batch->m_Readback = 0;
            }
        }
        bool cached_membership = gpu.m_DrawCache && batch->m_ListCache == gpu.m_DrawCache &&
        batch->m_ListGeneration == gpu.m_DrawCache->m_Generation && batch->m_CachedSlots == slots;
        bool membership_changed = batch->m_SlotRevision != gpu.m_SlotRevision || batch->m_Slots.Size() != count ||
        (!cached_membership && memcmp(batch->m_Slots.Begin(), slots, count * sizeof(uint32_t)));
        batch->m_ListCache = gpu.m_DrawCache;
        batch->m_ListGeneration = gpu.m_DrawCache ? gpu.m_DrawCache->m_Generation : 0;
        batch->m_CachedSlots = slots;
        bool layout_changed = !batch->m_Computed || batch->m_Stride != stride || batch->m_WorldOffset != world_offset || batch->m_NormalOffset != normal_offset;
        bool output_resized = !batch->m_InstanceBuffer || layout_changed || dmGraphics::GetStorageBufferSize(graphics, batch->m_InstanceBuffer) < (uint64_t)count * stride;
        if (!ResizeStorageBuffer(context, &batch->m_SlotBuffer, (uint64_t)count * sizeof(uint32_t)) ||
            !ResizeStorageBuffer(context, &batch->m_InstanceBuffer, (uint64_t)count * stride, layout_changed) ||
            !ResizeStorageBuffer(context, &batch->m_ArgumentBuffer, 5 * sizeof(uint32_t)) ||
            !UploadGpuCullingBounds(context) || !UploadWorlds(context))
        {
            uint32_t previous_count = context->m_RenderObjects.Size();
            Result result = AddToRenderCpuBatch(context, object, batch, slots, count, instance_stride, instance_world_offset, normal_offset);
            // An entirely culled or failed fallback has no queued buffer user.
            if (context->m_RenderObjects.Size() != previous_count)
                ++gpu.m_BatchCursor;
            return result;
        }
        layout_changed |= output_resized;
        if (membership_changed)
        {
            if (batch->m_Slots.Capacity() < count)
                batch->m_Slots.SetCapacity(count);
            batch->m_Slots.SetSize(count);
            memcpy(batch->m_Slots.Begin(), slots, count * sizeof(uint32_t));
            if (gpu.m_UploadSlots.Capacity() < count)
                gpu.m_UploadSlots.SetCapacity(count);
            gpu.m_UploadSlots.SetSize(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                uint32_t slot = context->m_PersistentRenderItems[slots[i]].m_GpuSlot;
                assert(slot != ~0u);
                gpu.m_UploadSlots[i] = slot;
            }
            dmGraphics::SetStorageBufferSubData(graphics, batch->m_SlotBuffer, 0, count * sizeof(uint32_t), gpu.m_UploadSlots.Begin());
            batch->m_SlotRevision = gpu.m_SlotRevision;
            DM_PROPERTY_ADD_U32(rmtp_GpuInstanceUploadedBytes, count * sizeof(uint32_t));
        }
        uint32_t first_index = object->m_VertexStart / (object->m_IndexType == dmGraphics::TYPE_UNSIGNED_INT ? 4u : 2u);
        bool     recompute = !batch->m_Computed || membership_changed || layout_changed || batch->m_Revision != gpu.m_Revision ||
        batch->m_Frustum.m_NumPlanes != gpu.m_Frustum.m_NumPlanes ||
        memcmp(batch->m_Frustum.m_Planes, gpu.m_Frustum.m_Planes, gpu.m_Frustum.m_NumPlanes * sizeof(Vector4)) != 0 || (normal_offset >= 0 && memcmp(&batch->m_View, &context->m_View, sizeof(Matrix4)) != 0) || batch->m_IndexCount != object->m_VertexCount || batch->m_FirstIndex != first_index;
        if (recompute)
        {
            DM_PROFILE("GpuModelCompaction");
            uint32_t arguments[5] = { object->m_VertexCount, 0, first_index, 0, 0 };
            dmGraphics::SetStorageBufferSubData(graphics, batch->m_ArgumentBuffer, 0, sizeof(arguments), arguments);
            dmGraphics::EnableProgram(graphics, gpu.m_Program);
            if (gpu.m_Frustum.m_NumPlanes)
                dmGraphics::SetConstantV4(graphics, gpu.m_Frustum.m_Planes, gpu.m_Frustum.m_NumPlanes, gpu.m_PlanesLocation);
            Vector4 params((float)count, (float)gpu.m_Frustum.m_NumPlanes, stride / 4.0f, world_offset / 4.0f);
            Vector4 instance_params(normal_offset / 4.0f, batch->m_VisibleInstanceProgram ? 1.0f : 0.0f, 0, 0);
            dmGraphics::SetConstantV4(graphics, &params, 1, gpu.m_ParamsLocation);
            dmGraphics::SetConstantV4(graphics, &instance_params, 1, gpu.m_InstanceParamsLocation);
            dmGraphics::SetConstantM4(graphics, (const Vector4*)&context->m_View, 1, gpu.m_ViewLocation);
            dmGraphics::HStorageBuffer buffers[] = { context->m_GpuCulling.m_BoundsBuffer, gpu.m_WorldBuffer, batch->m_SlotBuffer, batch->m_InstanceBuffer, batch->m_ArgumentBuffer };
            for (uint32_t i = 0; i < 5; ++i)
                dmGraphics::EnableStorageBuffer(graphics, buffers[i], 0, i);
            dmGraphics::DispatchCompute(graphics, (count + 127) / 128, 1, 1);
            for (uint32_t i = 0; i < 5; ++i)
                dmGraphics::DisableStorageBuffer(graphics, buffers[i]);
            dmGraphics::DisableProgram(graphics);
            batch->m_Frustum = gpu.m_Frustum;
            batch->m_View = context->m_View;
            batch->m_Revision = gpu.m_Revision;
            batch->m_Stride = stride;
            batch->m_WorldOffset = world_offset;
            batch->m_NormalOffset = normal_offset;
            batch->m_IndexCount = object->m_VertexCount;
            batch->m_FirstIndex = first_index;
            batch->m_Computed = 1;
            batch->m_StatisticsDirty = 1;
            DM_PROPERTY_ADD_U32(rmtp_GpuIndirectPasses, 1);
        }
        // Statistics are optional. Cached compaction keeps its previous snapshot
        // instead of splitting a render pass for another copy of the same count.
        // If a previous readback is pending, capture the newer result once ready.
        if (gpu.m_StatisticsEnabled && batch->m_StatisticsDirty && !batch->m_Readback)
        {
            batch->m_Readback = dmGraphics::RequestStorageBufferReadback(graphics, batch->m_ArgumentBuffer, 0, 5 * sizeof(uint32_t));
            batch->m_ReadbackCandidates = count;
            batch->m_ReadbackFrame = gpu.m_Frame;
            batch->m_StatisticsDirty = 0;
            DM_PROPERTY_ADD_U32(rmtp_GpuIndirectStatsRequests, 1);
        }
        object->m_VertexBuffers[1] = batch->m_VisibleInstanceProgram ? 0 : dmGraphics::GetStorageBufferVertexBuffer(graphics, batch->m_InstanceBuffer);
        if (batch->m_VisibleInstanceProgram)
            object->m_VertexDeclarations[1] = 0;
        DM_PROPERTY_ADD_U32(rmtp_GpuInstanceOutputCapacityBytes, dmGraphics::GetStorageBufferSize(graphics, batch->m_InstanceBuffer));
        object->m_VertexBufferOffsets[1] = 0;
        Result result = AddToRender(context, object);
        if (result == RESULT_OK)
        {
            gpu.m_RenderBatches.Back() = batch;
            ++gpu.m_BatchCursor;
        }
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectDraws, 1);
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectCandidates, count);
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectVisible, batch->m_StatisticsVisible);
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectCulled, batch->m_StatisticsCandidates - batch->m_StatisticsVisible);
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectStatsReady, batch->m_StatisticsCandidates != 0);
        DM_PROPERTY_ADD_U32(rmtp_GpuIndirectStatsCandidates, batch->m_StatisticsCandidates);
        if (batch->m_StatisticsCandidates)
            DM_PROPERTY_SET_U32(rmtp_GpuIndirectStatsAge, gpu.m_Frame - batch->m_StatisticsFrame);
        return result;
    }
} // namespace dmRender
