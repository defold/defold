#include <string.h>
#include <math.h>
#include <float.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <dlib/hashtable.h>
#include <dlib/utf8.h>
#include <graphics/graphics_util.h>

#include "display_profiles.h"
#include "display_profiles_private.h"

#include "render_private.h"
#include "render.h"

using namespace Vectormath::Aos;

namespace dmRender
{
    DisplayProfilesParams::DisplayProfilesParams()
    : m_DisplayProfilesDDF(0x0)
    , m_NameHash(0x0)
    { }

    HDisplayProfiles NewDisplayProfiles()
    {
        DisplayProfiles* profiles = new DisplayProfiles();
        profiles->m_NameHash = 0x0;
        return profiles;
    }


    void DeleteDisplayProfiles(HDisplayProfiles profiles)
    {
        uint32_t profile_count = profiles->m_Profiles.Size();
        DisplayProfiles::Qualifier* qualifier = &profiles->m_Qualifiers[0];
        for(uint32_t i = 0; i < profile_count; ++i)
        {
            DisplayProfiles::Profile& profile = profiles->m_Profiles[i];
            uint32_t qualifier_count = profile.m_QualifierCount;
            for(uint32_t q = 0; q < qualifier_count; ++q)
            {
                for (uint32_t d = 0; d < qualifier->m_NumDeviceModels; ++d)
                {
                    free(qualifier->m_DeviceModels[d]);
                }
                delete[] qualifier->m_DeviceModels;
                ++qualifier;
            }
        }
        delete profiles;
    }

    uint32_t SetDisplayProfiles(HDisplayProfiles profiles, DisplayProfilesParams& params)
    {
        profiles->m_NameHash = params.m_NameHash;
        if(params.m_DisplayProfilesDDF == 0x0)
        {
            profiles->m_Profiles.SetCapacity(0);
            profiles->m_Qualifiers.SetCapacity(0);
            return 0;
        }

        dmRenderDDF::DisplayProfiles& ddf = *params.m_DisplayProfilesDDF;
        uint32_t profile_count = 0;
        uint32_t qualifier_count = 0;
        for(uint32_t i = 0; i < ddf.m_Profiles.m_Count; ++i)
        {
            qualifier_count += ddf.m_Profiles[i].m_Qualifiers.m_Count;
            ++profile_count;
        }
        if(profile_count == 0)
            return 0;
        profiles->m_Profiles.SetCapacity(profile_count);
        profiles->m_Profiles.SetSize(profile_count);
        profiles->m_Qualifiers.SetCapacity(qualifier_count);
        profiles->m_Qualifiers.SetSize(qualifier_count);

        DisplayProfiles::Qualifier* qualifier = &profiles->m_Qualifiers[0];
        for(uint32_t i = 0; i < profile_count; ++i)
        {
            DisplayProfiles::Profile& profile = profiles->m_Profiles[i];
            profile.m_Id = dmHashString64(ddf.m_Profiles[i].m_Name);
            profile.m_QualifierCount = qualifier_count = ddf.m_Profiles[i].m_Qualifiers.m_Count;
            profile.m_Qualifiers = qualifier;
            for(uint32_t q = 0; q < qualifier_count; ++q)
            {
                dmRenderDDF::DisplayProfileQualifier& sq = ddf.m_Profiles[i].m_Qualifiers.m_Data[q];
                qualifier->m_Width = (float) sq.m_Width;
                qualifier->m_Height = (float) sq.m_Height;
                qualifier->m_Dpi = 0.0f;

                uint32_t device_models_count = sq.m_DeviceModels.m_Count;
                qualifier->m_NumDeviceModels = device_models_count;
                qualifier->m_DeviceModels = new char*[device_models_count];
                for (uint32_t d = 0; d < device_models_count; ++d)
                {
                    qualifier->m_DeviceModels[d] = strdup(sq.m_DeviceModels.m_Data[d]);
                }
                ++qualifier;
            }
        }
        return profile_count;
    }

    static int IsPortrait(uint32_t width, uint32_t height)
    {
        return width > height ? 0 : 1;
    }

