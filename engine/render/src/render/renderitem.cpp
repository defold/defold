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

#include <assert.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "render_private.h"

DM_PROPERTY_EXTERN(rmtp_Render);
DM_PROPERTY_U32(rmtp_RenderDispatchCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# dispatch registrations", &rmtp_Render);

namespace dmRender
{
    using namespace dmVMath;

    static HRenderListDispatch AllocateRenderListDispatch(HRenderContext context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data)
    {
        if (context->m_RenderListDispatch.Full())
        {
            dmLogError("Exhausted number of render dispatches. Too many collections?");
            return RENDERLIST_INVALID_DISPATCH;
        }
        RenderListDispatch dispatch;
        dispatch.m_DispatchFn = dispatch_fn;
        dispatch.m_VisibilityFn = visibility_fn;
        dispatch.m_UserData = user_data;
        context->m_RenderListDispatch.Push(dispatch);
        DM_PROPERTY_ADD_U32(rmtp_RenderDispatchCount, 1);
        return context->m_RenderListDispatch.Size() - 1;
    }

    HRenderListDispatch RenderListMakeDispatch(HRenderContext context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data)
    {
        dmLogOnceWarning("RenderListMakeDispatch() is deprecated. Use RegisterRenderListDispatch() and SubmitRenderListDispatch().");
        return AllocateRenderListDispatch(context, dispatch_fn, visibility_fn, user_data);
    }

    HRenderListDispatch RenderListMakeDispatch(HRenderContext render_context, RenderListDispatchFn dispatch_fn, void* user_data)
    {
        return RenderListMakeDispatch(render_context, dispatch_fn, 0, user_data);
    }

    HRenderItemDispatch RegisterRenderListDispatch(HRenderContext context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data)
    {
        uint32_t index = 1;
        while (index < context->m_RenderItemDispatches.Size() && context->m_RenderItemDispatches[index])
            ++index;
        if (index == (1u << 24))
        {
            dmLogError("Exhausted render item dispatch registrations.");
            return 0;
        }
        if (index == context->m_RenderItemDispatches.Size())
        {
            if (context->m_RenderItemDispatches.Full())
                context->m_RenderItemDispatches.OffsetCapacity(dmMath::Max(8u, index / 2));
            context->m_RenderItemDispatches.SetSize(index + 1);
        }
        RenderItemDispatch* registration = new RenderItemDispatch();
        registration->m_Dispatch.m_DispatchFn = dispatch_fn;
        registration->m_Dispatch.m_VisibilityFn = visibility_fn;
        registration->m_Dispatch.m_UserData = user_data;
        registration->m_NumChanges = 0;
        registration->m_FrameDispatch = RENDERLIST_INVALID_DISPATCH;
        registration->m_RebuildCache = true;
        context->m_RenderItemDispatches[index] = registration;
        return index;
    }

    void UnregisterRenderListDispatch(HRenderContext context, HRenderItemDispatch dispatch)
    {
        assert(dispatch < context->m_RenderItemDispatches.Size() && context->m_RenderItemDispatches[dispatch]);
        delete context->m_RenderItemDispatches[dispatch];
        context->m_RenderItemDispatches[dispatch] = 0;
    }

    void SubmitRenderListDispatch(HRenderContext context, HRenderItemDispatch dispatch)
    {
        assert(dispatch < context->m_RenderItemDispatches.Size() && context->m_RenderItemDispatches[dispatch]);
        RenderItemDispatch& registration = *context->m_RenderItemDispatches[dispatch];
        if (registration.m_FrameDispatch != RENDERLIST_INVALID_DISPATCH)
            return;
        const RenderListDispatch& d = registration.m_Dispatch;
        HRenderListDispatch       frame_dispatch = AllocateRenderListDispatch(context, d.m_DispatchFn, d.m_VisibilityFn, d.m_UserData);
        if (frame_dispatch == RENDERLIST_INVALID_DISPATCH)
            return;
        context->m_RenderListDispatch[frame_dispatch] = d;
        registration.m_FrameDispatch = frame_dispatch;
        context->m_SubmittedRenderItemDispatches.Push(dispatch);
        context->m_RenderListRanges.SetSize(0);
    }

    static PersistentRenderItem* GetPersistentRenderItem(HRenderContext context, HRenderItem handle)
    {
        uint32_t index = (uint32_t)handle;
        if (index >= context->m_PersistentRenderItems.Size())
            return 0;
        PersistentRenderItem& item = context->m_PersistentRenderItems[index];
        return item.m_Alive && item.m_Generation == (uint32_t)(handle >> 32) ? &item : 0;
    }

    static void RecordRenderItemCacheChange(HRenderContext context, RenderItemDispatch& dispatch, uint32_t index)
    {
        if (dispatch.m_FrameDispatch != RENDERLIST_INVALID_DISPATCH)
            context->m_RenderListRanges.SetSize(0);
        if (dispatch.m_RebuildCache)
            return;
        // Deduplicate by slot, retaining its original cached tag even if it is
        // deleted and reused before the next draw.
        for (uint32_t i = 0; i < dispatch.m_NumChanges; ++i)
        {
            if (dispatch.m_Changes[i].m_Index == index)
                return;
        }
        if (dispatch.m_NumChanges == MAX_RENDER_ITEM_CACHE_CHANGES)
        {
            dispatch.m_RebuildCache = true;
            dispatch.m_NumChanges = 0;
            return;
        }
        RenderItemCacheChange change = { index, context->m_RenderList[index].m_TagListKey };
        dispatch.m_Changes[dispatch.m_NumChanges++] = change;
    }

    HRenderItem NewRenderItem(HRenderContext context, HRenderItemDispatch dispatch, uint64_t user_data)
    {
        assert(dispatch < context->m_RenderItemDispatches.Size() && context->m_RenderItemDispatches[dispatch]);
        uint32_t index = context->m_FirstFreeRenderItem;
        if (index != ~0u)
        {
            context->m_FirstFreeRenderItem = context->m_PersistentRenderItems[index].m_NextFree;
        }
        else
        {
            index = context->m_PersistentRenderItems.Size();
            if (context->m_PersistentRenderItems.Full())
            {
                context->m_PersistentRenderItems.OffsetCapacity(dmMath::Max(256u, index / 2));
            }
            // Discard the draw snapshot, not the independent legacy allocation.
            context->m_RenderList.SetSize(index);
            if (context->m_RenderList.Full())
                context->m_RenderList.OffsetCapacity(dmMath::Max(256u, context->m_RenderList.Capacity() / 2));
            context->m_RenderList.SetSize(index + 1);
            context->m_TransientRenderListDirty = true;
            PersistentRenderItem item = {};
            item.m_WorldTransform = Matrix4::identity();
            item.m_Generation = 1;
            context->m_PersistentRenderItems.Push(item);
        }
        PersistentRenderItem& item = context->m_PersistentRenderItems[index];
        item.m_Alive = 1;
        item.m_Enabled = 0;
        item.m_Dispatch = dispatch;
        item.m_WorldTransform = Matrix4::identity();
        item.m_BoundingVolumeType = BOUNDING_VOLUME_NONE;
        RenderListEntry& entry = context->m_RenderList[index];
        memset(&entry, 0, sizeof(entry));
        entry.m_UserData = user_data;
        entry.m_Dispatch = RENDERLIST_INVALID_DISPATCH;
        entry.m_MajorOrder = RENDER_ORDER_WORLD;
        return ((uint64_t)item.m_Generation << 32) | index;
    }

    bool SetWorldTransform(HRenderContext context, HRenderItem handle, const Matrix4& world_transform)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        item->m_WorldTransform = world_transform;
        RenderListEntry& entry = context->m_RenderList[(uint32_t)handle];
        entry.m_WorldPosition = Point3(world_transform.getTranslation());
        entry.m_FrustumHash = FRUSTUM_HASH_UNINITIALIZED;
        return true;
    }

    static bool SetBoundingVolume(HRenderContext context, HRenderItem handle, BoundingVolumeType type, const float* bounds)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        if (item->m_BoundingVolumeType != type || memcmp(item->m_Bounds, bounds, sizeof(item->m_Bounds)) != 0)
        {
            item->m_BoundingVolumeType = type;
            memcpy(item->m_Bounds, bounds, sizeof(item->m_Bounds));
            context->m_RenderList[(uint32_t)handle].m_FrustumHash = FRUSTUM_HASH_UNINITIALIZED;
        }
        return true;
    }

    bool SetBoundingSphere(HRenderContext context, HRenderItem handle, const Point3& world_center, float radius)
    {
        float bounds[6] = { world_center.getX(), world_center.getY(), world_center.getZ(), radius * radius, 0, 0 };
        return SetBoundingVolume(context, handle, BOUNDING_VOLUME_SPHERE, bounds);
    }

    bool SetBoundingAABB(HRenderContext context, HRenderItem handle, const Vector3& world_min, const Vector3& world_max)
    {
        if (world_min.getX() > world_max.getX() || world_min.getY() > world_max.getY() || world_min.getZ() > world_max.getZ())
        {
            float bounds[6] = {};
            return SetBoundingVolume(context, handle, BOUNDING_VOLUME_NONE, bounds);
        }
        float bounds[6] = { world_min.getX(), world_min.getY(), world_min.getZ(), world_max.getX(), world_max.getY(), world_max.getZ() };
        return SetBoundingVolume(context, handle, BOUNDING_VOLUME_AABB, bounds);
    }

    bool SetBoundingOBB(HRenderContext context, HRenderItem handle, const Vector3& local_min, const Vector3& local_max)
    {
        float bounds[6] = { local_min.getX(), local_min.getY(), local_min.getZ(), local_max.getX(), local_max.getY(), local_max.getZ() };
        return SetBoundingVolume(context, handle, BOUNDING_VOLUME_OBB, bounds);
    }

    bool SetBatchKey(HRenderContext context, HRenderItem handle, uint32_t key)
    {
        if (!GetPersistentRenderItem(context, handle))
            return false;
        context->m_RenderList[(uint32_t)handle].m_BatchKey = key;
        return true;
    }

    bool SetMaterial(HRenderContext context, HRenderItem handle, HMaterial material)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        RenderItemDispatch& dispatch = *context->m_RenderItemDispatches[item->m_Dispatch];
        uint32_t            key = material ? GetMaterialTagListKey(material) : RegisterMaterialTagList(context, 0, 0);
        RenderListEntry&    entry = context->m_RenderList[(uint32_t)handle];
        if (entry.m_TagListKey != key)
        {
            if (item->m_Enabled)
                RecordRenderItemCacheChange(context, dispatch, (uint32_t)handle);
            entry.m_TagListKey = key;
        }
        return true;
    }

    bool SetOrder(HRenderContext context, HRenderItem handle, RenderOrder render_order, uint32_t order)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        RenderListEntry& entry = context->m_RenderList[(uint32_t)handle];
        entry.m_MajorOrder = render_order;
        entry.m_Order = order & 0x00ffffff;
        return true;
    }

    bool Enable(HRenderContext context, HRenderItem handle, bool enabled)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        if (item->m_Enabled != enabled)
        {
            RenderItemDispatch& dispatch = *context->m_RenderItemDispatches[item->m_Dispatch];
            RecordRenderItemCacheChange(context, dispatch, (uint32_t)handle);
            dmArray<uint32_t>& items = dispatch.m_EnabledItems;
            if (enabled)
            {
                if (items.Full())
                    items.OffsetCapacity(dmMath::Max(256u, items.Size() / 2));
                item->m_EnabledIndex = items.Size();
                items.Push((uint32_t)handle);
            }
            else
            {
                uint32_t last = items.Back();
                items[item->m_EnabledIndex] = last;
                context->m_PersistentRenderItems[last].m_EnabledIndex = item->m_EnabledIndex;
                items.SetSize(items.Size() - 1);
            }
            item->m_Enabled = enabled;
            RenderListEntry& entry = context->m_RenderList[(uint32_t)handle];
            entry.m_Dispatch = enabled ? dispatch.m_FrameDispatch : RENDERLIST_INVALID_DISPATCH;
            entry.m_FrustumHash = FRUSTUM_HASH_UNINITIALIZED;
        }
        return true;
    }

    bool DeleteRenderItem(HRenderContext context, HRenderItem handle)
    {
        PersistentRenderItem* item = GetPersistentRenderItem(context, handle);
        if (!item)
            return false;
        if (item->m_Enabled)
            Enable(context, handle, false);
        item->m_Alive = 0;
        if (++item->m_Generation == 0)
            ++item->m_Generation;
        item->m_NextFree = context->m_FirstFreeRenderItem;
        context->m_FirstFreeRenderItem = (uint32_t)handle;
        RenderListEntry& entry = context->m_RenderList[(uint32_t)handle];
        entry.m_Dispatch = RENDERLIST_INVALID_DISPATCH;
        return true;
    }

} // namespace dmRender
