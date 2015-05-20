#include "res_display_profiles.h"

#include <dlib/log.h>
#include <render/render_ddf.h>
#include <render/display_profiles.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, dmRender::HDisplayProfiles profiles, const char* filename)
    {
        dmRenderDDF::DisplayProfiles* display_profiles_ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::DisplayProfiles>(buffer, buffer_size, (&display_profiles_ddf));
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmRender::DisplayProfilesParams params;
        params.m_DisplayProfilesDDF = display_profiles_ddf;
        params.m_NameHash = dmHashString64(filename);
        dmRender::SetDisplayProfiles(profiles, params);
        dmDDF::FreeMessage(display_profiles_ddf);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDisplayProfilesCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename)
    {
        dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, profiles, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) profiles;
        }
        else
        {
            dmRender::DeleteDisplayProfiles(profiles);
        }
        return r;
    }

    dmResource::Result ResDisplayProfilesDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource)
    {
        dmRender::DeleteDisplayProfiles((dmRender::HDisplayProfiles)resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDisplayProfilesRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmRender::HDisplayProfiles profiles = (dmRender::HDisplayProfiles) resource->m_Resource;
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, profiles, filename);
        return r;
    }
}
