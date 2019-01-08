#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>
#include <script/script.h>

#include "render/render_ddf.h"
#include "render/render.h"
#include "render/display_profiles.h"
#include "render/display_profiles_private.h"

TEST(dmDisplayProfilesTest, TestDeviceModel)
{
    dmSys::SystemInfo sys_info;
    memcpy(&sys_info.m_DeviceModel, "banana-phone", 13);

    dmRender::DisplayProfiles::Qualifier qualifier;
    qualifier.m_DeviceModels = new char*[1];
    
    qualifier.m_DeviceModels[0] =strdup("apple-phone");
    qualifier.m_NumDeviceModels = 1;
    ASSERT_EQ(false, dmRender::DeviceModelMatch(&qualifier, &sys_info));
    free(qualifier.m_DeviceModels[0]);

    qualifier.m_DeviceModels[0] = strdup("banana-phone");
    ASSERT_EQ(true, dmRender::DeviceModelMatch(&qualifier, &sys_info));
    free(qualifier.m_DeviceModels[0]);

    delete[] qualifier.m_DeviceModels;
}

TEST(dmDisplayProfilesTest, TestOptimalProfile)
{
    char** device_models = new char*[1];
    device_models[0] = strdup("a device model");
    dmRenderDDF::DisplayProfileQualifier qualifiers[] =
    {
        { 1280, 720, {0x0, 0} },
        { 720, 1280, {0x0, 0} },
        { 722, 1280, {0x0, 0} },
        { 722, 1280, {0x0, 0} },
        { 2560, 960, {0x0, 0} },
        { 1280, 722, {0x0, 0} },
        { 1280, 900, {(const char**)device_models, 1} }
    };
    dmRenderDDF::DisplayProfile profile[] =
    {
        { "profile1", { &qualifiers[0], 1 }},
        { "profile2", { &qualifiers[1], 1 }},
        { "profile3", { &qualifiers[2], 1 }},
        { "profile4", { &qualifiers[3], 1 }},
        { "profile5", { &qualifiers[4], 2 }},
        { "profile6", { &qualifiers[5], 1 }},
        { "profile7", { &qualifiers[6], 1 }}
    };
    dmRenderDDF::DisplayProfiles display_profiles =
    {
        { &profile[0], 7 }
    };

    dmhash_t q_hash[] = {
        dmHashString64(profile[0].m_Name),
        dmHashString64(profile[1].m_Name),
        dmHashString64(profile[2].m_Name),
        dmHashString64(profile[3].m_Name),
        dmHashString64(profile[4].m_Name),
        dmHashString64(profile[5].m_Name),
        dmHashString64(profile[6].m_Name)
    };

    dmRender::HDisplayProfiles profiles = dmRender::NewDisplayProfiles();
    ASSERT_NE((uintptr_t)profiles, 0);

    dmRender::DisplayProfilesParams params;
    params.m_DisplayProfilesDDF = &display_profiles;
    params.m_NameHash = dmHashString64("profiles");
    uint32_t profile_count = display_profiles.m_Profiles.m_Count;
    ASSERT_EQ(profile_count, dmRender::SetDisplayProfiles(profiles, params));
    ASSERT_EQ(params.m_NameHash, dmRender::GetDisplayProfilesName(profiles));


    for(uint32_t i = 0; i < profile_count; ++i)
    {
        dmRender::DisplayProfileDesc desc;
        dmRender::Result r = dmRender::GetDisplayProfileDesc(profiles, q_hash[i], desc);
        ASSERT_EQ(dmRender::RESULT_OK, r);
        ASSERT_EQ(qualifiers[i].m_Width, desc.m_Width);
        ASSERT_EQ(qualifiers[i].m_Height, desc.m_Height);
    }

    ASSERT_EQ(q_hash[0],  dmRender::GetOptimalDisplayProfile(profiles, 1280, 720, 0, 0));
    ASSERT_EQ(q_hash[1],  dmRender::GetOptimalDisplayProfile(profiles, 720, 1280, 0, 0));
    ASSERT_EQ(q_hash[2],  dmRender::GetOptimalDisplayProfile(profiles, 721, 1280, 0, 0));   // testing result is first found match
    ASSERT_EQ(q_hash[4],  dmRender::GetOptimalDisplayProfile(profiles, 1280, 722, 0, 0));   // testing multiple qualifiers
    // testing really large screen (i.e. if you start using a new device, but haven't yet added new display profiles)
    ASSERT_EQ(q_hash[4],  dmRender::GetOptimalDisplayProfile(profiles, 10000, 5625, 0, 0));
    ASSERT_EQ(q_hash[2],  dmRender::GetOptimalDisplayProfile(profiles, 5625, 10000, 0, 0));
    // should never pick q_hash[6] with device-model qualifier even though it has the best resolution match
    ASSERT_NE(q_hash[6],  dmRender::GetOptimalDisplayProfile(profiles, 1280, 900, 0, 0));
    ASSERT_EQ(q_hash[4],  dmRender::GetOptimalDisplayProfile(profiles, 1280, 900, 0, 0));

    dmArray<dmhash_t> choices;
    choices.SetCapacity(2);
    choices.Push(q_hash[0]);
    choices.Push(q_hash[1]);
    ASSERT_EQ(q_hash[1], dmRender::GetOptimalDisplayProfile(profiles, 721, 1280, 0, &choices)); // cherry picking

    dmRender::DeleteDisplayProfiles(profiles);
    free(device_models[0]);
    delete[] device_models;
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
