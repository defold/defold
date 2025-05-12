// Copyright 2020-2025 The Defold Foundation
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
#include <vector>
#include <map>
#include <string>
#include <string.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "dlib/dstrings.h"
#include "dlib/hash.h"
#include "dlib/time.h"
#include "dlib/mutex.h"
#include "dlib/thread.h"
#include "dlib/profile/profile.h"
#include "dlib/profile/profile_private.h"

struct TestSample
{
    char m_Name[32];
    uint64_t m_Elapsed;
    uint32_t m_Count;

    TestSample() {
        memset(this, 0, sizeof(*this));
    }

    TestSample(const TestSample& rhs) {
        memcpy(this, &rhs, sizeof(*this));
    }
};

struct TestProperty
{
    char                        m_Name[32];
    dmProfile::PropertyType     m_Type;
    dmProfile::PropertyValue    m_Value;
    int                         m_Parent;

    TestProperty() {
        memset(this, 0, sizeof(*this));
    }

    TestProperty(const TestProperty& rhs) {
        memcpy(this, &rhs, sizeof(*this));
    }
};

struct SampleCtx
{
    dmMutex::HMutex m_Mutex;
    std::vector<TestSample> samples;
};

struct PropertyCtx
{
    dmMutex::HMutex m_Mutex;
    std::vector<TestProperty> properties;
};

// *******************************************************************************
// Samples
static void ProcessSample(SampleCtx* ctx, dmProfile::HSample sample)
{
    TestSample out;

    const char* name = dmProfile::SampleGetName(sample); // Do not store this pointer!

    dmStrlCpy(out.m_Name, name, sizeof(out.m_Name));
    out.m_Name[sizeof(out.m_Name)-1] = 0;
    out.m_Elapsed = dmProfile::SampleGetTime(sample);
    out.m_Count = dmProfile::SampleGetCallCount(sample);

    ctx->samples.push_back(out);

    printf("%s %u  time: %u\n", name, out.m_Count, (uint32_t)out.m_Elapsed);
}

static void TraverseSampleTree(SampleCtx* ctx, int indent, dmProfile::HSample sample)
{
    ProcessSample(ctx, sample);

    dmProfile::SampleIterator iter;
    dmProfile::SampleIterateChildren(sample, &iter);
    while (dmProfile::SampleIterateNext(&iter))
    {
        TraverseSampleTree(ctx, indent + 1, iter.m_Sample);
    }
}

static void SampleTreeCallback(void* _ctx, const char* thread_name, dmProfile::HSample root)
{
    SampleCtx* ctx = (SampleCtx*)_ctx;
    if (strcmp(thread_name, "Remotery") == 0)
        return;

    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    printf("Thread: %s\n", thread_name);
    TraverseSampleTree(ctx, 1, root);
}

// *******************************************************************************
// Properties

static void ProcessProperty(PropertyCtx* ctx, int depth, dmProfile::HProperty property)
{
    const char* name = dmProfile::PropertyGetName(property); // Do not store this pointer!
    dmProfile::PropertyType type = dmProfile::PropertyGetType(property);
    dmProfile::PropertyValue value = dmProfile::PropertyGetValue(property);

    TestProperty prop;
    dmStrlCpy(prop.m_Name, name, sizeof(prop.m_Name));
    prop.m_Type = type;
    prop.m_Value = value;
    prop.m_Parent = depth - 1;

    dmProfile::PrintProperty(property, depth);

    ctx->properties.push_back(prop);
}

static void TraversePropertyTree(PropertyCtx* ctx, int depth, dmProfile::HProperty property)
{
    ProcessProperty(ctx, depth, property);

    dmProfile::PropertyIterator iter;
    dmProfile::PropertyIterateChildren(property, &iter);
    while (dmProfile::PropertyIterateNext(&iter))
    {
        TraversePropertyTree(ctx, depth + 1, iter.m_Property);
    }
}

static void PropertyTreeCallback(void* _ctx, dmProfile::HProperty root)
{
    PropertyCtx* ctx = (PropertyCtx*)_ctx;

    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    dmProfile::PropertyIterator iter;
    dmProfile::PropertyIterateChildren(root, &iter);
    while (dmProfile::PropertyIterateNext(&iter))
    {
        TraversePropertyTree(ctx, 0, iter.m_Property);
    }
}

TEST(dmProfile, SmallTest)
{
    dmProfile::Initialize(0);
        DM_PROFILE(0);
        DM_PROFILE_DYN(0, 0);
    dmProfile::Finalize();
}

