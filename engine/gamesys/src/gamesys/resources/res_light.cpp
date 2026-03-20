// Copyright 2020-2026 The Defold Foundation
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

#include <dmsdk/resource/resource.h>
#include <dmsdk/gamesys/resources/res_light.h>

#include <render/render.h>

namespace dmGameSystem
{
    enum LightParseResult
    {
        LIGHT_PARSE_RESULT_OK = 0,
        LIGHT_PARSE_RESULT_KEY_NOT_FOUND = -1,
        LIGHT_PARSE_RESULT_INVALID_TYPE = -2,
        LIGHT_PARSE_RESULT_INVALID_DATA = -3,
    };

    struct LightResource
    {
        dmRender::HLightPrototype m_LightPrototype;
    };

    dmRender::LightPrototype* GetLightPrototype(LightResource* res)
    {
        return res ? (dmRender::LightPrototype*)res->m_LightPrototype : (dmRender::LightPrototype*)0;
    }

    static const char* ParseResultToStr(LightParseResult res)
    {
        switch(res)
        {
            case LIGHT_PARSE_RESULT_OK: return "LIGHT_PARSE_RESULT_OK";
            case LIGHT_PARSE_RESULT_KEY_NOT_FOUND: return "LIGHT_PARSE_RESULT_KEY_NOT_FOUND";
            case LIGHT_PARSE_RESULT_INVALID_TYPE: return "LIGHT_PARSE_RESULT_INVALID_TYPE";
            case LIGHT_PARSE_RESULT_INVALID_DATA: return "LIGHT_PARSE_RESULT_INVALID_DATA";
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
            {
                return &s->m_Fields[i];
            }
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
        *out = (float) f->m_Value->m_Kind.m_Number;
        return LIGHT_PARSE_RESULT_OK;
    }

    static LightParseResult GetVector4(const dmStructDDF::Struct* s, const char* key, dmVMath::Vector4* out)
    {
        const dmStructDDF::Struct::FieldsEntry* f = FindField(s, key);
        if (!f)
        {
            return LIGHT_PARSE_RESULT_KEY_NOT_FOUND;
        }

        dmStructDDF::ListValue* list = f->m_Value->m_Kind.m_List;
        if (!list || list->m_Values.m_Count < 3)
        {
            return LIGHT_PARSE_RESULT_INVALID_TYPE;
        }

        float r = (float) list->m_Values[0].m_Kind.m_Number;
        float g = (float) list->m_Values[1].m_Kind.m_Number;
        float b = (float) list->m_Values[2].m_Kind.m_Number;
        float a = (list->m_Values.m_Count >= 4)
                    ? (float) list->m_Values[3].m_Kind.m_Number
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

    static LightParseResult DDFToLightParams(const dmGameSystemDDF::Data* ddf, dmRender::LightPrototypeParams& params)
    {
        // Parse tags to determine the light type
        dmRender::LightType type = dmRender::LIGHT_TYPE_POINT;
        for (uint32_t i=0; i < ddf->m_Tags.m_Count; ++i)
        {
            const char* tag = ddf->m_Tags[i];
            if (strcmp(tag, "directional_light") == 0)
            {
                type = dmRender::LIGHT_TYPE_DIRECTIONAL;
            }
            else if (strcmp(tag, "point_light") == 0)
            {
                type = dmRender::LIGHT_TYPE_POINT;
            }
            else if (strcmp(tag, "spot_light") == 0)
            {
                type = dmRender::LIGHT_TYPE_SPOT;
            }
        }

        params.m_Type = type;

        // Parse the data from the ddf data field
        const dmStructDDF::Value* data = &ddf->m_Data;
        LightParseResult res = LIGHT_PARSE_RESULT_OK;

    #define HANDLE_LIGHT_PARSE_RES(lbl, r) \
        if (r != LIGHT_PARSE_RESULT_OK) \
        { \
            dmLogError("Error parsing light data for %s: error=%s", lbl, ParseResultToStr(r)); \
            return r; \
        }

        const dmStructDDF::Struct light_data = data->m_Kind.m_Struct;

        // Shared properties
        res = GetVector4(&light_data, "color", &params.m_Color);
        HANDLE_LIGHT_PARSE_RES("color", res);

        res = GetNumber(&light_data, "intensity", &params.m_Intensity);
        HANDLE_LIGHT_PARSE_RES("intensity", res);

        // Light type specific properties
        if (type == dmRender::LIGHT_TYPE_DIRECTIONAL)
        {
            res = GetVector3(&light_data, "direction", &params.m_Direction);
            HANDLE_LIGHT_PARSE_RES("directional.direction", res);
        }
        else if (type == dmRender::LIGHT_TYPE_POINT)
        {
            res = GetNumber(&light_data, "range", &params.m_Range);
            HANDLE_LIGHT_PARSE_RES("point.range", res);
        }
        else if (type == dmRender::LIGHT_TYPE_SPOT)
        {
            res = GetNumber(&light_data, "range", &params.m_Range);
            HANDLE_LIGHT_PARSE_RES("spot.range", res);
            res = GetNumber(&light_data, "inner_cone_angle", &params.m_InnerConeAngle);
            HANDLE_LIGHT_PARSE_RES("spot.inner_cone_angle", res);
            res = GetNumber(&light_data, "outer_cone_angle", &params.m_OuterConeAngle);
            HANDLE_LIGHT_PARSE_RES("spot.outer_cone_angle", res);
        }
    #undef HANDLE_LIGHT_PARSE_RES

        return LIGHT_PARSE_RESULT_OK;
    }

    static dmResource::Result ResLightPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmGameSystemDDF::Data* ddf = 0;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameSystemDDF::Data>(params->m_Buffer, params->m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        dmGameSystemDDF::Data* ddf = (dmGameSystemDDF::Data*)params->m_PreloadData;

        dmRender::LightPrototypeParams prototype_params;
        if (DDFToLightParams(ddf, prototype_params) != LIGHT_PARSE_RESULT_OK)
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        LightResource* resource = new LightResource();
        resource->m_LightPrototype = dmRender::NewLightPrototype(render_context, prototype_params);

        dmResource::SetResource(params->m_Resource, resource);
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        return dmResource::RESULT_OK;
    }

    static inline void ReleaseResources(dmResource::HFactory factory, dmRender::HRenderContext render_context, LightResource* resource)
    {
        if (!resource)
            return;

        if (resource->m_LightPrototype)
        {
            dmRender::DeleteLightPrototype(render_context, resource->m_LightPrototype);
            // Avoid double-deletion if recreate/destroy happens after a failed reload.
            resource->m_LightPrototype = 0;
        }
    }

    static dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams* params)
    {
        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        if (!light_resource)
            return dmResource::RESULT_OK;
        ReleaseResources(params->m_Factory, render_context, light_resource);
        delete light_resource;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResLightRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGameSystemDDF::Data* ddf;

        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        ReleaseResources(params->m_Factory, render_context, light_resource);

        dmRender::LightPrototypeParams light_params;
        if (DDFToLightParams(ddf, light_params) != LIGHT_PARSE_RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        light_resource->m_LightPrototype = dmRender::NewLightPrototype(render_context, light_params);

        dmDDF::FreeMessage(ddf);

        return dmResource::RESULT_OK;
    }

    static ResourceResult RegisterResourceType_Light(HResourceTypeContext ctx, HResourceType type)
    {
        // Same pattern as fontc: engine.cpp maps extension hash -> shared context (see m_ResourceTypeContexts).
        void* render_context = ResourceTypeContextGetContextByHash(ctx, ResourceTypeGetNameHash(type));
        if (render_context == 0)
        {
            dmLogError("Missing resource context for 'lightc' when registering resource type 'lightc' (add lightc to resource type contexts, e.g. next to fontc)");
            return RESOURCE_RESULT_INVAL;
        }

        return (ResourceResult) dmResource::SetupType(ctx,
                                                      type,
                                                      render_context,
                                                      ResLightPreload,
                                                      ResLightCreate,
                                                      0, // post create
                                                      ResLightDestroy,
                                                      ResLightRecreate);
    }

    static ResourceResult DeregisterResourceType_Light(HResourceTypeContext ctx, HResourceType type)
    {
        return RESOURCE_RESULT_OK;
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeLight, "lightc", dmGameSystem::RegisterResourceType_Light, dmGameSystem::DeregisterResourceType_Light);

