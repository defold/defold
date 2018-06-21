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

    dmResource::Result ResDisplayProfilesCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
        dmResource::Result r = AcquireResources(params.m_Factory, params.m_Buffer, params.m_BufferSize, profiles, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) profiles;
        }
        else
        {
            dmRender::DeleteDisplayProfiles(profiles);
        }
        return r;
    }

    dmResource::Result ResDisplayProfilesDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::DeleteDisplayProfiles((dmRender::HDisplayProfiles)params.m_Resource->m_Resource);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDisplayProfilesRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRender::HDisplayProfiles old_profiles = (dmRender::HDisplayProfiles)params.m_Resource->m_Resource;
        dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
        dmResource::Result r = AcquireResources(params.m_Factory, params.m_Buffer, params.m_BufferSize, profiles, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            if (old_profiles != 0x0)
                dmRender::DeleteDisplayProfiles(old_profiles);
            params.m_Resource->m_Resource = profiles;
        }
        return r;
    }
}
