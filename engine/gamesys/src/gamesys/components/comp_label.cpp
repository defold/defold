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

#include "comp_label.h"

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/object_pool.h>
#include <dlib/math.h>
#include <dlib/transform.h>
#include <dmsdk/dlib/vmath.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <font/text_layout.h>
#include <render/font/fontmap.h>
#include <render/font/font_renderer.h>
#include <gameobject/gameobject_ddf.h>
#include <dmsdk/gameobject/script.h>

#include "../resources/res_label.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include <gamesys/label_ddf.h>
#include <gamesys/gamesys_ddf.h>
#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/gamesys/resources/res_font.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Label, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

    static const dmhash_t TAG_LINK          = dmHashString64("link");
    static const dmhash_t STYLE_LINK_HOVER  = dmHashString64("link:hover");
    static const dmhash_t STYLE_LINK_ACTIVE = dmHashString64("link:active");

    struct LabelComponent
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Size;         // The text area size
        Vector3                     m_Scale;
        Vector4                     m_Color;
        Vector4                     m_Outline;
        Vector4                     m_Shadow;
        Matrix4                     m_World;
        uint32_t                    m_Pivot;
        // Hash of the components properties. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        LabelResource*              m_Resource;
        HComponentRenderConstants   m_RenderConstants;
        MaterialResource*           m_Material;
        FontResource*               m_Font;

        float                       m_Leading;
        float                       m_Tracking;

        const char*                 m_Text;
        HTextLayout                 m_TextLayout;
        uint32_t                    m_TextLayoutFontVersion;
        uint32_t                    m_HoveredLinkObject;
        uint32_t                    m_PressedLinkObject;
        uint32_t                    m_Index;

        uint16_t                    m_ComponentIndex;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_UserAllocatedText : 1;
        uint16_t                    m_ReHash : 1;
        uint16_t                    m_LineBreak : 1;
        uint16_t                    m_TextLayoutDirty : 1;
        uint16_t                    m_Padding : 10;
    };

    struct LabelWorld
    {
        dmObjectPool<LabelComponent>    m_Components;
    };

    DM_GAMESYS_PROP_VECTOR3(LABEL_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(LABEL_PROP_SIZE, size, false);
    DM_GAMESYS_PROP_VECTOR4(LABEL_PROP_COLOR, color, false);
    DM_GAMESYS_PROP_VECTOR4(LABEL_PROP_OUTLINE, outline, false);
    DM_GAMESYS_PROP_VECTOR4(LABEL_PROP_SHADOW, shadow, false);
    static const dmhash_t LABEL_PROP_LEADING = dmHashString64("leading");
    static const dmhash_t LABEL_PROP_TRACKING = dmHashString64("tracking");
    static const dmhash_t LABEL_PROP_LINE_BREAK = dmHashString64("line_break");

    static void        InvalidateTextLayout(LabelComponent* component);
    static HTextLayout GetOrCreateTextLayout(LabelComponent* component);

    // Reserves the proposed one-em or explicit sprite dimensions until resource loading is implemented.
    static uint8_t ResolveLabelLayoutObject(void*, const char*, const TextLayoutObjectAttribute*,
                                            float proposed_width, float proposed_height, TextLayoutObject* object)
    {
        object->m_Width = proposed_width;
        object->m_Height = proposed_height;
        object->m_Resource = 0;

        return 1;
    }

    dmGameObject::CreateResult CompLabelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LabelContext* label_context = (LabelContext*)params.m_Context;
        LabelWorld*   world = new LabelWorld();
        uint32_t      comp_count = dmMath::Min(params.m_MaxComponentInstances, label_context->m_MaxLabelCount);
        world->m_Components.SetCapacity(comp_count);
        memset(world->m_Components.GetRawObjects().Begin(), 0, sizeof(LabelComponent) * comp_count);

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        LabelWorld*              world = (LabelWorld*)params.m_World;

        dmArray<LabelComponent>& components = world->m_Components.GetRawObjects();
        uint32_t                 n = components.Size();

        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent& component = components[i];
            InvalidateTextLayout(&component);
            if (component.m_UserAllocatedText)
            {
                free((void*)component.m_Text);
            }
        }

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline MaterialResource* GetMaterialResource(LabelComponent* component, LabelResource* resource)
    {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline dmRender::HMaterial GetMaterial(LabelComponent* component, LabelResource* resource)
    {
        return GetMaterialResource(component, resource)->m_Material;
    }

    static inline FontResource* GetFontResource(const LabelComponent* component, const LabelResource* resource)
    {
        return component->m_Font ? component->m_Font : resource->m_Font;
    }

    static inline dmRender::HFontMap GetFontMap(const LabelComponent* component, const LabelResource* resource)
    {
        FontResource* font = GetFontResource(component, resource);
        return dmGameSystem::ResFontGetHandle(font);
    }

    static void InvalidateTextLayout(LabelComponent* component)
    {
        if (component->m_TextLayout)
        {
            TextLayoutRelease(component->m_TextLayout);
            component->m_TextLayout = 0;
        }
        component->m_TextLayoutFontVersion = 0;
        component->m_TextLayoutDirty = 1;
        component->m_HoveredLinkObject = UINT32_MAX;
        component->m_PressedLinkObject = UINT32_MAX;
    }

    static HTextLayout GetOrCreateTextLayout(LabelComponent* component)
    {
        FontResource* font_resource = GetFontResource(component, component->m_Resource);
        uint32_t      font_version = font_resource ? ResFontGetVersion(font_resource) : 0;

        if (!component->m_TextLayoutDirty && component->m_TextLayout && font_resource &&
            component->m_TextLayoutFontVersion == font_version)
        {
            return component->m_TextLayout;
        }

        InvalidateTextLayout(component);

        dmRender::HFontMap font_map = font_resource ? ResFontGetHandle(font_resource) : 0;
        if (!font_map || !component->m_Text)
            return 0;

        TextLayoutSettings settings = { 0 };
        settings.m_Width = component->m_Size.getX();
        settings.m_LineBreak = component->m_LineBreak;
        settings.m_Leading = component->m_Leading;
        settings.m_Tracking = component->m_Tracking;
        settings.m_Size = dmRender::GetFontMapSize(font_map);
        settings.m_Monospace = dmRender::GetFontMapMonospaced(font_map);
        settings.m_Padding = dmRender::GetFontMapPadding(font_map);
        settings.m_ResolveObject = ResolveLabelLayoutObject;

        HTextLayout  layout = 0;
        HMarkup      markup = 0;
        MarkupResult markup_result = MarkupCreateRecovering(component->m_Text, strlen(component->m_Text), &markup, 0);
        TextResult   result = TEXT_RESULT_ERROR;

        if (markup_result == MARKUP_RESULT_OK)
        {
            result = TextLayoutCreateMarkup(dmRender::GetFontCollection(font_map), markup, &settings, &layout);
        }

        MarkupDestroy(markup);

        if (result != TEXT_RESULT_OK)
        {
            if (layout)
            {
                TextLayoutRelease(layout);
            }

            layout = 0;
            dmArray<uint32_t> codepoints;
            TextToCodePoints(component->m_Text, codepoints);
            result = TextLayoutCreate(dmRender::GetFontCollection(font_map), codepoints.Begin(), codepoints.Size(), &settings, &layout);
        }

        if (result != TEXT_RESULT_OK)
        {
            if (layout)
                TextLayoutRelease(layout);
            return 0;
        }

        component->m_TextLayout = layout;
        component->m_TextLayoutFontVersion = font_version;
        component->m_TextLayoutDirty = 0;
        return component->m_TextLayout;
    }

    void ReHash(LabelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        LabelResource* resource = component->m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
        dmRender::HMaterial material = GetMaterial(component, resource);
        FontResource* font = GetFontResource(component, resource);

        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, font, sizeof(font));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmHashUpdateBuffer32(&state, &ddf->m_Color, sizeof(ddf->m_Color));
        dmHashUpdateBuffer32(&state, &ddf->m_Outline, sizeof(ddf->m_Outline));
        dmHashUpdateBuffer32(&state, &ddf->m_Shadow, sizeof(ddf->m_Shadow));

        if (component->m_RenderConstants) {
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        }

        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    /** Taken from gui_private.h
     */
    inline Vector3 CalcPivotDelta(uint32_t pivot, Vector3 size)
    {
        float width = size.getX();
        float height = size.getY();

        Vector3 delta_pivot = Vector3(0.0f, 0.0f, 0.0f);

        switch (pivot)
        {
            case dmGameSystemDDF::LabelDesc::PIVOT_CENTER:
            case dmGameSystemDDF::LabelDesc::PIVOT_S:
            case dmGameSystemDDF::LabelDesc::PIVOT_N:
                delta_pivot.setX(-width * 0.5f);
                break;

            case dmGameSystemDDF::LabelDesc::PIVOT_NE:
            case dmGameSystemDDF::LabelDesc::PIVOT_E:
            case dmGameSystemDDF::LabelDesc::PIVOT_SE:
                delta_pivot.setX(-width);
                break;

            case dmGameSystemDDF::LabelDesc::PIVOT_SW:
            case dmGameSystemDDF::LabelDesc::PIVOT_W:
            case dmGameSystemDDF::LabelDesc::PIVOT_NW:
                break;
        }
        switch (pivot) {
            case dmGameSystemDDF::LabelDesc::PIVOT_CENTER:
            case dmGameSystemDDF::LabelDesc::PIVOT_E:
            case dmGameSystemDDF::LabelDesc::PIVOT_W:
                delta_pivot.setY(-height * 0.5f);
                break;

            case dmGameSystemDDF::LabelDesc::PIVOT_N:
            case dmGameSystemDDF::LabelDesc::PIVOT_NE:
            case dmGameSystemDDF::LabelDesc::PIVOT_NW:
                delta_pivot.setY(-height);
                break;

            case dmGameSystemDDF::LabelDesc::PIVOT_S:
            case dmGameSystemDDF::LabelDesc::PIVOT_SW:
            case dmGameSystemDDF::LabelDesc::PIVOT_SE:
                break;
        }
        return delta_pivot;
    }

    void InitParametersFromDescription(LabelComponent* label_component, dmGameSystemDDF::LabelDesc* label_desc)
    {
        label_component->m_Size     = Vector3(label_desc->m_Size[0], label_desc->m_Size[1], label_desc->m_Size[2]);
        label_component->m_Color    = Vector4(label_desc->m_Color[0], label_desc->m_Color[1], label_desc->m_Color[2], label_desc->m_Color[3]);
        label_component->m_Outline  = Vector4(label_desc->m_Outline[0], label_desc->m_Outline[1], label_desc->m_Outline[2], label_desc->m_Outline[3]);
        label_component->m_Shadow   = Vector4(label_desc->m_Shadow[0], label_desc->m_Shadow[1], label_desc->m_Shadow[2], label_desc->m_Shadow[3]);
        label_component->m_Pivot    = label_desc->m_Pivot;
        label_component->m_Text = label_desc->m_Text;
        label_component->m_ReHash = 1;
        label_component->m_TextLayoutDirty = 1;
        label_component->m_Leading = label_desc->m_Leading;
        label_component->m_Tracking = label_desc->m_Tracking;
        label_component->m_LineBreak = label_desc->m_LineBreak;
    }

    dmGameObject::CreateResult CompLabelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            ShowFullBufferError("Label", "label.max_count", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        LabelResource* resource = (LabelResource*)params.m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;

        uint32_t index = world->m_Components.Alloc();
        LabelComponent* component = &world->m_Components.Get(index);
        memset(component, 0, sizeof(LabelComponent));
        component->m_Instance = params.m_Instance;
        component->m_Scale    = params.m_Scale;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = resource;
        component->m_RenderConstants = 0;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Index = index;
        component->m_Enabled = 1;
        component->m_UserAllocatedText = 0;
        component->m_HoveredLinkObject = UINT32_MAX;
        component->m_PressedLinkObject = UINT32_MAX;

        InitParametersFromDescription(component, ddf);

        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent& component = *(LabelComponent*)*params.m_UserData;
        uint32_t index = component.m_Index;
        InvalidateTextLayout(&component);
        if (component.m_UserAllocatedText)
        {
            component.m_UserAllocatedText = 0;
            free((void*)component.m_Text);
        }
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Collection);
        if (component.m_Material) {
            dmResource::Release(factory, component.m_Material);
        }
        if (component.m_Font) {
            dmResource::Release(factory, component.m_Font);
        }
        if (component.m_RenderConstants)
        {
            dmGameSystem::DestroyRenderConstants(component.m_RenderConstants);
        }
        world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    Matrix4 CompLabelLocalTransform(const Point3& position, const Quat& rotation, const Vector3& scale, const Vector3& size, uint32_t pivot)
    {
        // Move pivot to (0,0). Rotate around (0,0). Move pivot to position.
        return dmTransform::ToMatrix4(
            dmTransform::Mul(
                dmTransform::Transform(Vector3(position), rotation, 1.0f),
                dmTransform::Transform(CalcPivotDelta(pivot, mulPerElem(scale, size)), Quat::identity(), 1.0f)
            )
        );
    }

    static void UpdateTransforms(LabelWorld* world, bool sub_pixels)
    {
        DM_PROFILE("UpdateTransforms");

        dmArray<LabelComponent>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent* c = &components[i];

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            Matrix4 local = CompLabelLocalTransform(c->m_Position, c->m_Rotation, c->m_Scale, c->m_Size, c->m_Pivot);
            Matrix4 world = dmGameObject::GetWorldMatrix(c->m_Instance);
            Matrix4 w = world * local;
            w = dmVMath::AppendScale(w, c->m_Scale);

            Vector4 position = w.getCol3();
            if (!sub_pixels)
            {
                position.setX((int) position.getX());
                position.setY((int) position.getY());
            }
            w.setCol3(position);
            c->m_World = w;
        }
    }

    dmGameObject::CreateResult CompLabelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        LabelComponent* component = (LabelComponent*)*params.m_UserData;
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLabelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        (void)update_result;

        LabelWorld* world = (LabelWorld*)params.m_World;
        dmArray<LabelComponent>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();

        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent* component = &components[i];

            if (!component->m_Enabled || !component->m_AddedToUpdate || !component->m_TextLayout)
            {
                continue;
            }

            TextLayoutUpdate(component->m_TextLayout, params.m_UpdateContext->m_DT);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLabelLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("LateUpdate");
        LabelContext* label_context = (LabelContext*)params.m_Context;
        LabelWorld* world = (LabelWorld*)params.m_World;

        UpdateTransforms(world, label_context->m_Subpixels);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void CreateDrawTextParams(LabelComponent* component, dmRender::DrawTextParams& params)
    {
        dmGameSystemDDF::LabelDesc* ddf = component->m_Resource->m_DDF;
        HTextLayout layout = GetOrCreateTextLayout(component);

        params.m_FaceColor = component->m_Color;
        params.m_OutlineColor = component->m_Outline;
        params.m_ShadowColor = component->m_Shadow;
        params.m_Text = layout ? 0 : component->m_Text;
        params.m_TextLayout = layout;
        params.m_WorldTransform = component->m_World;
        params.m_RenderOrder = 0;
        params.m_LineBreak = component->m_LineBreak;
        params.m_Leading = component->m_Leading;
        params.m_Tracking = component->m_Tracking;
        params.m_Width = component->m_Size.getX();
        params.m_Height = component->m_Size.getY();
        // Disable stencil
        params.m_StencilTestParamsSet = 0;

        switch (ddf->m_Pivot)
        {
        case dmGameSystemDDF::LabelDesc::PIVOT_NW:
            params.m_Align = dmRender::TEXT_ALIGN_LEFT;
            params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_N:
            params.m_Align = dmRender::TEXT_ALIGN_CENTER;
            params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_NE:
            params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
            params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_W:
            params.m_Align = dmRender::TEXT_ALIGN_LEFT;
            params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_CENTER:
            params.m_Align = dmRender::TEXT_ALIGN_CENTER;
            params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_E:
            params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
            params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_SW:
            params.m_Align = dmRender::TEXT_ALIGN_LEFT;
            params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_S:
            params.m_Align = dmRender::TEXT_ALIGN_CENTER;
            params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
            break;
        case dmGameSystemDDF::LabelDesc::PIVOT_SE:
            params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
            params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
            break;
        }

        // Taken from comp_sprite.cpp
        switch (ddf->m_BlendMode)
        {
            case dmGameSystemDDF::LabelDesc::BLEND_MODE_ALPHA:
                params.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                params.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::LabelDesc::BLEND_MODE_ADD:
                params.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                params.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::LabelDesc::BLEND_MODE_MULT:
                params.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                params.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::LabelDesc::BLEND_MODE_SCREEN:
                params.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                params.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Label: Unknown blend mode: %d\n", ddf->m_BlendMode);
                assert(0);
            break;
        }
    }

    dmGameObject::UpdateResult CompLabelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        DM_PROFILE("Render");
        LabelContext* label_context = (LabelContext*)params.m_Context;
        LabelWorld* world = (LabelWorld*)params.m_World;
        dmRender::HRenderContext render_context = label_context->m_RenderContext;

        dmArray<LabelComponent>& components = world->m_Components.GetRawObjects();
        uint32_t component_count = components.Size();

        DM_PROPERTY_ADD_U32(rmtp_Label, component_count);

        if (!component_count)
            return dmGameObject::UPDATE_RESULT_OK;

        UpdateTransforms(world, label_context->m_Subpixels);

        for (uint32_t i = 0; i < component_count; ++i)
        {
            LabelComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
                continue;

            if (component->m_ReHash || (component->m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component->m_RenderConstants)))
            {
                ReHash(component);
            }

            dmRender::DrawTextParams text_params;
            CreateDrawTextParams(component, text_params);

            if (component->m_RenderConstants)
            {
                uint32_t size = dmGameSystem::GetRenderConstantCount(component->m_RenderConstants);
                size = dmMath::Min<uint32_t>(size, dmRender::MAX_FONT_RENDER_CONSTANTS);
                for (uint32_t i = 0; i < size; ++i)
                {
                    text_params.m_RenderConstants[i] = dmGameSystem::GetRenderConstant(component->m_RenderConstants, i);
                }
                text_params.m_NumRenderConstants = size;
            }

            LabelResource* resource = component->m_Resource;
            dmRender::DrawText(render_context, GetFontMap(component, resource), GetMaterial(component, resource), component->m_MixedHash, text_params);
        }

        dmRender::FlushTexts(render_context, dmRender::RENDER_ORDER_WORLD, false);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompLabelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        LabelComponent* component = (LabelComponent*)user_data;
        return component->m_RenderConstants && dmGameSystem::GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompLabelSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        LabelComponent* component = (LabelComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();

        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, value_index, element_index, var);
        component->m_ReHash = 1;
    }

    dmGameObject::UpdateResult CompLabelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        LabelComponent* component = (LabelComponent*)*params.m_UserData;

        if (params.m_Message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)params.m_Message->m_Descriptor;
            dmDDF::ResolvePointers(descriptor, params.m_Message->m_Data);
        }

        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::SetText::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::SetText* textmsg = (dmGameSystemDDF::SetText*)params.m_Message->m_Data;
            if (component->m_UserAllocatedText)
            {
                free((void*)component->m_Text);
            }
            component->m_Text = strdup(textmsg->m_Text);
            component->m_UserAllocatedText = 1;
            InvalidateTextLayout(component);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static float LabelOffsetX(uint32_t align, float width)
    {
        if (align == dmRender::TEXT_ALIGN_RIGHT)
        {
            return width;
        }

        if (align == dmRender::TEXT_ALIGN_CENTER)
        {
            return width * 0.5f;
        }

        return 0.0f;
    }

    static float LabelLayoutY(uint32_t valign, float height, float layout_height)
    {
        if (valign == dmRender::TEXT_VALIGN_MIDDLE)
        {
            return (height - layout_height) * 0.5f;
        }

        if (valign == dmRender::TEXT_VALIGN_BOTTOM)
        {
            return 0.0f;
        }

        return height - layout_height;
    }

    static uint32_t HitTestLabelLink(LabelComponent* component, float world_x, float world_y)
    {
        HTextLayout layout = GetOrCreateTextLayout(component);

        if (!layout)
        {
            return UINT32_MAX;
        }

        const Vector4 local = inverse(component->m_World) * Vector4(world_x, world_y, 0.0f, 1.0f);
        dmRender::DrawTextParams params;
        CreateDrawTextParams(component, params);
        float layout_width;
        float layout_height;
        TextLayoutGetBounds(layout, &layout_width, &layout_height);
        (void)layout_width;

        TextGlyph* glyphs = TextLayoutGetGlyphs(layout);
        TextLine* lines = TextLayoutGetLines(layout);
        TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
        const TextLayoutObject* objects = TextLayoutGetObjects(layout);
        const float layout_y = LabelLayoutY(params.m_VAlign, params.m_Height, layout_height);

        for (uint32_t object_index = 0; object_index < TextLayoutGetObjectCount(layout); ++object_index)
        {
            const TextLayoutObject& object = objects[object_index];

            if (object.m_Tag != TAG_LINK || object.m_TextLength == 0)
            {
                continue;
            }

            const uint32_t link_end = object.m_TextOffset + object.m_TextLength;

            for (uint32_t line_index = 0; line_index < TextLayoutGetLineCount(layout); ++line_index)
            {
                const TextLine& line = lines[line_index];

                if (line.m_Length == 0)
                {
                    continue;
                }

                float first_x = glyphs[line.m_Index].m_X;

                for (uint32_t i = line.m_Index + 1; i < line.m_Index + line.m_Length; ++i)
                {
                    first_x = dmMath::Min(first_x, glyphs[i].m_X);
                }

                uint32_t align = params.m_Align;

                if (paragraphs[line.m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL)
                {
                    if (align == dmRender::TEXT_ALIGN_LEFT)
                    {
                        align = dmRender::TEXT_ALIGN_RIGHT;
                    }
                    else if (align == dmRender::TEXT_ALIGN_RIGHT)
                    {
                        align = dmRender::TEXT_ALIGN_LEFT;
                    }
                }

                const float line_x = LabelOffsetX(align, params.m_Width) - LabelOffsetX(align, line.m_Width);
                const float line_y = layout_y + line.m_Baseline;

                for (uint32_t i = line.m_Index; i < line.m_Index + line.m_Length; ++i)
                {
                    const TextGlyph& glyph = glyphs[i];

                    if (glyph.m_Cluster < object.m_TextOffset || glyph.m_Cluster >= link_end)
                    {
                        continue;
                    }

                    const float glyph_size = dmRender::GetFontMapSize(GetFontMap(component, component->m_Resource)) * glyph.m_RenderScale;
                    const float scale = FontGetScaleFromSize(glyph.m_Font, glyph_size);
                    const float ascent = FontGetAscent(glyph.m_Font, scale);
                    const float descent = fabsf(FontGetDescent(glyph.m_Font, scale));
                    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
                    TextGlyphRenderData render_data;
                    TextLayoutGetGlyphRenderData(layout, glyph, white, &render_data);
                    const float x = line_x + glyph.m_X - first_x + render_data.m_OffsetX;
                    const float y = line_y + glyph.m_Y + render_data.m_OffsetY;
                    const float width = dmMath::Max(glyph.m_Width, glyph_size * 0.35f);

                    if (local.getX() >= x && local.getX() <= x + width &&
                        local.getY() >= y - descent && local.getY() <= y + ascent)
                    {
                        return object_index;
                    }
                }
            }
        }

        return UINT32_MAX;
    }

    static const TextLayoutObjectAttribute* FindLabelLinkAttribute(HTextLayout layout, const TextLayoutObject& object, const char* name)
    {
        const char* source = TextLayoutGetObjectSource(layout);
        const TextLayoutObjectAttribute* attributes = TextLayoutGetObjectAttributes(layout);
        const uint32_t name_length = (uint32_t)strlen(name);

        for (uint32_t i = 0; i < object.m_AttributeCount; ++i)
        {
            const TextLayoutObjectAttribute& attribute = attributes[object.m_AttributeIndex + i];

            if (attribute.m_NameLength == name_length && memcmp(source + attribute.m_NameOffset, name, name_length) == 0)
            {
                return &attribute;
            }
        }

        return 0;
    }

    template <class Message>
    static void PostLabelLinkMessage(LabelComponent* component, uint32_t object_index)
    {
        HTextLayout layout = component->m_TextLayout;

        if (!layout || object_index >= TextLayoutGetObjectCount(layout))
        {
            return;
        }

        const TextLayoutObject& object = TextLayoutGetObjects(layout)[object_index];
        const TextLayoutObjectAttribute* src = FindLabelLinkAttribute(layout, object, "src");
        const char* source = TextLayoutGetObjectSource(layout);
        dmArray<char> src_value;
        src_value.SetCapacity(src ? src->m_ValueLength + 1 : 1);
        src_value.SetSize(src ? src->m_ValueLength + 1 : 1);

        if (src)
        {
            memcpy(src_value.Begin(), source + src->m_ValueOffset, src->m_ValueLength);
        }

        src_value.Back() = 0;

        Message message = {};
        message.m_Id = object.m_Id;
        message.m_Src = src_value.Begin();
        dmMessage::URL receiver;
        dmMessage::ResetURL(&receiver);
        receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
        receiver.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
        dmMessage::URL sender = receiver;

        if (dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment) == dmGameObject::RESULT_OK)
        {
            dmGameObject::PostDDF(&message, &sender, &receiver, 0, false);
        }
    }

    static void SetLabelLinkStyle(LabelComponent* component, uint32_t object_index, dmhash_t style)
    {
        HTextLayout layout = component->m_TextLayout;

        if (!layout || object_index >= TextLayoutGetObjectCount(layout))
        {
            return;
        }

        const TextLayoutObject& object = TextLayoutGetObjects(layout)[object_index];
        TextLayoutSetObjectStyle(layout, object.m_Id, style);
    }

    dmGameObject::InputResult CompLabelOnInput(const dmGameObject::ComponentOnInputParams& params)
    {
        LabelComponent* component = (LabelComponent*)*params.m_UserData;
        const dmGameObject::InputAction& action = *params.m_InputAction;

        if (!component->m_Enabled || !action.m_PositionSet)
        {
            return dmGameObject::INPUT_RESULT_IGNORED;
        }

        const uint32_t hovered = HitTestLabelLink(component, action.m_X, action.m_Y);

        if (hovered != component->m_HoveredLinkObject)
        {
            if (component->m_HoveredLinkObject != UINT32_MAX)
            {
                SetLabelLinkStyle(component, component->m_HoveredLinkObject, 0);
                PostLabelLinkMessage<dmGameSystemDDF::LabelLinkUnhovered>(component, component->m_HoveredLinkObject);
            }

            component->m_HoveredLinkObject = hovered;

            if (hovered != UINT32_MAX)
            {
                SetLabelLinkStyle(component, hovered, STYLE_LINK_HOVER);
                PostLabelLinkMessage<dmGameSystemDDF::LabelLinkHovered>(component, hovered);
            }
        }

        if (action.m_Pressed && hovered != UINT32_MAX)
        {
            component->m_PressedLinkObject = hovered;
            SetLabelLinkStyle(component, hovered, STYLE_LINK_ACTIVE);
        }

        if (component->m_PressedLinkObject != UINT32_MAX && hovered != component->m_PressedLinkObject && !action.m_Released)
        {
            SetLabelLinkStyle(component, component->m_PressedLinkObject, 0);
            component->m_PressedLinkObject = UINT32_MAX;
        }

        if (component->m_PressedLinkObject != UINT32_MAX && action.m_Released)
        {
            const uint32_t pressed = component->m_PressedLinkObject;
            SetLabelLinkStyle(component, pressed, hovered == pressed ? STYLE_LINK_HOVER : 0);
            component->m_PressedLinkObject = UINT32_MAX;

            if (hovered == pressed)
            {
                PostLabelLinkMessage<dmGameSystemDDF::LabelLinkClicked>(component, pressed);
            }
        }

        return dmGameObject::INPUT_RESULT_IGNORED;
    }

    void CompLabelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        LabelResource*              resource = (LabelResource*)params.m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;

        LabelComponent*             component = (LabelComponent*)*params.m_UserData;
        InvalidateTextLayout(component);
        InitParametersFromDescription(component, ddf);
    }

    dmGameObject::HComponent CompLabelGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (dmGameObject::HComponent)params.m_UserData;
    }

    // For testing
    void CompLabelGetTextMetrics(const LabelComponent* component, dmRender::TextMetrics& metrics)
    {
        LabelComponent*    mutable_component = const_cast<LabelComponent*>(component);
        dmRender::HFontMap font_map = GetFontMap(mutable_component, mutable_component->m_Resource);
        HTextLayout        layout = GetOrCreateTextLayout(mutable_component);
        dmRender::GetTextMetrics(font_map, layout, &metrics);
    }

    const char* CompLabelGetText(const LabelComponent* component)
    {
        return component->m_Text;
    }

    HTextLayout CompLabelGetTextLayout(LabelComponent* component)
    {
        return GetOrCreateTextLayout(component);
    }

    dmGameObject::PropertyResult CompLabelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        LabelComponent* component = (LabelComponent*)*params.m_UserData;
        dmhash_t get_property = params.m_PropertyId;

        if (IsReferencingProperty(LABEL_PROP_SCALE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Scale, LABEL_PROP_SCALE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SIZE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Size, LABEL_PROP_SIZE);
        }
        else if (IsReferencingProperty(LABEL_PROP_COLOR, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Color, LABEL_PROP_COLOR);
        }
        else if (IsReferencingProperty(LABEL_PROP_OUTLINE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Outline, LABEL_PROP_OUTLINE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SHADOW, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Shadow, LABEL_PROP_SHADOW);
        }
        else if (get_property == PROP_MATERIAL)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterialResource(component, component->m_Resource), out_value);
        }
        else if (get_property == PROP_FONT)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetFontResource(component, component->m_Resource), out_value);
        }
        else if (get_property == LABEL_PROP_LEADING)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Leading);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == LABEL_PROP_TRACKING)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_Tracking);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == LABEL_PROP_LINE_BREAK)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_LineBreak != 0);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);
        return GetMaterialConstant(GetMaterial(component, component->m_Resource), get_property, value_index, out_value, false, CompLabelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompLabelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        LabelComponent* component = (LabelComponent*)*params.m_UserData;
        dmhash_t set_property = params.m_PropertyId;

        if (IsReferencingProperty(LABEL_PROP_SCALE, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Scale, LABEL_PROP_SCALE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SIZE, set_property))
        {
            dmGameObject::PropertyResult res = SetProperty(set_property, params.m_Value, component->m_Size, LABEL_PROP_SIZE);
            if (res == dmGameObject::PROPERTY_RESULT_OK)
                InvalidateTextLayout(component);
            return res;
        }
        else if (IsReferencingProperty(LABEL_PROP_COLOR, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Color, LABEL_PROP_COLOR);
        }
        else if (IsReferencingProperty(LABEL_PROP_OUTLINE, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Outline, LABEL_PROP_OUTLINE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SHADOW, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Shadow, LABEL_PROP_SHADOW);
        }
        else if (set_property == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        else if (set_property == PROP_FONT)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, FONT_EXT_HASH, (void**)&component->m_Font);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            if (res == dmGameObject::PROPERTY_RESULT_OK)
                InvalidateTextLayout(component);
            return res;
        }
        else if (set_property == LABEL_PROP_LEADING)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            component->m_Leading = params.m_Value.m_Number;
            InvalidateTextLayout(component);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (set_property == LABEL_PROP_TRACKING)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            component->m_Tracking = params.m_Value.m_Number;
            InvalidateTextLayout(component);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (set_property == LABEL_PROP_LINE_BREAK)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_BOOLEAN)
            {
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
            component->m_LineBreak = params.m_Value.m_Bool;
            InvalidateTextLayout(component);
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), set_property, params.m_Value, value_index, CompLabelSetConstantCallback, component);
    }

    static bool CompLabelIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        LabelComponent* component = (LabelComponent*)pit->m_Node->m_Component;

        uint64_t index = pit->m_Next++;

        const char* property_names[] = {
            "position",
            "rotation",
            "scale",
            "size",
            "text"
        };
        uint32_t num_properties = DM_ARRAY_SIZE(property_names);
        if (index < 4)
        {
            int num_elements = 3;
            Vector4 value;
            switch(index)
            {
            case 0: value = Vector4(component->m_Position); break;
            case 1: value = Vector4(component->m_Rotation); num_elements = 4; break;
            case 2: value = Vector4(component->m_Scale); break;
            case 3: value = Vector4(component->m_Size); break;
            }

            pit->m_Property.m_NameHash = dmHashString64(property_names[index]);
            pit->m_Property.m_Type = num_elements == 3 ? dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3 : dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4;
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();

            return true;
        }
        else if (index == 4)
        {
            pit->m_Property.m_NameHash = dmHashString64(property_names[index]);
            pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_TEXT;
            pit->m_Property.m_Value.m_Text = CompLabelGetText(component);
            return true;
        }

        index -= num_properties;

        const char* world_property_names[] = {
            "world_position",
            "world_rotation",
            "world_scale",
            "world_size",
        };

        uint32_t num_world_properties = DM_ARRAY_SIZE(world_property_names);
        if (index < num_world_properties)
        {
            dmTransform::Transform transform = dmTransform::ToTransform(component->m_World);

            dmGameObject::SceneNodePropertyType type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3;
            Vector4 value;
            switch(index)
            {
                case 0: value = Vector4(transform.GetTranslation()); break;
                case 1: value = Vector4(transform.GetRotation()); type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4; break;
                case 2:
                    {
                        // Since the size is baked into the matrix, we divide by it here
                        Vector3 size( component->m_Size.getX() * component->m_Scale.getX(), component->m_Size.getY() * component->m_Scale.getY(), 1);
                        value = Vector4(dmVMath::DivPerElem(transform.GetScale(), size));
                    }
                    break;
                case 3: value = Vector4(transform.GetScale()); break; // the size is baked into this matrix as the scale
                default:
                    return false;
            }

            pit->m_Property.m_Type = type;
            pit->m_Property.m_NameHash = dmHashString64(world_property_names[index]);
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();
            return true;
        }
        index -= num_world_properties;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = component->m_Enabled;
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    void CompLabelIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompLabelIterPropertiesGetNext;
    }
}
