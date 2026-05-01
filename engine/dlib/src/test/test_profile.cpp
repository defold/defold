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

#include <stdint.h>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/dstrings.h"
#include "dlib/hash.h"
#include "dlib/mutex.h"
#include "dlib/profile/profile.h"
#include "dlib/profile/profile_private.h"
#include "dlib/thread.h"
#include "dlib/time.h"

#include "test_profiler_dummy.h"


TEST(dmProfile, SmallTest)
{
    ProfileInitialize();
        DM_PROFILE(0);
        DM_PROFILE_DYN(0, 0);
    ProfileFinalize();
}

#define TOL 0.1

TEST(dmProfile, Profile)
{
    DummyProfilerRegister();
    ProfileInitialize();

    ProfilerDummyContext* ctx = DummyProfilerGetContext();

    for (int i = 0; i < 2; ++i)
    {
        {
            HProfile profile = ProfileFrameBegin();
            {
                DM_PROFILE("a");
                dmTime::BusyWait(100000);
                {
                    {
                        DM_PROFILE("a_b1");
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE("a_b1_c")
                            dmTime::BusyWait(40000);
                        }
                    }
                    {
                        DM_PROFILE("b2");
                        dmTime::BusyWait(50000);
                        {
                            DM_PROFILE("a_b2_c1");
                            dmTime::BusyWait(40000);
                        }
                        {
                            DM_PROFILE("a_b2_c2");
                            dmTime::BusyWait(60000);
                        }
                    }
                }
            }
            {
                DM_PROFILE("a_d");
                dmTime::BusyWait(80000);
            }


            {
                //DM_MUTEX_SCOPED_LOCK(ctx.m_Mutex);

                double ticks_per_sec = 1000000.0;

                ASSERT_EQ(8U, (uint32_t)ctx->m_NumSamples);

                int index = 1; // skip the root
                ASSERT_STREQ("a", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("a_b1", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("a_b1_c", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("b2", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("a_b2_c1", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("a_b2_c2", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));
                ASSERT_STREQ("a_d", dmHashReverseSafe64(ctx->m_Samples[index++].m_NameHash));

                index = 1;
                ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((50000 + 40000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((40000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((50000 + 40000 + 60000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((40000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((60000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
                ASSERT_NEAR((80000) / 1000000.0, ctx->m_Samples[index++].m_Length / ticks_per_sec, TOL);
            }

            ProfileFrameEnd(profile);
            dmTime::BusyWait(80000);
        }

    }

    ProfileFinalize();
    DummyProfilerUnregister();
}

DM_PROPERTY_EXTERN(prop_ChildToExtern1);
DM_PROPERTY_F32(prop_GrandChildToExternF32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_ChildToExtern1);

DM_PROPERTY_GROUP(prop_TestGroup1, "", 0);
DM_PROPERTY_BOOL(prop_TestBOOL, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_S32(prop_TestS32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_U32(prop_TestU32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_F32(prop_TestF32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_S64(prop_TestS64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_U64(prop_TestU64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_F64(prop_TestF64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);

DM_PROPERTY_GROUP(prop_TestGroup2, "", &prop_TestGroup1);
DM_PROPERTY_U32(prop_FrameCounter, 0, PROFILE_PROPERTY_NONE, "", &prop_TestGroup2);


static void CheckProp(ProfilerDummyProperty* prop, const char* name,
                        ProfilePropertyType type,
                        const ProfilePropertyValue& value)
{
    if (strcmp(name, prop->m_Name)!=0)
        return;

    ASSERT_EQ(type, prop->m_Type);

    ASSERT_NE(PROFILE_PROPERTY_INVALID_IDX, prop->m_Type);

    if (strcmp("Root", prop->m_Name)!=0)
    {
        ASSERT_GT(prop->m_Idx, prop->m_ParentIdx);
    }

    if (type == PROFILE_PROPERTY_TYPE_GROUP)
    {
        return;
    }
    else if (type == PROFILE_PROPERTY_TYPE_S32)
    {
        ASSERT_EQ(value.m_S32, prop->m_Value.m_S32);
    }
    else if (type == PROFILE_PROPERTY_TYPE_U32)
    {
        ASSERT_EQ(value.m_U32, prop->m_Value.m_U32);
    }
    else if (type == PROFILE_PROPERTY_TYPE_S64)
    {
        ASSERT_EQ(value.m_S64, prop->m_Value.m_S64);
    }
    else if (type == PROFILE_PROPERTY_TYPE_U64)
    {
        ASSERT_EQ(value.m_U64, prop->m_Value.m_U64);
    }
    else if (type == PROFILE_PROPERTY_TYPE_F32)
    {
        ASSERT_EQ(value.m_F32, prop->m_Value.m_F32);
    }
    else if (type == PROFILE_PROPERTY_TYPE_F64)
    {
        ASSERT_EQ(value.m_F64, prop->m_Value.m_F64);
    }
}

TEST(dmProfile, PropertyCreationOrder)
{
    DummyProfilerRegister();
    ProfileInitialize();
    //////////////////////////////

    ProfilerDummyContext* ctx = DummyProfilerGetContext();
    ASSERT_NE((ProfilerDummyContext*)0, ctx);

    DM_PROPERTY_ADD_F32(prop_GrandChildToExternF32, 1.0f);
    DM_PROPERTY_SET_U32(prop_ChildToExtern1, 42);

    ASSERT_NE(0U, ctx->m_NumProperties);
    for (uint32_t i = 0; i < ctx->m_NumProperties; ++i)
    {
        ProfilerDummyProperty* prop = &ctx->m_Properties[i];

        ProfilePropertyValue value = {0};

        CheckProp(prop, "Root", PROFILE_PROPERTY_TYPE_GROUP, value);

        value.m_U32 = 42;
        CheckProp(prop, "prop_ChildToExtern1", PROFILE_PROPERTY_TYPE_U32, value);
    }

    /////////////////////////////
    ProfileFinalize();
    DummyProfilerUnregister();
}

TEST(dmProfile, DynamicScope)
{
    const char* SCOPE_NAMES[] = {
        "Scope1",
        "Scope2"
    };

    DummyProfilerRegister();
    ProfileInitialize();

    ProfilerDummyContext* ctx = DummyProfilerGetContext();

    HProfile profile = ProfileFrameBegin();

    for (uint32_t i = 0; i < 10; ++i)
    {
        {
            DM_PROFILE_DYN(SCOPE_NAMES[0], 0);
            DM_PROFILE_DYN(SCOPE_NAMES[1], 0);
        }
        {
            DM_PROFILE_DYN(SCOPE_NAMES[1], 0); // becomes a new sample, as it doesn't share same parent
        }
        DM_PROFILE_DYN(SCOPE_NAMES[0], 0); // same sample, as it shares same parent
    }

    ProfilerDummySample* samples = &ctx->m_Samples[0];
    ASSERT_EQ(4U, ctx->m_NumSamples); // root + 3 scopes
    ASSERT_EQ(10U * 2, samples[1].m_CallCount);
    ASSERT_EQ(10U, samples[2].m_CallCount);
    ASSERT_EQ(10U, samples[3].m_CallCount);

    // check the parents
    ASSERT_EQ(&samples[0], samples[1].m_Parent);
        ASSERT_EQ(&samples[1], samples[2].m_Parent);
    ASSERT_EQ(&samples[0], samples[3].m_Parent);

    // Check children/siblings
    ASSERT_EQ(&samples[1], samples[0].m_FirstChild);
    ASSERT_EQ(&samples[3], samples[0].m_LastChild);

    ASSERT_EQ(&samples[3], samples[1].m_Sibling);

    ASSERT_EQ(&samples[2], samples[1].m_FirstChild);
    ASSERT_EQ(&samples[2], samples[1].m_LastChild);

    ASSERT_EQ((ProfilerDummySample*)0, samples[2].m_Sibling);
    ASSERT_EQ((ProfilerDummySample*)0, samples[3].m_Sibling);


    ProfileFrameEnd(profile);

    /////////////////////////////
    ProfileFinalize();
    DummyProfilerUnregister();
}


int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
