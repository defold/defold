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

#ifndef DM_GAMESYS_COMP_SPRITE_H
#define DM_GAMESYS_COMP_SPRITE_H

#include <gameobject/component.h>

namespace dmRender { struct RenderFrameBuilder; struct RenderFrameConsumers; }

namespace dmGameSystem
{
    // Internal PoC diagnostics, not a public SDK contract. Frame capacity includes
    // capture lookup tables; allocation-growth peaks are reported separately. Renderer CPU includes
    // named constant buffers; logical GPU and reported resource sizes are separate.
    struct SpriteSnapshotStats
    {
        uint64_t m_PayloadUsedBytes;
        uint64_t m_SlotsPayloadUsedBytes; // At capture, before the older slot is retired.
        uint64_t m_FrameCapacityBytes;
        uint64_t m_RendererCpuCapacityBytes;
        uint64_t m_ConstantBufferCapacityBytes;
        uint64_t m_RendererGpuLogicalBytes;
        uint64_t m_RetainedResourceReportedBytes;
        uint64_t m_FrameGrowthPeakBytes;
        uint64_t m_CaptureCount;
        uint64_t m_CaptureTotalUs;
        uint32_t m_RecordBytes;
        uint32_t m_BoundBytes;
        uint32_t m_SpriteCount;
        uint32_t m_BindingCount;
        uint32_t m_GeometryCount;
        uint32_t m_ConstantBlockCount;
        uint32_t m_AttributeBlockCount;
        uint32_t m_RetainedReferenceCount;
        uint8_t m_Inline;
        uint8_t m_Threaded;
    };

    struct SpriteContext;
    bool RegisterSpriteRenderFrame(void* world, dmRender::RenderFrameConsumers* consumers);
    bool CaptureSpriteRenderFrame(void* world, SpriteContext* context, dmRender::RenderFrameBuilder* builder);
    // Capture may overlap the other slot. Finish retires it after consumer drain.
    void FinishSpriteThreadFrame(void* world, SpriteContext* context, uint32_t slot);
    bool CaptureSpriteThreadFrame(void* world, SpriteContext* context, uint32_t slot, uint32_t capacity_limit = 32 * 1024 * 1024);
    void RenderSpriteThreadFrame(void* world, SpriteContext* context, uint32_t slot);
    void ReleaseSpriteThreadFrames(void* world, SpriteContext* context);
    void GetSpriteSnapshotStats(void* sprite_world, SpriteSnapshotStats* stats);

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompSpriteCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompSpriteAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompSpriteLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompSpriteRender(const dmGameObject::ComponentsRenderParams& params);

    dmGameObject::UpdateResult CompSpriteOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void*                      CompSpriteGetComponent(const dmGameObject::ComponentGetParams& params);

    void CompSpriteOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompSpriteGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompSpriteSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    void CompSpriteIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node);
}

#endif // DM_GAMESYS_COMP_SPRITE_H
