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

#include <dmsdk/dlib/vmath.h>

#include <gamesys/gamesys.h>

#include "res_light.h"

namespace dmGameSystem
{
    static const dmStructDDF::Struct::FieldsEntry* FindField(const dmStructDDF::Struct* s, const char* key)
    {
        if (!s)
            return 0x0;

        for (uint32_t i = 0; i < s->m_Fields.m_Count; ++i)
        {
            if (strcmp(s->m_Fields[i].m_Key, key) == 0)
                return &s->m_Fields[i];
        }
        return 0x0;
    }

    static void GetNumber(const dmStructDDF::Struct* s, const char* key, float* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (f)
        {
            *out = (float) f->m_Value->m_Kind.m_NumberValue;
        }
    }

    static void GetVector3(const dmStructDDF::Struct* s, const char* key, dmVMath::Vector3* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (!f)
        {
            return;
        }

        dmStructDDF::ListValue* list = f->m_Value->m_Kind.m_ListValue;
        if (!list || list->m_Values.m_Count < 3)
        {
            return;
        }

        out->setX((float) list->m_Values[0].m_Kind.m_NumberValue);
        out->setY((float) list->m_Values[1].m_Kind.m_NumberValue);
        out->setZ((float) list->m_Values[2].m_Kind.m_NumberValue);
    }

    static void GetVector4(const dmStructDDF::Struct* s, const char* key, dmVMath::Vector4* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (!f)
        {
            return;
        }

        dmStructDDF::ListValue* list = f->m_Value->m_Kind.m_ListValue;
        if (!list || list->m_Values.m_Count < 3)
        {
            return;
        }

        float r = (float) list->m_Values[0].m_Kind.m_NumberValue;
        float g = (float) list->m_Values[1].m_Kind.m_NumberValue;
        float b = (float) list->m_Values[2].m_Kind.m_NumberValue;
        float a = (list->m_Values.m_Count >= 4)
                    ? (float) list->m_Values[3].m_Kind.m_NumberValue
                    : 1.0f;

        *out = dmVMath::Vector4(r, g, b, a);
    }

    static void DDFToLightParams(const dmGameSystemDDF::LightDesc* light_desc, dmRender::LightParams& params)
    {
        if (light_desc == 0x0)
            return;

        const dmStructDDF::Struct* data = &light_desc->m_Data;

        // Shared properties
        GetVector4(data, "color", &params.m_Color);
        GetNumber(data, "intensity", &params.m_Intensity);

        const dmStructDDF::Struct::FieldsEntry* type_field = FindField(data, "type");
        if (type_field)
        {
            const char* type_str = type_field->m_Value->m_Kind.m_StringValue;
            if (type_str)
            {
                if (strcmp(type_str, "directional") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_DIRECTIONAL;
                    GetVector3(data, "direction", &params.m_Direction);
                }
                else if (strcmp(type_str, "point") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_POINT;
                    GetNumber(data, "range", &params.m_Range);
                }
                else if (strcmp(type_str, "spot") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_SPOT;
                    GetNumber(data, "range", &params.m_Range);
                    GetNumber(data, "inner_cone_angle", &params.m_InnerConeAngle);
                    GetNumber(data, "outer_cone_angle", &params.m_OuterConeAngle);
                }
            }
        }
    }

    dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams* params)
    {
        LightResource* light_resource = new LightResource();
        memset(light_resource, 0, sizeof(LightResource));

        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;

        light_resource->m_DDF = (dmGameSystemDDF::LightDesc*) params->m_PreloadData;

        dmRender::LightParams light_params;
        DDFToLightParams(light_resource->m_DDF, light_params);

        light_resource->m_Light = dmRender::NewLight(render_context, light_params);

        dmResource::SetResource(params->m_Resource, light_resource);

        return dmResource::RESULT_OK;
    }

    static inline void ReleaseResources(dmResource::HFactory factory, dmRender::HRenderContext render_context, LightResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Light)
            dmRender::DeleteLight(render_context, resource->m_Light);
    }

    dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams* params)
    {
        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        ReleaseResources(params->m_Factory, render_context, light_resource);
        delete light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::LightDesc *ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightRecreate(const dmResource::ResourceRecreateParams* params)
    {
        LightResource tmp_light_resource = {};

        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &tmp_light_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        ReleaseResources(params->m_Factory, render_context, light_resource);
        *light_resource = tmp_light_resource;

        return dmResource::RESULT_OK;
    }
}
