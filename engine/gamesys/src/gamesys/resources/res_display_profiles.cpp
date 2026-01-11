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

    dmResource::Result ResDisplayProfilesCreate(const dmResource::ResourceCreateParams* params)
    {
        dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, profiles, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, profiles);
        }
        else
        {
            dmRender::DeleteDisplayProfiles(profiles);
        }
        return r;
    }

    dmResource::Result ResDisplayProfilesDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmRender::DeleteDisplayProfiles((dmRender::HDisplayProfiles)dmResource::GetResource(params->m_Resource));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDisplayProfilesRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRender::HDisplayProfiles old_profiles = (dmRender::HDisplayProfiles)dmResource::GetResource(params->m_Resource);
        dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, profiles, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            if (old_profiles != 0x0)
                dmRender::DeleteDisplayProfiles(old_profiles);
            dmResource::SetResource(params->m_Resource, profiles);
        }
        return r;
    }
}
