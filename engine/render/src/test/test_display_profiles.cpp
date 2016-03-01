#include <stdint.h>
#include <gtest/gtest.h>

#include <dlib/hash.h>
#include <script/script.h>

#include "render/render_ddf.h"
#include "render/render.h"
#include "render/display_profiles.h"

TEST(dmDisplayProfilesTest, TestOptimalProfile)
{
    dmRenderDDF::DisplayProfileQualifier qualifiers[] =
    {
        { 1280, 720 },
        { 720, 1280 },
        { 722, 1280 },
        { 722, 1280 },
        { 2560, 960 },
        { 1280, 722 }
    };
    dmRenderDDF::DisplayProfile profile[] =
    {
        { "profile1", { &qualifiers[0], 1 }},
        { "profile2", { &qualifiers[1], 1 }},
        { "profile3", { &qualifiers[2], 1 }},
        { "profile4", { &qualifiers[3], 1 }},
        { "profile5", { &qualifiers[4], 2 }}
    };
    dmRenderDDF::DisplayProfiles display_profiles =
    {
        { &profile[0], 5 }
    };

    dmhash_t q_hash[] = {
        dmHashString64(profile[0].m_Name),
        dmHashString64(profile[1].m_Name),
        dmHashString64(profile[2].m_Name),
        dmHashString64(profile[3].m_Name),
        dmHashString64(profile[4].m_Name)
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

    dmArray<dmhash_t> choices;
    choices.SetCapacity(2);
    choices.Push(q_hash[0]);
    choices.Push(q_hash[1]);
    ASSERT_EQ(q_hash[1], dmRender::GetOptimalDisplayProfile(profiles, 721, 1280, 0, &choices)); // cherry picking

    dmRender::DeleteDisplayProfiles(profiles);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