    bool DeviceModelMatch(DisplayProfiles::Qualifier *qualifier, dmSys::SystemInfo *sys_info)
    {
        uint32_t device_models_count = qualifier->m_NumDeviceModels;
        size_t sys_device_model_len = strlen(sys_info->m_DeviceModel);
        
        for (uint32_t d = 0; d < device_models_count; ++d)
        {
            const char* device_model = qualifier->m_DeviceModels[d];
            size_t q_device_model_len = strlen(device_model);

            if (q_device_model_len <= sys_device_model_len)
            {
                if (strncmp(device_model, sys_info->m_DeviceModel, q_device_model_len) == 0) // match
                {
                    return true;
                }
            }
        }

        return false;
    }

    dmhash_t GetOptimalDisplayProfile(HDisplayProfiles profiles, uint32_t width, uint32_t height, uint32_t dpi, const dmArray<dmhash_t>* id_choices)
    {
        dmSys::SystemInfo sys_info;
        dmSys::GetSystemInfo(&sys_info);
        float width_f = (float) width;
        float height_f = (float) height;
        float match_area = width_f * height_f;
        float match_ratio = width_f / height_f;
        float match_dpi = (float)dpi;
        double match_distance[2] = { DBL_MAX, DBL_MAX };
        dmhash_t match_id[2] = { 0, 0 };

        for(uint32_t i = 0; i < profiles->m_Profiles.Size(); ++i)
        {
            DisplayProfiles::Profile& profile = profiles->m_Profiles[i];
            uint32_t ci;
            if(id_choices)
            {
                for(ci = 0; ci < id_choices->Size(); ++ci)
                {
                    if(profile.m_Id == (*id_choices)[ci])
                        break;
                }
                if(ci == id_choices->Size())
                    continue;
            }

            for(uint32_t q = 0; q < profile.m_QualifierCount; ++q)
            {
                DisplayProfiles::Qualifier& qualifier = profile.m_Qualifiers[q];

                bool device_model_match = (qualifier.m_NumDeviceModels == 0) ? true : DeviceModelMatch(&qualifier, &sys_info);

                if (!device_model_match)
                {
                    continue;
                }

                int category = IsPortrait(qualifier.m_Width, qualifier.m_Height);
                float area = qualifier.m_Width * qualifier.m_Height;
                float ratio = qualifier.m_Width / qualifier.m_Height;
                double distance = dmMath::Abs(1.0 - (match_area / area)) + dmMath::Abs(1.0 - (match_ratio / ratio)) + (dpi == 0 ? 0.0 : dmMath::Abs(1.0 - (qualifier.m_Dpi / match_dpi)));
                if(distance < match_distance[category])
                {
                    match_distance[category] = distance;
                    match_id[category] = profile.m_Id;
                }
            }
        }

        // If there doesn't exist one of the correct category, we must choose one from the other category
        int wantedcategory = IsPortrait(width, height);
        dmhash_t id = match_id[wantedcategory];
        if( id == 0 )
            id = match_id[ (wantedcategory+1) & 0x01 ];
        return id;
    }

    Result GetDisplayProfileDesc(HDisplayProfiles profiles, dmhash_t id, DisplayProfileDesc& desc_out)
    {
        for(uint32_t i = 0; i < profiles->m_Profiles.Size(); ++i)
        {
            if(id == profiles->m_Profiles[i].m_Id)
            {
                if(profiles->m_Profiles[i].m_QualifierCount ==0)
                    return RESULT_INVALID_PARAMETER;
                DisplayProfiles::Qualifier& q = *profiles->m_Profiles[i].m_Qualifiers;
                desc_out.m_Width = (uint32_t) q.m_Width;
                desc_out.m_Height = (uint32_t) q.m_Height;
                desc_out.m_Dpi = (uint32_t) q.m_Dpi;
                return RESULT_OK;
            }
        }
        return RESULT_INVALID_PARAMETER;
    }

    dmhash_t GetDisplayProfilesName(HDisplayProfiles profiles)
    {
        return profiles->m_NameHash;
    }

}
