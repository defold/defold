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

#include <assert.h>                 // for assert
#include <string.h>                 // for memcpy
#include <float.h>                  // for FLT_MAX

#include <dlib/memory.h>            // for dmMemory
#include <dlib/hashtable.h>         // for dmHashTable32
#include <dlib/log.h>               // for dmLogError, dmLogOnceError, dmLog...
#include <dlib/profile.h>           // for DM_PROFILE, DM_PROPERTY_*
#include <dlib/zlib.h>              // for InflateBuffer
#include <dmsdk/dlib/intersection.h>// for dmIntersection

#include <graphics/graphics.h>      // for TextureParams, TextureFilter, Tex...
#include <graphics/graphics_util.h> // for PackRGBA

#include "fontmap.h"
#include "fontmap_private.h"
#include "font_renderer_private.h"  // for FontMap, RenderLayerMask

#include "font_renderer.h"       // for FontGlyphCompression
#include "font_renderer_api.h"   // for the font renderer backend api

#include <dmsdk/font/text_layout.h>

DM_PROPERTY_EXTERN(rmtp_Render);
DM_PROPERTY_U32(rmtp_FontCharacterCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# glyphs", &rmtp_Render);
DM_PROPERTY_U32(rmtp_FontVertexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of vertices in bytes", &rmtp_Render);

namespace dmRender
{
    using namespace dmVMath;

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters, uint32_t max_batches)
    {
        // TODO: Why does the vertex need to be 16-byte aligned?
        //DM_STATIC_ASSERT(GetFontVertexSize() % 16 == 0, Invalid_Struct_Size);

        DM_STATIC_ASSERT( MAX_FONT_RENDER_CONSTANTS == MAX_TEXT_RENDER_CONSTANTS, Constant_Arrays_Must_Have_Same_Size );

        TextContext& text_context = render_context->m_TextContext;

        text_context.m_FontRenderBackend = CreateFontRenderBackend();

        text_context.m_MaxVertexCount = max_characters * 6; // 6 vertices per character
        uint32_t buffer_size = GetFontVertexSize(text_context.m_FontRenderBackend) * text_context.m_MaxVertexCount;
        text_context.m_ClientBuffer = 0x0;
        text_context.m_VertexIndex = 0;
        text_context.m_VerticesFlushed = 0;
        text_context.m_Frame = 0;
        text_context.m_PreviousFrame = ~0;
        text_context.m_TextEntriesFlushed = 0;

        dmMemory::Result r = dmMemory::AlignedMalloc((void**)&text_context.m_ClientBuffer, 16, buffer_size);
        if (r != dmMemory::RESULT_OK) {
            dmLogError("Could not allocate text vertex buffer of size %u (%d).", buffer_size, r);
            return;
        }

        text_context.m_VertexDecl = CreateVertexDeclaration(text_context.m_FontRenderBackend, render_context->m_GraphicsContext);
        text_context.m_VertexBuffer = dmGraphics::NewVertexBuffer(render_context->m_GraphicsContext, buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        text_context.m_ConstantBuffers.SetCapacity(max_batches); // 1:1 index mapping with render object
        text_context.m_RenderObjects.SetCapacity(max_batches);
        text_context.m_RenderObjectIndex = 0;

        // Approximately as we store terminating '\0'
        text_context.m_TextBuffer.SetCapacity(max_characters);
        // NOTE: 8 is "arbitrary" heuristic
        text_context.m_TextEntries.SetCapacity(max_characters / 8);

        for (uint32_t i = 0; i < text_context.m_RenderObjects.Capacity(); ++i)
        {
            RenderObject ro;
            ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
            ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            ro.m_SetBlendFactors = 1;
            ro.m_VertexBuffer = text_context.m_VertexBuffer;
            ro.m_VertexDeclaration = text_context.m_VertexDecl;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            text_context.m_RenderObjects.Push(ro);
            text_context.m_ConstantBuffers.Push(dmRender::NewNamedConstantBuffer());
        }
    }

    void FinalizeTextContext(HRenderContext render_context)
    {
        TextContext& text_context = render_context->m_TextContext;
        for (uint32_t i = 0; i < text_context.m_ConstantBuffers.Size(); ++i)
        {
            dmRender::DeleteNamedConstantBuffer(text_context.m_ConstantBuffers[i]);
        }
        dmMemory::AlignedFree(text_context.m_ClientBuffer);
        dmGraphics::DeleteVertexBuffer(text_context.m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(text_context.m_VertexDecl);

        DestroyFontRenderBackend(text_context.m_FontRenderBackend);
    }

    DrawTextParams::DrawTextParams()
    : m_WorldTransform(Matrix4::identity())
    , m_FaceColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_OutlineColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_ShadowColor(0.0f, 0.0f, 0.0f, 1.0f)
    , m_Text(0x0)
    , m_SourceBlendFactor(dmGraphics::BLEND_FACTOR_ONE)
    , m_DestinationBlendFactor(dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
    , m_RenderOrder(0)
    , m_NumRenderConstants(0)
    , m_Width(FLT_MAX)
    , m_Height(0)
    , m_Leading(1.0f)
    , m_Tracking(0.0f)
    , m_LineBreak(false)
    , m_Align(TEXT_ALIGN_LEFT)
    , m_VAlign(TEXT_VALIGN_TOP)
    , m_StencilTestParamsSet(0)
    {
        m_StencilTestParams.Init();
    }

    static dmhash_t g_TextureSizeRecipHash = dmHashString64("texture_size_recip");

    static dmVMath::Point3 CalcCenterPoint(HFontMap font_map, const TextEntry& te, const TextMetrics& metrics) {
        float x_offset = OffsetX(te.m_Align, te.m_Width);
        float y_offset = OffsetY(te.m_VAlign, te.m_Height, font_map->m_MaxAscent, font_map->m_MaxDescent, te.m_Leading, metrics.m_LineCount);

        // find X,Y local coordinate of text center
        float center_x = x_offset; // start from the X position of the pivot point
        switch (te.m_Align) {
            case TEXT_ALIGN_LEFT:
                center_x += metrics.m_Width/2; // move halfway to the right since we're aligning left
            break;
            case TEXT_ALIGN_RIGHT:
                center_x -= metrics.m_Width/2; // move halfway to the left from pivot since we're aligning right
            break;
            // nothing to do for TEXT_ALIGN_CENTER. Pivot is already at the center of the text X-wise
        }
        float center_y = y_offset + font_map->m_MaxAscent - metrics.m_Height/2; // 'y_offset' to move to the baseline of first letter in text. +'MaAscent' to get to the top of the text. -layout_height to get to the center.

        dmVMath::Point3 center_point(center_x, center_y, 0); // fix at Z 0
        return center_point;
    }

    void DrawText(HRenderContext render_context, HFontMap font_map, HMaterial material, uint64_t batch_key, const DrawTextParams& params)
    {
        DM_PROFILE("DrawText");

        TextContext* text_context = &render_context->m_TextContext;

        if (text_context->m_TextEntries.Full()) {
            dmLogWarning("Out of text-render entries: %u", text_context->m_TextEntries.Capacity());
            return;
        }

        // The gui doesn't currently generate a batch key for each gui node, but instead rely on this being generated by the DrawText
        // The label component however, generates a batchkey when the label changes (which is usually not every frame)
        if( batch_key == 0 )
        {
            HashState64 key_state;
            dmHashInit64(&key_state, false);
            dmHashUpdateBuffer64(&key_state, &font_map, sizeof(font_map));
            dmHashUpdateBuffer64(&key_state, &params.m_RenderOrder, sizeof(params.m_RenderOrder));
            if (params.m_StencilTestParamsSet) {
                dmHashUpdateBuffer64(&key_state, &params.m_StencilTestParams, sizeof(params.m_StencilTestParams));
            }
            if (material) {
                dmHashUpdateBuffer64(&key_state, &material, sizeof(material));
            }
            batch_key = dmHashFinal64(&key_state);
        }

        uint32_t text_len = strlen(params.m_Text);
        uint32_t offset = text_context->m_TextBuffer.Size();
        if (text_context->m_TextBuffer.Capacity() < (offset + text_len + 1)) {
            dmLogWarning("Out of text-render buffer %u. Modify the graphics.max_characters in game.project.", text_context->m_TextBuffer.Capacity());
            return;
        }

        text_context->m_TextBuffer.PushArray(params.m_Text, text_len);
        text_context->m_TextBuffer.Push('\0');

        material = material ? material : GetFontMapMaterial(font_map);
        TextEntry te;
        te.m_Transform = params.m_WorldTransform;
        te.m_StringOffset = offset;
        te.m_FontMap = font_map;
        te.m_Material = material;
        te.m_BatchKey = batch_key;
        te.m_Next = -1;
        te.m_Tail = -1;

        te.m_FaceColor = dmGraphics::PackRGBA(Vector4(params.m_FaceColor.getXYZ(), params.m_FaceColor.getW() * font_map->m_Alpha));
        te.m_OutlineColor = dmGraphics::PackRGBA(Vector4(params.m_OutlineColor.getXYZ(), params.m_OutlineColor.getW() * font_map->m_OutlineAlpha));
        te.m_ShadowColor = dmGraphics::PackRGBA(Vector4(params.m_ShadowColor.getXYZ(), params.m_ShadowColor.getW() * font_map->m_ShadowAlpha));
        te.m_RenderOrder = params.m_RenderOrder;
        te.m_Width = params.m_Width;
        te.m_Height = params.m_Height;
        te.m_Leading = params.m_Leading;
        te.m_Tracking = params.m_Tracking;
        te.m_LineBreak = params.m_LineBreak;
        te.m_Align = params.m_Align;
        te.m_VAlign = params.m_VAlign;
        te.m_StencilTestParams = params.m_StencilTestParams;
        te.m_StencilTestParamsSet = params.m_StencilTestParamsSet;
        te.m_SourceBlendFactor = params.m_SourceBlendFactor;
        te.m_DestinationBlendFactor = params.m_DestinationBlendFactor;

        TextLayoutSettings settings;
        settings.m_Width = params.m_Width;
        settings.m_LineBreak = params.m_LineBreak;
        settings.m_Leading = params.m_Leading;
        settings.m_Tracking = params.m_Tracking;
        settings.m_Size = dmRender::GetFontMapSize(font_map);
        // legacy options for glyph bank fonts
        settings.m_Monospace = dmRender::GetFontMapMonospaced(font_map);
        settings.m_Padding = dmRender::GetFontMapPadding(font_map);
        TextMetrics metrics;

        // TODO: Allow for callers to have their prepared HTextLayout

        GetTextMetrics(text_context->m_FontRenderBackend, font_map, params.m_Text, &settings, &metrics);

        // find center and radius for frustum culling
        // TODO: Calculate proper AABB
        dmVMath::Point3 centerpoint_local = CalcCenterPoint(font_map, te, metrics);
        dmVMath::Point3 cornerpoint_local(centerpoint_local.getX() + metrics.m_Width/2, centerpoint_local.getY() + metrics.m_Height/2, centerpoint_local.getZ());
        dmVMath::Vector4 centerpoint_world = te.m_Transform * centerpoint_local; // transform to world coordinates
        dmVMath::Vector4 cornerpoint_world = te.m_Transform * cornerpoint_local;

        te.m_FrustumCullingRadiusSq = dmVMath::LengthSqr(cornerpoint_world - centerpoint_world);
        te.m_FrustumCullingCenter = dmVMath::Point3(centerpoint_world.getXYZ());


        assert( params.m_NumRenderConstants <= dmRender::MAX_FONT_RENDER_CONSTANTS );
        te.m_NumRenderConstants = params.m_NumRenderConstants;
        memcpy( te.m_RenderConstants, params.m_RenderConstants, params.m_NumRenderConstants * sizeof(dmRender::HConstant));

        text_context->m_TextEntries.Push(te);
    }

    static void CreateFontRenderBatch(HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("FontRenderBatch");
        TextContext& text_context = render_context->m_TextContext;

        const TextEntry& first_te = *(TextEntry*) buf[*begin].m_UserData;

        HFontMap font_map = first_te.m_FontMap;
        float im_recip = 1.0f;
        float ih_recip = 1.0f;
        float cache_cell_width_ratio  = 0.0;
        float cache_cell_height_ratio = 0.0;

        if (font_map->m_Texture)
        {
            dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
            float cache_width  = (float) dmGraphics::GetTextureWidth(graphics_context, font_map->m_Texture);
            float cache_height = (float) dmGraphics::GetTextureHeight(graphics_context, font_map->m_Texture);

            im_recip /= cache_width;
            ih_recip /= cache_height;

            cache_cell_width_ratio  = ((float) font_map->m_CacheCellWidth) / cache_width;
            cache_cell_height_ratio = ((float) font_map->m_CacheCellHeight) / cache_height;
        }

        uint32_t vertex_stride = GetFontVertexSize(text_context.m_FontRenderBackend);
        uint8_t* vertices = (uint8_t*)text_context.m_ClientBuffer;

        if (text_context.m_RenderObjectIndex >= text_context.m_RenderObjects.Size()) {
            dmLogWarning("Fontrenderer: Render object count reached limit (%d). Increase the capacity with graphics.max_font_batches", text_context.m_RenderObjectIndex);
            return;
        }

        dmRender::HNamedConstantBuffer constants_buffer = text_context.m_ConstantBuffers[text_context.m_RenderObjectIndex];
        RenderObject* ro = &text_context.m_RenderObjects[text_context.m_RenderObjectIndex];
        text_context.m_RenderObjectIndex++;

        ro->m_SourceBlendFactor = first_te.m_SourceBlendFactor;
        ro->m_DestinationBlendFactor = first_te.m_DestinationBlendFactor;
        ro->m_SetBlendFactors = 1;
        ro->m_Material = first_te.m_Material;
        ro->m_Textures[0] = font_map->m_Texture;
        ro->m_VertexStart = text_context.m_VertexIndex;
        ro->m_StencilTestParams = first_te.m_StencilTestParams;
        ro->m_SetStencilTest = first_te.m_StencilTestParamsSet;

        Vector4 texture_size_recip(im_recip, ih_recip, cache_cell_width_ratio, cache_cell_height_ratio);

        dmRender::ClearNamedConstantBuffer(constants_buffer);
        dmRender::SetNamedConstants(constants_buffer, (HConstant*)first_te.m_RenderConstants, first_te.m_NumRenderConstants);
        dmRender::SetNamedConstant(constants_buffer, g_TextureSizeRecipHash, &texture_size_recip, 1);

        ro->m_ConstantBuffer = constants_buffer;

        // The cache size may have changed, and we need to update the font map glyph texture
        UpdateCacheTexture(font_map);

        for (uint32_t *i = begin;i != end; ++i)
        {
            const TextEntry& te = *(TextEntry*) buf[*i].m_UserData;
            const char* text = &text_context.m_TextBuffer[te.m_StringOffset];

            uint32_t num_vertices = CreateFontVertexData(text_context.m_FontRenderBackend, font_map, text_context.m_Frame, text, te, im_recip, ih_recip, vertices + text_context.m_VertexIndex * vertex_stride, text_context.m_MaxVertexCount - text_context.m_VertexIndex);
            text_context.m_VertexIndex += num_vertices;
        }

        ro->m_VertexCount = text_context.m_VertexIndex - ro->m_VertexStart;

        dmRender::AddToRender(render_context, ro);
    }

    static void FontRenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        HRenderContext render_context = (HRenderContext)params.m_UserData;
        TextContext& text_context = render_context->m_TextContext;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                if (text_context.m_VerticesFlushed != text_context.m_VertexIndex)
                {
                    uint32_t vertex_size = GetFontVertexSize(text_context.m_FontRenderBackend);
                    uint32_t buffer_size = vertex_size * text_context.m_VertexIndex;
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
                    dmGraphics::SetVertexBufferData(text_context.m_VertexBuffer, buffer_size, text_context.m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

                    uint32_t num_vertices = text_context.m_VertexIndex - text_context.m_VerticesFlushed;
                    text_context.m_VerticesFlushed = text_context.m_VertexIndex;

                    DM_PROPERTY_ADD_U32(rmtp_FontCharacterCount, num_vertices / 6);
                    DM_PROPERTY_ADD_U32(rmtp_FontVertexSize, num_vertices * vertex_size);
                }
                break;
            case dmRender::RENDER_LIST_OPERATION_BATCH:
                CreateFontRenderBatch(render_context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            default:
                break;
        }
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Label");

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];
            TextEntry* te = ((TextEntry*) entry->m_UserData);

            bool intersect = dmIntersection::TestFrustumSphereSq(frustum, te->m_FrustumCullingCenter, te->m_FrustumCullingRadiusSq);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    void FlushTexts(HRenderContext render_context, uint32_t major_order, uint32_t render_order, bool final)
    {
        DM_PROFILE("FlushTexts");

        (void)final;
        TextContext& text_context = render_context->m_TextContext;

        if (text_context.m_TextEntries.Size() > 0)
        {
            // This function is called for both game object text (labels) and gui
            // Once the sprites are done rendering, it is ok to reuse/reset the state
            // and the gui can start rendering its text
            // See also ClearRenderObjects() in render.cpp for clearing the text entries
            if (text_context.m_Frame != text_context.m_PreviousFrame)
            {
                text_context.m_PreviousFrame = text_context.m_Frame;
                text_context.m_RenderObjectIndex = 0;
                text_context.m_VertexIndex = 0;
                text_context.m_VerticesFlushed = 0;
                text_context.m_TextEntriesFlushed = 0;
            }

            uint32_t count = text_context.m_TextEntries.Size() - text_context.m_TextEntriesFlushed;

            if (count > 0) {
                dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
                dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &FontRenderListDispatch, &RenderListFrustumCulling, render_context);
                dmRender::RenderListEntry* write_ptr = render_list;

                for( uint32_t i = 0; i < count; ++i )
                {
                    TextEntry& te = text_context.m_TextEntries[text_context.m_TextEntriesFlushed + i];
                    write_ptr->m_WorldPosition = Point3(te.m_Transform.getTranslation());
                    write_ptr->m_MinorOrder = 0;
                    write_ptr->m_MajorOrder = major_order;
                    write_ptr->m_Order = render_order;
                    write_ptr->m_UserData = (uintptr_t) &te; // The text entry must live until the dispatch is done
                    write_ptr->m_BatchKey = te.m_BatchKey;
                    write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(te.m_Material);
                    write_ptr->m_Dispatch = dispatch;
                    write_ptr++;
                }
                dmRender::RenderListSubmit(render_context, render_list, write_ptr);
            }
        }

        // Always update after flushing
        text_context.m_TextEntriesFlushed = text_context.m_TextEntries.Size();
    }

}
