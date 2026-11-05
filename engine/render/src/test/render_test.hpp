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

#ifndef DM_RENDER_TEST_HPP
#define DM_RENDER_TEST_HPP

#include <render/render.h>
#include <render/font/fontmap.h>

// Integration-test inspection and fault injection owned by the renderer.
// Handles are borrowed. Returned objects/data remain borrowed until the next
// mutation of the corresponding collection, upload, cache or render context.
namespace dmRender
{
    struct TestPersistentRenderItem
    {
        dmVMath::Matrix4 m_WorldTransform;
        uint32_t         m_Generation;
        bool             m_Enabled;
        bool             m_Alive;
    };

    struct TestDispatchCache
    {
        uint32_t m_SortIndexCount;
        bool     m_RebuildCache;
    };

    struct TestRenderListDispatch
    {
        RenderListDispatchFn   m_DispatchFn;
        RenderListVisibilityFn m_VisibilityFn;
        void*                  m_UserData;
    };

    struct TestTextEntry
    {
        dmVMath::Matrix4 m_Transform;
        HFontMap         m_FontMap;
        HTextLayout      m_TextLayout;
        float            m_Width;
        float            m_Height;
        uint32_t         m_Align;
        uint32_t         m_VAlign;
    };

    struct TestLight
    {
        dmVMath::Vector4 m_Position;
        dmVMath::Vector4 m_Color;
        dmVMath::Vector4 m_DirectionRange;
        dmVMath::Vector4 m_Params;
    };

    struct TestLightState
    {
        dmVMath::Vector3 m_AmbientLight;
        uint32_t         m_ScratchCount;
        uint32_t         m_UploadScratchCount;
        uint32_t         m_MaxLightCount;
        uint32_t         m_LightBufferInfoWriteStart;
        uint32_t         m_LightBufferDataWriteStart;
    };

    struct TestLightBufferInfo
    {
        uint16_t m_Set;
        uint16_t m_Binding;
        uint16_t m_Capacity;
        bool     m_Enabled;
    };

    struct TestFontCacheState
    {
        uint32_t             m_GlyphCount;
        int32_t              m_CacheCellMaxAscent;
        int32_t              m_CacheCellWidth;
        int32_t              m_CacheCellHeight;
        uint32_t             m_CacheCellCount;
        dmGraphics::HTexture m_VectorSdfTexture;
        bool                 m_HasCache;
        bool                 m_VectorBitmapEffects;
    };

    uint32_t                 RegisterMaterialTagList(HRenderContext context, uint32_t count, const dmhash_t* tags);
    void                     DispatchCompute(HRenderContext context, uint32_t x, uint32_t y, uint32_t z, HNamedConstantBuffer constants);
    void                     ApplyMaterialProgramLightBuffers(HRenderContext context, HMaterial material);
    void                     ApplyComputeProgramLightBuffers(HRenderContext context, HComputeProgram program);
    bool                     TestPoisonNextMaterialAttributeAllocation(dmhash_t element_id, uint32_t attribute_count);
    bool                     TestWasMaterialAttributeAllocationPoisoned();
    void                     TestSetMaterial(HRenderContext context, HMaterial material);
    void                     TestSetComputeProgram(HRenderContext context, HComputeProgram program);
    void                     TestSetMultiBufferingRequired(HRenderContext context, bool required);
    bool                     TestGetMultiBufferingRequired(HRenderContext context);
    uint32_t                 TestGetPersistentRenderItemCount(HRenderContext context);
    TestPersistentRenderItem TestGetPersistentRenderItem(HRenderContext context, uint32_t index);
    uint32_t                 TestGetRenderListCount(HRenderContext context);
    const RenderListEntry&   TestGetRenderListEntry(HRenderContext context, uint32_t index);
    uint32_t                 TestGetSortedRenderListCount(HRenderContext context);
    uint32_t                 TestGetSortedRenderListEntry(HRenderContext context, uint32_t index);
    uint32_t                 TestGetRenderObjectCount(HRenderContext context);
    const RenderObject*      TestGetRenderObject(HRenderContext context, uint32_t index);
    uint32_t                 TestGetRenderListDispatchCount(HRenderContext context);
    TestRenderListDispatch   TestGetRenderListDispatch(HRenderContext context, uint32_t index);
    TestDispatchCache        TestGetSubmittedDispatchCache(HRenderContext context, uint32_t index);
    void                     TestSwapSubmittedDispatchIndices(HRenderContext context, uint32_t dispatch, uint32_t first, uint32_t second);
    uint32_t                 TestGetBufferedBufferCount(HBufferedRenderBuffer buffer);
    HRenderBuffer            TestGetBufferedBuffer(HBufferedRenderBuffer buffer, uint32_t index);
    RenderBufferType         TestGetBufferedBufferType(HBufferedRenderBuffer buffer);
    uint32_t                 TestGetQueuedTextCount(HRenderContext context);
    uint32_t                 TestGetTextBufferSize(HRenderContext context);
    TestTextEntry            TestGetQueuedText(HRenderContext context, uint32_t index);
    uint32_t                 TestGetTextRenderObjectCount(HRenderContext context);
    const RenderObject&      TestGetTextRenderObject(HRenderContext context, uint32_t index);
    TestLightState           TestGetLightState(HRenderContext context);
    TestLight                TestGetLightScratch(HRenderContext context, uint32_t index);
    const void*              TestGetLightUploadData(HRenderContext context);
    TestLightBufferInfo      TestGetMaterialLightBufferInfo(HMaterial material);
    TestLightBufferInfo      TestGetComputeLightBufferInfo(HComputeProgram program);
    bool                     TestGetLightPrototypeParams(HRenderContext context, HLightPrototype light, LightPrototypeParams* params);
    TestFontCacheState       TestGetFontCacheState(HFontMap font_map);
    void                     TestSetFontMapPadding(HFontMap font_map, uint32_t padding);
    FontGlyph*               TestGetCachedFontGlyph(HFontMap font_map, uint64_t key);
}

#endif // DM_RENDER_TEST_HPP
