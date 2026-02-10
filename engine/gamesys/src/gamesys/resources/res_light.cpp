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
    enum LightParseResult
    {
        LIGHT_PARSE_RESULT_OK = 0,
        LIGHT_PARSE_RESULT_KEY_NOT_FOUND = -1,
        LIGHT_PARSE_RESULT_INVALID_TYPE = -2,
    };

    static const char* ParseResultToStr(LightParseResult res)
    {
        switch(res)
        {
            case LIGHT_PARSE_RESULT_KEY_NOT_FOUND: return "LIGHT_PARSE_RESULT_KEY_NOT_FOUND";
            case LIGHT_PARSE_RESULT_INVALID_TYPE: return "LIGHT_PARSE_RESULT_INVALID_TYPE";
            default:break;
        }
        return "<unknown error>";
    }

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

    static LightParseResult GetNumber(const dmStructDDF::Struct* s, const char* key, float* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (!f)
        {
            return LIGHT_PARSE_RESULT_KEY_NOT_FOUND;
        }
        *out = (float) f->m_Value->m_Kind.m_NumberValue;
        return LIGHT_PARSE_RESULT_OK;
    }

    static LightParseResult GetVector4(const dmStructDDF::Struct* s, const char* key, dmVMath::Vector4* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (!f)
        {
            return LIGHT_PARSE_RESULT_KEY_NOT_FOUND;
        }

        dmStructDDF::ListValue* list = f->m_Value->m_Kind.m_ListValue;
        if (!list || list->m_Values.m_Count < 3)
        {
            return LIGHT_PARSE_RESULT_INVALID_TYPE;
        }

        float r = (float) list->m_Values[0].m_Kind.m_NumberValue;
        float g = (float) list->m_Values[1].m_Kind.m_NumberValue;
        float b = (float) list->m_Values[2].m_Kind.m_NumberValue;
        float a = (list->m_Values.m_Count >= 4)
                    ? (float) list->m_Values[3].m_Kind.m_NumberValue
                    : 1.0f;

        *out = dmVMath::Vector4(r, g, b, a);
        return LIGHT_PARSE_RESULT_OK;
    }

    static LightParseResult GetVector3(const dmStructDDF::Struct* s, const char* key, dmVMath::Vector3* out)
    {
        dmVMath::Vector4 v4;
        LightParseResult res = GetVector4(s, key, &v4);
        if (res != LIGHT_PARSE_RESULT_OK)
        {
            return res;
        }
        *out = v4.getXYZ();
        return LIGHT_PARSE_RESULT_OK;
    }

    static void DDFToLightParams(const dmGameSystemDDF::LightDesc* light_desc, dmRender::LightPrototypeParams& params)
    {
        if (light_desc == 0x0)
            return;

        const dmStructDDF::Struct* data = &light_desc->m_Data;
        LightParseResult res = LIGHT_PARSE_RESULT_OK;

    #define HANDLE_LIGHT_PARSE_RES(lbl, r) \
        if (res != LIGHT_PARSE_RESULT_OK) \
            dmLogError("Error parsing light data for %s: error=%s", lbl, ParseResultToStr(r));

        // Shared properties
        res = GetVector4(data, "color", &params.m_Color);
        HANDLE_LIGHT_PARSE_RES("color", res);

        res = GetNumber(data, "intensity", &params.m_Intensity);
        HANDLE_LIGHT_PARSE_RES("intensity", res);

        const dmStructDDF::Struct::FieldsEntry* type_field = FindField(data, "type");
        if (type_field)
        {
            const char* type_str = type_field->m_Value->m_Kind.m_StringValue;
            if (type_str)
            {
                if (strcmp(type_str, "directional") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_DIRECTIONAL;
                    res = GetVector3(data, "direction", &params.m_Direction);
                    HANDLE_LIGHT_PARSE_RES("directional.direction", res);
                }
                else if (strcmp(type_str, "point") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_POINT;
                    res = GetNumber(data, "range", &params.m_Range);
                    HANDLE_LIGHT_PARSE_RES("point.range", res);
                }
                else if (strcmp(type_str, "spot") == 0)
                {
                    params.m_Type = dmRender::LIGHT_TYPE_SPOT;
                    res = GetNumber(data, "range", &params.m_Range);
                    HANDLE_LIGHT_PARSE_RES("spot.range",res);
                    res = GetNumber(data, "inner_cone_angle", &params.m_InnerConeAngle);
                    HANDLE_LIGHT_PARSE_RES("spot.inner_cone_angle", res);
                    res = GetNumber(data, "outer_cone_angle", &params.m_OuterConeAngle);
                    HANDLE_LIGHT_PARSE_RES("spot.outer_cone_angle", res);
                }
            }
        }
    #undef HANDLE_LIGHT_PARSE_RES
    }

    dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams* params)
    {
        LightResource* light_resource = new LightResource();
        memset(light_resource, 0, sizeof(LightResource));

        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;

        dmGameSystemDDF::LightDesc* ddf = (dmGameSystemDDF::LightDesc*) params->m_PreloadData;

        dmRender::LightPrototypeParams light_params;
        DDFToLightParams(ddf, light_params);

        light_resource->m_LightPrototype = dmRender::NewLightPrototype(render_context, light_params);

        dmResource::SetResource(params->m_Resource, light_resource);

        dmDDF::FreeMessage(ddf);

        return dmResource::RESULT_OK;
    }

    static inline void ReleaseResources(dmResource::HFactory factory, dmRender::HRenderContext render_context, LightResource* resource)
    {
        if (resource->m_LightPrototype)
            dmRender::DeleteLightPrototype(render_context, resource->m_LightPrototype);
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
        dmGameSystemDDF::LightDesc* ddf;
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
        dmGameSystemDDF::LightDesc* ddf;

        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        ReleaseResources(params->m_Factory, render_context, light_resource);

        dmRender::LightPrototypeParams light_params;
        DDFToLightParams(ddf, light_params);
        light_resource->m_LightPrototype = dmRender::NewLightPrototype(render_context, light_params);

        dmDDF::FreeMessage(ddf);

        return dmResource::RESULT_OK;
    }
}
