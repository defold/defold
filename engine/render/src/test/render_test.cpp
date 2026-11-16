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

#include "render_test.hpp"
#include "../render/render_private.h"
#include "../render/font/fontmap_private.h"
#include <dlib/static_assert.h>
#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
#include <sanitizer/allocator_interface.h>
#endif

namespace dmRender
{
#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
    struct MaterialAttributeAllocationPoison
    {
        dmhash_t m_ElementId;
        uint32_t m_AttributeCount;
        bool     m_Armed;
        bool     m_MaterialAllocated;
        bool     m_Poisoned;
    };

    static MaterialAttributeAllocationPoison g_MaterialAttributeAllocationPoison;

    static void MaterialAttributeMallocHook(const volatile void* ptr, size_t size)
    {
        MaterialAttributeAllocationPoison& poison = g_MaterialAttributeAllocationPoison;
        if (!poison.m_Armed)
        {
            return;
        }

        if (!poison.m_MaterialAllocated)
        {
            poison.m_MaterialAllocated = size == sizeof(Material);
            return;
        }

        if (size == sizeof(MaterialAttribute) * poison.m_AttributeCount)
        {
            MaterialAttribute* attributes = (MaterialAttribute*) const_cast<void*>(ptr);
            attributes[0].m_ElementIds[0] = poison.m_ElementId;
            poison.m_Poisoned = true;
            poison.m_Armed = false;
        }
        else
        {
            poison.m_MaterialAllocated = size == sizeof(Material);
        }
    }

    static void MaterialAttributeFreeHook(const volatile void*)
    {
    }

    bool TestPoisonNextMaterialAttributeAllocation(dmhash_t element_id, uint32_t attribute_count)
    {
        static bool hook_installed = false;
        if (!hook_installed)
        {
            hook_installed = __sanitizer_install_malloc_and_free_hooks(MaterialAttributeMallocHook, MaterialAttributeFreeHook) != 0;
        }

        memset(&g_MaterialAttributeAllocationPoison, 0, sizeof(g_MaterialAttributeAllocationPoison));
        g_MaterialAttributeAllocationPoison.m_ElementId = element_id;
        g_MaterialAttributeAllocationPoison.m_AttributeCount = attribute_count;
        g_MaterialAttributeAllocationPoison.m_Armed = hook_installed;
        return hook_installed;
    }

    bool TestWasMaterialAttributeAllocationPoisoned()
    {
        return g_MaterialAttributeAllocationPoison.m_Poisoned;
    }
#else
    bool TestPoisonNextMaterialAttributeAllocation(dmhash_t, uint32_t)
    {
        return false;
    }

    bool TestWasMaterialAttributeAllocationPoisoned()
    {
        return false;
    }
#endif

    DM_STATIC_ASSERT(sizeof(TestLight) == sizeof(LightSTD140), Invalid_Test_Light_Upload_Size);

    void TestSetMaterial(HRenderContext context, HMaterial material)
    {
        context->m_Material = material;
    }

    void TestSetComputeProgram(HRenderContext context, HComputeProgram program)
    {
        context->m_ComputeProgram = program;
    }

    void TestSetMultiBufferingRequired(HRenderContext context, bool required)
    {
        context->m_MultiBufferingRequired = required;
    }

    bool TestGetMultiBufferingRequired(HRenderContext context)
    {
        return context->m_MultiBufferingRequired;
    }

    uint32_t TestGetPersistentRenderItemCount(HRenderContext context)
    {
        return context->m_PersistentRenderItems.Size();
    }

    uint32_t TestGetRenderListCount(HRenderContext context)
    {
        return context->m_RenderList.Size();
    }

    const RenderListEntry& TestGetRenderListEntry(HRenderContext context, uint32_t index)
    {
        return context->m_RenderList[index];
    }

    uint32_t TestGetSortedRenderListCount(HRenderContext context)
    {
        return context->m_RenderListSortBuffer.Size();
    }

    uint32_t TestGetSortedRenderListEntry(HRenderContext context, uint32_t index)
    {
        return context->m_RenderListSortBuffer[index];
    }

    uint32_t TestGetRenderObjectCount(HRenderContext context)
    {
        return context->m_RenderObjects.Size();
    }

    const RenderObject* TestGetRenderObject(HRenderContext context, uint32_t index)
    {
        return context->m_RenderObjects[index];
    }

    uint32_t TestGetRenderListDispatchCount(HRenderContext context)
    {
        return context->m_RenderListDispatch.Size();
    }

    uint32_t TestGetBufferedBufferCount(HBufferedRenderBuffer buffer)
    {
        return buffer->m_Buffers.Size();
    }

    HRenderBuffer TestGetBufferedBuffer(HBufferedRenderBuffer buffer, uint32_t index)
    {
        return buffer->m_Buffers[index];
    }

    RenderBufferType TestGetBufferedBufferType(HBufferedRenderBuffer buffer)
    {
        return buffer->m_Type;
    }

    uint32_t TestGetQueuedTextCount(HRenderContext context)
    {
        return context->m_TextContext.m_TextEntries.Size();
    }

    uint32_t TestGetTextBufferSize(HRenderContext context)
    {
        return context->m_TextContext.m_TextBuffer.Size();
    }

