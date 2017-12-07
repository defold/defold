#include <gtest/gtest.h>

#include "../dlib/job.h"
#include "../dlib/atomic.h"
#include "../dlib/time.h"

class JobTest : public ::testing::TestWithParam<int>
{
public:
    JobTest()
    {
        dmJob::Initialize(GetParam(), 1024);
    }

    ~JobTest()
    {
        dmJob::Finalize();
    }
};

int32_atomic_t g_counter;

void BasicEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
{
    dmTime::Sleep(10 * 1000);
    dmAtomicIncrement32(&g_counter);
}

void DummyEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
{
}

void IncrementEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
{
    dmAtomicIncrement32(&g_counter);
}

void SleepEntry(dmJob::HJob job, dmJob::Param* params, uint8_t* param_types, int param_count)
{
    dmTime::Sleep(params[0].m_Int);
    dmAtomicAdd32(&g_counter, params[0].m_Int);
    //dmAtomicIncrement32(&g_counter);
}

TEST_P(JobTest, Basic)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::Result r;

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    for (int i = 0; i < job_count; ++i) {
        r = dmJob::Wait(jobs[i]);
        ASSERT_EQ(dmJob::RESULT_OK, r);
    }

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

TEST_P(JobTest, OneRoot)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::Result r;
    dmJob::HJob *jobs = new dmJob::HJob[job_count];
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    for (int i = 0; i < job_count; ++i)
    {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job, root);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    dmJob::Run(root);
    dmJob::Wait(root);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

#if 1
TEST_P(JobTest, Parent)
{
    g_counter = 0;
    dmJob::Result r;
    dmJob::HJob j1, j2, j3;
    r = dmJob::New(&BasicEntry, &j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    r = dmJob::New(&BasicEntry, &j2);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    r = dmJob::New(&BasicEntry, &j3);
    ASSERT_EQ(dmJob::RESULT_OK, r);

#if 0
    r = dmJob::Run(j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);

    r = dmJob::Run(j2, j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);

    r = dmJob::Run(j3, j2);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);
#else

    r = dmJob::Run(j3, j2);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);

    r = dmJob::Run(j2, j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);

    r = dmJob::Run(j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    //dmTime::Sleep(100);

#endif

    r = dmJob::Wait(j1);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(3, g_counter);
}
#endif

#if 0
TEST_P(JobTest, ParentLongChain)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::Result r;
    dmJob::HJob *jobs = new dmJob::HJob[job_count];

    for (int i = 0; i < job_count; ++i)
    {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    for (int i = 0; i < job_count; ++i)
    {
        dmJob::HJob parent = dmJob::INVALID_JOB;
        if (i > 0)
        {
            parent = jobs[i - 1];
        }
        r = dmJob::Run(jobs[i], parent);
        ASSERT_EQ(dmJob::RESULT_OK, r);
    }

    dmJob::Wait(jobs[0]);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

#endif

TEST_P(JobTest, Stale1)
{
    g_counter = 0;
    int job_count = 10;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::Result r;

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    for (int i = 0; i < job_count; ++i) {
        dmJob::Result r = dmJob::Wait(jobs[i]);
        ASSERT_EQ(dmJob::RESULT_OK, r);
    }

    ASSERT_EQ(0, dmJob::GetActiveJobCount());

    for (int i = 0; i < job_count; ++i) {
        dmJob::Result r = dmJob::Wait(jobs[i]);
        ASSERT_EQ(dmJob::RESULT_STALE_HANDLE, r);
    }

    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

TEST_P(JobTest, Stale2)
{
    g_counter = 0;
    int job_count = 10;
    dmJob::Result r;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job, root);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    dmJob::Run(root);
    r = dmJob::Wait(root);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    r = dmJob::Wait(root);
    ASSERT_EQ(dmJob::RESULT_STALE_HANDLE, r);

    for (int i = 0; i < job_count; ++i) {
        dmJob::Result r = dmJob::Wait(jobs[i]);
        ASSERT_EQ(dmJob::RESULT_STALE_HANDLE, r);
    }

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

TEST_P(JobTest, Stress)
{
    // TODO: Support this (no worker thread)
    if (GetParam() == 0) {
        return;
    }
    g_counter = 0;
    int job_count = 10000;
    int jobs = 0;
    int out_of_jobs = 0;
    dmJob::Result r;
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    int counter = 0;

    while (jobs < job_count) {
        dmJob::HJob job;
        r = dmJob::New(&SleepEntry, &job);
        //printf("%d, %d\n", jobs, job_count);
        if (r == dmJob::RESULT_OK) {
            jobs++;
            int x = rand() % 100;
            counter += x;
            r = dmJob::AddParamInt(job, x);
            ASSERT_EQ(dmJob::RESULT_OK, r);

            r = dmJob::Run(job, root);
            //r = dmJob::Run(job);
         //   printf("%d\n", g_counter);
            ASSERT_EQ(dmJob::RESULT_OK, r);
        } else {
            ASSERT_EQ(dmJob::RESULT_OUT_OF_JOBS, r);
            out_of_jobs++;
        }
    }

    ASSERT_GE(out_of_jobs, 0);
    r = dmJob::Run(root);

    //dmTime::Sleep(1000 * 1000);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    r = dmJob::Wait(root);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(counter, g_counter);
}

INSTANTIATE_TEST_CASE_P(JobTests,
                        JobTest,
                        ::testing::Values(0, 1, 2, 3, 4, 8, 16));

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
