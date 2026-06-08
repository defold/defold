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

#include <ddf/ddf.h>
#include <dlib/log.h>
#include <dlib/math.h>
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

    dmRender::HLightPrototype GetLightPrototype(LightResource* res)
    {
        return res ? res->m_LightPrototype : (dmRender::HLightPrototype)0;
    }

    static const char* LightTypeToStr(dmRender::LightType type)
    {
        switch (type)
        {
            case dmRender::LIGHT_TYPE_DIRECTIONAL: return "directional";
            case dmRender::LIGHT_TYPE_POINT:       return "point";
            case dmRender::LIGHT_TYPE_SPOT:        return "spot";
            case dmRender::LIGHT_TYPE_AMBIENT:     return "ambient";
            default:                               return "<unknown>";
        }
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
            else if (strcmp(tag, "ambient_light") == 0)
            {
                type = dmRender::LIGHT_TYPE_AMBIENT;
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

        // Shared properties. Source validation and unit normalization happen in the build pipeline.
        res = GetVector4(&light_data, "color", &params.m_Color);
        HANDLE_LIGHT_PARSE_RES("color", res);

        res = GetNumber(&light_data, "intensity", &params.m_Intensity);
        HANDLE_LIGHT_PARSE_RES("intensity", res);

        // Light type specific properties
        if (type == dmRender::LIGHT_TYPE_DIRECTIONAL)
        {
            // Direction is derived from game object rotation applied to (0, 0, -1).
        }
        else if (type == dmRender::LIGHT_TYPE_AMBIENT)
        {
            // Ambient light has no transform-dependent data.
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
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        LightResource* resource = new LightResource();
        resource->m_LightPrototype = dmRender::NewLightPrototype(render_context, prototype_params);

        dmLogInfo("TEMP LIGHT ResLightCreate resource=%p render_context=%p prototype=%u type=%s color=(%.3f, %.3f, %.3f, %.3f) intensity=%.3f range=%.3f inner=%.3f outer=%.3f tags=%u",
                  (void*) params->m_Resource,
                  (void*) render_context,
                  (uint32_t) resource->m_LightPrototype,
                  LightTypeToStr(prototype_params.m_Type),
                  prototype_params.m_Color.getX(),
                  prototype_params.m_Color.getY(),
                  prototype_params.m_Color.getZ(),
                  prototype_params.m_Color.getW(),
                  prototype_params.m_Intensity,
                  prototype_params.m_Range,
                  prototype_params.m_InnerConeAngle,
                  prototype_params.m_OuterConeAngle,
                  ddf->m_Tags.m_Count);

        dmResource::SetResource(params->m_Resource, resource);
        dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize);

        dmDDF::FreeMessage(ddf);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams* params)
    {
        LightResource* light_resource = (LightResource*) dmResource::GetResource(params->m_Resource);
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params->m_Context;
        if (!light_resource)
            return dmResource::RESULT_OK;
        dmLogInfo("TEMP LIGHT ResLightDestroy resource=%p render_context=%p prototype=%u",
                  (void*) params->m_Resource,
                  (void*) render_context,
                  (uint32_t) light_resource->m_LightPrototype);
        dmRender::DeleteLightPrototype(render_context, light_resource->m_LightPrototype);
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

        dmRender::LightPrototypeParams light_params;
        if (DDFToLightParams(ddf, light_params) != LIGHT_PARSE_RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return dmResource::RESULT_FORMAT_ERROR;
        }

        // No need to delete the prototype, we can just update its data. Otherwise, we would have to
        // re-link all pointers in the light components to use the new pointer.
        dmRender::SetLightPrototype(render_context, light_resource->m_LightPrototype, light_params);

        dmLogInfo("TEMP LIGHT ResLightRecreate resource=%p render_context=%p prototype=%u type=%s intensity=%.3f range=%.3f",
                  (void*) params->m_Resource,
                  (void*) render_context,
                  (uint32_t) light_resource->m_LightPrototype,
                  LightTypeToStr(light_params.m_Type),
                  light_params.m_Intensity,
                  light_params.m_Range);

        dmDDF::FreeMessage(ddf);

        return dmResource::RESULT_OK;
    }

    static ResourceResult RegisterResourceType_Light(HResourceTypeContext ctx, HResourceType type)
    {
        // Same pattern as fontc: engine.cpp maps extension hash -> shared context (see m_ResourceTypeContexts).
        void* render_context = ResourceTypeContextGetContextByHash(ctx, ResourceTypeGetNameHash(type));
        dmLogInfo("TEMP LIGHT RegisterResourceType_Light type=%p name_hash=%llu render_context=%p",
                  (void*) type,
                  (unsigned long long) ResourceTypeGetNameHash(type),
                  render_context);
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