    uint32_t TestGetTextRenderObjectCount(HRenderContext context)
    {
        return context->m_TextContext.m_RenderObjectIndex;
    }

    const RenderObject& TestGetTextRenderObject(HRenderContext context, uint32_t index)
    {
        return context->m_TextContext.m_RenderObjects[index];
    }

    const void* TestGetLightUploadData(HRenderContext context)
    {
        return context->m_LightBufferUploadScratch.Begin();
    }

    TestPersistentRenderItem TestGetPersistentRenderItem(HRenderContext context, uint32_t index)
    {
        const PersistentRenderItem& item = context->m_PersistentRenderItems[index];
        TestPersistentRenderItem result = { item.m_WorldTransform, item.m_Generation, item.m_Enabled != 0, item.m_Alive != 0 };
        return result;
    }

    TestRenderListDispatch TestGetRenderListDispatch(HRenderContext context, uint32_t index)
    {
        const RenderListDispatch& dispatch = context->m_RenderListDispatch[index];
        TestRenderListDispatch result = { dispatch.m_DispatchFn, dispatch.m_VisibilityFn, dispatch.m_UserData };
        return result;
    }

    TestDispatchCache TestGetSubmittedDispatchCache(HRenderContext context, uint32_t index)
    {
        RenderItemDispatch* dispatch = context->m_RenderItemDispatches[context->m_SubmittedRenderItemDispatches[index]];
        TestDispatchCache result = { dispatch->m_SortIndices.Size(), dispatch->m_RebuildCache != 0 };
        return result;
    }

    void TestSwapSubmittedDispatchIndices(HRenderContext context, uint32_t index, uint32_t first, uint32_t second)
    {
        RenderItemDispatch* dispatch = context->m_RenderItemDispatches[context->m_SubmittedRenderItemDispatches[index]];
        uint32_t value = dispatch->m_SortIndices[first];
        dispatch->m_SortIndices[first] = dispatch->m_SortIndices[second];
        dispatch->m_SortIndices[second] = value;
    }

    TestTextEntry TestGetQueuedText(HRenderContext context, uint32_t index)
    {
        const TextEntry& entry = context->m_TextContext.m_TextEntries[index];
        TestTextEntry result = { entry.m_Transform, entry.m_FontMap, entry.m_TextLayout, entry.m_Curve,
                                 entry.m_Width, entry.m_Height, entry.m_ArcRadius, entry.m_ArcAngle,
                                 entry.m_FrustumCullingRadiusSq, entry.m_Align, entry.m_VAlign };
        return result;
    }

    TestLightState TestGetLightState(HRenderContext context)
    {
        TestLightState result = { context->m_AmbientLight, context->m_LightBufferScratch.Size(),
                                  context->m_LightBufferUploadScratch.Size(), context->m_MaxLightCount,
                                  context->m_LightBufferInfoWriteStart, context->m_LightBufferDataWriteStart };
        return result;
    }

    TestLight TestGetLightScratch(HRenderContext context, uint32_t index)
    {
        const LightSTD140& light = context->m_LightBufferScratch[index];
        TestLight result = { light.m_Position, light.m_Color, light.m_DirectionRange, light.m_Params };
        return result;
    }

    TestLightBufferInfo TestGetMaterialLightBufferInfo(HMaterial material)
    {
        TestLightBufferInfo result = { material->m_LightBufferSet, material->m_LightBufferBinding,
                                       material->m_LightBufferCapacity, material->m_HasLightBuffer != 0 };
        return result;
    }

    TestLightBufferInfo TestGetComputeLightBufferInfo(HComputeProgram program)
    {
        TestLightBufferInfo result = { program->m_LightBufferSet, program->m_LightBufferBinding,
                                       program->m_LightBufferCapacity, program->m_HasLightBuffer != 0 };
        return result;
    }

    bool TestGetLightPrototypeParams(HRenderContext context, HLightPrototype handle, LightPrototypeParams* params)
    {
        const LightPrototype* light = GetLightPrototype(context, handle);
        if (!light)
        {
            return false;
        }
        params->m_Type = light->m_Type;
        params->m_Color = light->m_Color;
        params->m_Intensity = light->m_Intensity;
        params->m_Range = light->m_Range;
        params->m_InnerConeAngle = light->m_InnerConeAngle;
        params->m_OuterConeAngle = light->m_OuterConeAngle;
        return true;
    }

    TestFontCacheState TestGetFontCacheState(HFontMap font_map)
    {
        TestFontCacheState result = { font_map->m_Glyphs.Size(), font_map->m_CacheCellMaxAscent,
                                      font_map->m_CacheCellWidth, font_map->m_CacheCellHeight,
                                      font_map->m_CacheCellCount, font_map->m_VectorSdfTexture,
                                      font_map->m_Cache != 0, font_map->m_VectorBitmapEffects };
        return result;
    }

    void TestSetFontMapPadding(HFontMap font_map, uint32_t padding)
    {
        font_map->m_IsMonospaced = padding != 0;
        font_map->m_Padding = padding;
    }

    FontGlyph* TestGetCachedFontGlyph(HFontMap font_map, uint64_t key)
    {
        FontGlyph** glyph = font_map->m_Glyphs.Get(key);
        return glyph ? *glyph : 0;
    }
}
