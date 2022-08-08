// Copyright 2020-2022 The Defold Foundation
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
#include <render/font_renderer.h>
#include <gameobject/gameobject_ddf.h>

#include "../resources/res_label.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include <gamesys/label_ddf.h>
#include <gamesys/gamesys_ddf.h>
#include <dmsdk/gamesys/render_constants.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Label, 0, FrameReset, "# components", &rmtp_Components);

namespace dmGameSystem
{
    using namespace dmVMath;

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
        dmRender::HMaterial         m_Material;
        dmRender::HFontMap          m_FontMap;

        const char*                 m_Text;

        uint16_t                    m_ComponentIndex;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_UserAllocatedText : 1;
        uint16_t                    m_ReHash : 1;
        uint16_t                    m_Padding : 4;
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

    dmGameObject::CreateResult CompLabelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        LabelContext* label_context = (LabelContext*)params.m_Context;
        LabelWorld* world = new LabelWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, label_context->m_MaxLabelCount);
        world->m_Components.SetCapacity(comp_count);
        memset(world->m_Components.m_Objects.Begin(), 0, sizeof(LabelComponent) * comp_count);

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;

        LabelComponent* components = world->m_Components.m_Objects.Begin();
        for (uint32_t i = 0; i < world->m_Components.m_Objects.Size(); ++i )
        {
            LabelComponent& component = components[i];
            if (component.m_UserAllocatedText)
            {
                free((void*)component.m_Text);
            }
        }

        delete world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline dmRender::HMaterial GetMaterial(LabelComponent* component, LabelResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline dmRender::HFontMap GetFontMap(const LabelComponent* component, const LabelResource* resource) {
        return component->m_FontMap ? component->m_FontMap : resource->m_FontMap;
    }

    void ReHash(LabelComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        LabelResource* resource = component->m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmRender::HFontMap font_map = GetFontMap(component, resource);

        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, &font_map, sizeof(font_map));
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

    dmGameObject::CreateResult CompLabelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Label could not be created since the label buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        LabelResource* resource = (LabelResource*)params.m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;

        uint32_t index = world->m_Components.Alloc();
        LabelComponent* component = &world->m_Components.Get(index);
        memset(component, 0, sizeof(LabelComponent));
        component->m_Instance = params.m_Instance;
        component->m_Size     = Vector3(ddf->m_Size[0], ddf->m_Size[1], ddf->m_Size[2]);
        component->m_Scale    = params.m_Scale;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Color    = Vector4(ddf->m_Color[0], ddf->m_Color[1], ddf->m_Color[2], ddf->m_Color[3]);
        component->m_Outline  = Vector4(ddf->m_Outline[0], ddf->m_Outline[1], ddf->m_Outline[2], ddf->m_Outline[3]);
        component->m_Shadow   = Vector4(ddf->m_Shadow[0], ddf->m_Shadow[1], ddf->m_Shadow[2], ddf->m_Shadow[3]);
        component->m_Resource = resource;
        component->m_Pivot    = ddf->m_Pivot;
        component->m_RenderConstants = 0;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Text = ddf->m_Text;
        component->m_UserAllocatedText = 0;
        component->m_ReHash = 1;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompLabelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        uint32_t index = *params.m_UserData;

        LabelComponent& component = world->m_Components.Get(index);
        if (component.m_UserAllocatedText)
        {
            component.m_UserAllocatedText = 0;
            free((void*)component.m_Text);
        }
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Collection);
        if (component.m_Material) {
            dmResource::Release(factory, component.m_Material);
        }
        if (component.m_FontMap) {
            dmResource::Release(factory, component.m_FontMap);
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

        dmArray<LabelComponent>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            LabelComponent* c = &components[i];

            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            Matrix4 local = CompLabelLocalTransform(c->m_Position, c->m_Rotation, c->m_Scale, c->m_Size, c->m_Pivot);
            Matrix4 world = dmGameObject::GetWorldMatrix(c->m_Instance);
            Matrix4 w;

            if (dmGameObject::ScaleAlongZ(c->m_Instance))
            {
                w = world * local;
            }
            else
            {
                w = dmTransform::MulNoScaleZ(world, local);
            }

            w = appendScale(w, c->m_Scale);
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
        LabelWorld* world = (LabelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        LabelComponent* component = &world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompLabelUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        (void)params;
        (void)update_result;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void CreateDrawTextParams(LabelComponent* component, dmRender::DrawTextParams& params)
    {
        dmGameSystemDDF::LabelDesc* ddf = component->m_Resource->m_DDF;

        params.m_FaceColor = component->m_Color;
        params.m_OutlineColor = component->m_Outline;
        params.m_ShadowColor = component->m_Shadow;
        params.m_Text = component->m_Text;
        params.m_WorldTransform = component->m_World;
        params.m_RenderOrder = 0;
        params.m_LineBreak = ddf->m_LineBreak;
        params.m_Leading = ddf->m_Leading;
        params.m_Tracking = ddf->m_Tracking;
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

        dmArray<LabelComponent>& components = world->m_Components.m_Objects;
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

        dmRender::FlushTexts(render_context, dmRender::RENDER_ORDER_WORLD, 0, false);
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
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);

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
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompLabelOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        (void)params;
    }

    void* CompLabelGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        return &world->m_Components.Get(index);
    }

    void CompLabelGetTextMetrics(const LabelComponent* component, struct dmRender::TextMetrics& metrics)
    {
        LabelResource* resource = component->m_Resource;
        dmGameSystemDDF::LabelDesc* ddf = resource->m_DDF;
        dmRender::HFontMap font_map = GetFontMap(component, resource);
        dmRender::GetTextMetrics(font_map, component->m_Text, component->m_Size.getX(),
                                    ddf->m_LineBreak, ddf->m_Leading, ddf->m_Tracking, &metrics);

        metrics.m_Width      = metrics.m_Width;
        metrics.m_Height     = metrics.m_Height;
        metrics.m_MaxAscent  = metrics.m_MaxAscent;
        metrics.m_MaxDescent = metrics.m_MaxDescent;
    }

    const char* CompLabelGetText(const LabelComponent* component)
    {
        return component->m_Text;
    }

    dmGameObject::PropertyResult CompLabelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);
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
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterial(component, component->m_Resource), out_value);
        }
        else if (get_property == PROP_FONT)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetFontMap(component, component->m_Resource), out_value);
        }
        return GetMaterialConstant(GetMaterial(component, component->m_Resource), get_property, params.m_Options.m_Index, out_value, false, CompLabelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompLabelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        LabelWorld* world = (LabelWorld*)params.m_World;
        LabelComponent* component = &world->m_Components.Get(*params.m_UserData);
        dmhash_t set_property = params.m_PropertyId;

        if (IsReferencingProperty(LABEL_PROP_SCALE, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Scale, LABEL_PROP_SCALE);
        }
        else if (IsReferencingProperty(LABEL_PROP_SIZE, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Size, LABEL_PROP_SIZE);
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
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, FONT_EXT_HASH, (void**)&component->m_FontMap);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), set_property, params.m_Value, params.m_Options.m_Index, CompLabelSetConstantCallback, component);
    }

    static bool CompLabelIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        LabelWorld* label_world = (LabelWorld*)pit->m_Node->m_ComponentWorld;
        LabelComponent* component = &label_world->m_Components.Get(pit->m_Node->m_Component);

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
                        value = Vector4(Vectormath::Aos::divPerElem(transform.GetScale(), size));
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
