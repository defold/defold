// Copyright 2021 The Defold Foundation
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

#include "res_ttf.h"
#include <dmsdk/dlib/log.h>
#include <dmsdk/font/font.h>
#include <dmsdk/resource/resource.h>

namespace dmGameSystem
{

struct TTFResource
{
    HFont m_Font;
};

static void DeleteResource(TTFResource* resource)
{
    FontDestroy(resource->m_Font);
    delete resource;
}

static dmResource::Result TTF_Create(const dmResource::ResourceCreateParams* params)
{
    HFont font = FontLoadFromMemory(params->m_Filename, (void*)params->m_Buffer, params->m_BufferSize, true);
    if (!font)
    {
        dmLogError("Failed to load font from '%s'", params->m_Filename);
        return dmResource::RESULT_INVALID_DATA;
    }
    TTFResource* resource = new TTFResource;
    resource->m_Font = font;

    dmResource::SetResource(params->m_Resource, resource);
    dmResource::SetResourceSize(params->m_Resource, FontGetResourceSize(resource->m_Font));
    return dmResource::RESULT_OK;
}

static dmResource::Result TTF_Destroy(const dmResource::ResourceDestroyParams* params)
{
    TTFResource* resource = (TTFResource*)dmResource::GetResource(params->m_Resource);
    DeleteResource(resource);
    return dmResource::RESULT_OK;
}

static dmResource::Result TTF_Recreate(const dmResource::ResourceRecreateParams* params)
{
    HFont new_font = FontLoadFromMemory(params->m_Filename, (void*)params->m_Buffer, params->m_BufferSize, true);
    if (!new_font)
    {
        dmLogError("Failed to reload font from '%s'", params->m_Filename);
        return dmResource::RESULT_INVALID_DATA;
    }

    // We need to keep the original pointer as it may be used elsewhere
    TTFResource* resource = (TTFResource*)dmResource::GetResource(params->m_Resource);
    HFont old_font = resource->m_Font;
    resource->m_Font = new_font;

    FontDestroy(old_font);

    dmResource::SetResourceSize(params->m_Resource, FontGetResourceSize(resource->m_Font));
    return dmResource::RESULT_OK;
}

static ResourceResult RegisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
{
    return (ResourceResult)dmResource::SetupType(ctx,
                                                 type,
                                                 0, // context
                                                 0, // preload
                                                 TTF_Create,
                                                 0, // post create
                                                 TTF_Destroy,
                                                 TTF_Recreate);

}

static ResourceResult DeregisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
{
    return RESOURCE_RESULT_OK;
}

 // Fontgen api

HFont GetFont(TTFResource* resource)
{
    return resource->m_Font;
}

} // namespace


DM_DECLARE_RESOURCE_TYPE(ResourceTypeTTF, "ttf", dmGameSystem::RegisterResourceType_TTFFont, dmGameSystem::DeregisterResourceType_TTFFont);