#define TOL 0.1

TEST(dmProfile, Profile)
{
    SampleCtx ctx;
    ctx.m_Mutex = dmMutex::New();

    dmProfile::SetSampleTreeCallback(&ctx, SampleTreeCallback);
    dmProfile::Initialize(0);

    for (int i = 0; i < 2; ++i)
    {
        {
            // Due to the nature of the sample callback (once per root node)
            // we clear it here in order to collect all objects for each game frame
            DM_MUTEX_SCOPED_LOCK(ctx.m_Mutex);
            ctx.samples.clear();
        }

        {
            dmProfile::HProfile profile = dmProfile::BeginFrame();
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
            dmProfile::EndFrame(profile);
            dmTime::BusyWait(80000);
        }

        {
            DM_MUTEX_SCOPED_LOCK(ctx.m_Mutex);

            double ticks_per_sec = (double)dmProfile::GetTicksPerSecond();

            ASSERT_EQ(7U, (uint32_t)ctx.samples.size());

            int index = 0;
            ASSERT_STREQ("a", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b1", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b1_c", ctx.samples[index++].m_Name);
            ASSERT_STREQ("b2", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b2_c1", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_b2_c2", ctx.samples[index++].m_Name);
            ASSERT_STREQ("a_d", ctx.samples[index++].m_Name);

            index = 0;
            ASSERT_NEAR((100000 + 50000 + 40000 + 50000 + 40000 + 60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((50000 + 40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((50000 + 40000 + 60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((40000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((60000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
            ASSERT_NEAR((80000) / 1000000.0, ctx.samples[index++].m_Elapsed / ticks_per_sec, TOL);
        }
    }
    dmProfile::Finalize();

    dmMutex::Delete(ctx.m_Mutex);
}


DM_PROPERTY_GROUP(prop_TestGroup1, "", 0);
DM_PROPERTY_BOOL(prop_TestBOOL, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_S32(propt_TestS32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_U32(propt_TestU32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_F32(propt_TestF32, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_S64(propt_TestS64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_U64(propt_TestU64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);
DM_PROPERTY_F64(propt_TestF64, 0, PROFILE_PROPERTY_FRAME_RESET, "", &prop_TestGroup1);

DM_PROPERTY_GROUP(prop_TestGroup2, "", &prop_TestGroup1);
DM_PROPERTY_U32(prop_FrameCounter, 0, PROFILE_PROPERTY_NONE, "", &prop_TestGroup2);

static TestProperty* GetProperty(PropertyCtx* ctx, const char* name)
{
    for (uint32_t i = 0; i < ctx->properties.size(); ++i)
    {
        TestProperty* prop = &ctx->properties[i];
        if (strcmp(name, prop->m_Name) == 0)
            return prop;
    }
    return 0;
}

TEST(dmProfile, PropertyIterator)
{
    PropertyCtx ctx;
    ctx.m_Mutex = dmMutex::New();

    dmProfile::SetPropertyTreeCallback(&ctx, PropertyTreeCallback);
    dmProfile::Initialize(0);

    if (dmProfile::IsInitialized()) // false for profile null (i.e. on unsupported platforms)
    {
        for (int i = 0; i < 2; ++i)
        {
            ctx.properties.clear();
            dmProfile::HProfile profile = dmProfile::BeginFrame();

            int index = i + 1;

            DM_PROFILE(""); // Tests that the custom hash function doesn't return 0 (which Remotery doesn't like)

            DM_PROPERTY_SET_S32(propt_TestS32, index * 1);
            DM_PROPERTY_SET_U32(propt_TestU32, index * 2);
            DM_PROPERTY_SET_F32(propt_TestF32, index * 3.0f);
            DM_PROPERTY_SET_S64(propt_TestS64, index * 4);
            DM_PROPERTY_SET_U64(propt_TestU64, index * 5);
            DM_PROPERTY_SET_F64(propt_TestF64, index * 6.0f);

            DM_PROPERTY_ADD_S32(prop_FrameCounter, 1);

            dmProfile::EndFrame(profile);

            DM_MUTEX_SCOPED_LOCK(ctx.m_Mutex);

#define TEST_CHECK(NAME, TYPE, VALUE) \
    { \
        TestProperty* property = GetProperty(&ctx, NAME); \
        ASSERT_NE((TestProperty*)0, property); \
        ASSERT_EQ(dmProfile::PROPERTY_TYPE_ ## TYPE, property->m_Type); \
        ASSERT_EQ((VALUE), property->m_Value.m_ ## TYPE); \
    }


            TEST_CHECK("propt_TestS32", S32, index * 1);
            TEST_CHECK("propt_TestU32", U32, index * 2);
            TEST_CHECK("propt_TestF32", F32, index * 3.0f);
            TEST_CHECK("propt_TestS64", S64, index * 4);
            TEST_CHECK("propt_TestU64", U64, index * 5);
            TEST_CHECK("propt_TestF64", F64, index * 6.0f);

            TEST_CHECK("prop_FrameCounter", U32, i+1);

#undef TEST_CHECK
        }
    }

    dmProfile::Finalize();

    dmMutex::Delete(ctx.m_Mutex);
}

/*

TEST(dmProfile, DynamicScope)
{
    const char* FUNCTION_NAMES[] = {
        "FirstFunction",
        "SecondFunction",
        "ThirdFunction"
    };

    const char* SCOPE_NAMES[] = {
        "Scope1",
        "Scope2"
    };

    dmProfile::Initialize(128, 1024 * 1024, 16);

    dmProfile::HProfile profile = dmProfile::Begin();
    dmProfile::Release(profile);

    char names[3][128];
    dmSnPrintf(names[0], sizeof(names[0]), "%s@%s", "test.script", FUNCTION_NAMES[0]);
    dmSnPrintf(names[1], sizeof(names[1]), "%s@%s", "test.script", FUNCTION_NAMES[1]);
    dmSnPrintf(names[2], sizeof(names[2]), "%s@%s", "test.script", FUNCTION_NAMES[2]);
    uint32_t names_hash[3] = {
        dmProfile::GetNameHash(names[0], strlen(names[0])),
        dmProfile::GetNameHash(names[1], strlen(names[1])),
        dmProfile::GetNameHash(names[2], strlen(names[2]))
    };

    for (uint32_t i = 0; i < 10 ; ++i)
    {
        {
            DM_PROFILE_DYN(Scope1, names[0]);
            DM_PROFILE_DYN(Scope2, names[1]);
        }
        {
            DM_PROFILE_DYN(Scope2, names[2]);
        }
        DM_PROFILE_DYN(Scope1, names[0]);
    }

    std::vector<dmProfile::Sample> samples;
    std::map<std::string, const dmProfile::ScopeData*> scopes;

    profile = dmProfile::Begin();
    dmProfile::IterateSamples(profile, &samples, false ,&ProfileSampleCallback);
    dmProfile::IterateScopeData(profile, &scopes, false ,&ProfileScopeCallback);

    ASSERT_EQ(10U * 4U, samples.size());
    ASSERT_EQ(10U * 2, scopes[SCOPE_NAMES[0]]->m_Count);
    ASSERT_EQ(10U * 2, scopes[SCOPE_NAMES[1]]->m_Count);

    char name0[128];
    dmSnPrintf(name0, sizeof(name0), "%s@%s", "test.script", FUNCTION_NAMES[0]);

    char name1[128];
    dmSnPrintf(name1, sizeof(name1), "%s@%s", "test.script", FUNCTION_NAMES[1]);

    char name2[128];
    dmSnPrintf(name2, sizeof(name2), "%s@%s", "test.script", FUNCTION_NAMES[2]);

    for (size_t i = 0; i < samples.size(); i++)
    {
        dmProfile::Sample* sample = &samples[i];
        if (sample->m_Scope->m_NameHash == dmProfile::GetNameHash("Scope1", (uint32_t)strlen("Scope1")))
        {
            ASSERT_STREQ(sample->m_Name, name0);
        }
        else if (sample->m_Scope->m_NameHash == dmProfile::GetNameHash("Scope2", (uint32_t)strlen("Scope2")))
        {
            if (sample->m_NameHash == dmProfile::GetNameHash(name1, (uint32_t)strlen(name1)))
            {
                ASSERT_STREQ(sample->m_Name, name1);
            }
            else if (sample->m_NameHash == dmProfile::GetNameHash(name2, (uint32_t)strlen(name2)))
            {
                ASSERT_STREQ(sample->m_Name, name2);
            }
            else
            {
                ASSERT_TRUE(false);
            }
        }
        else
        {
            ASSERT_TRUE(false);
        }
    }

    dmProfile::Release(profile);

    dmProfile::Finalize();
}
*/


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
