#include <gtest/gtest.h>

#include "../dlib/job.h"
#include "../dlib/atomic.h"
#include "../dlib/time.h"

int32_atomic_t g_counter;

void BasicEntry(dmJob::Param* params)
{
    dmTime::Sleep(10 * 1000);
    dmAtomicIncrement32(&g_counter);
}

void DummyEntry(dmJob::Param* params)
{
}

void IncrementEntry(dmJob::Param* params)
{
    dmAtomicIncrement32(&g_counter);
}

void SleepEntry(dmJob::Param* params)
{
    dmTime::Sleep(params[0].m_Int);
    dmAtomicIncrement32(&g_counter);
}

TEST(dmJob, Basic)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::Result r;

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job, dmJob::INVALID_JOB);
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

TEST(dmJob, Parent)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::Result r;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root, dmJob::INVALID_JOB);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job, root);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        jobs[i] = job;
    }

    dmJob::Run(root);
    dmJob::Wait(root);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

TEST(dmJob, Stale1)
{
    g_counter = 0;
    int job_count = 10;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::Result r;

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job, dmJob::INVALID_JOB);
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

TEST(dmJob, Stale2)
{
    g_counter = 0;
    int job_count = 10;
    dmJob::Result r;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root, dmJob::INVALID_JOB);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        r = dmJob::New(&BasicEntry, &job, root);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::AddParamFloat(job, 123.0f);
        ASSERT_EQ(dmJob::RESULT_OK, r);
        r = dmJob::Run(job);
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

TEST(dmJob, Stress)
{
    g_counter = 0;
    int job_count = 100000;
    int jobs = 0;
    int out_of_jobs = 0;
    dmJob::Result r;
    dmJob::HJob root;
    r = dmJob::New(&DummyEntry, &root, dmJob::INVALID_JOB);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    while (jobs < job_count) {
        dmJob::HJob job;
        r = dmJob::New(&SleepEntry, &job, root);
        if (r == dmJob::RESULT_OK) {
            jobs++;
            r = dmJob::AddParamInt(job, rand() % 100);
            ASSERT_EQ(dmJob::RESULT_OK, r);

            r = dmJob::Run(job);
            ASSERT_EQ(dmJob::RESULT_OK, r);
        } else {
            ASSERT_EQ(dmJob::RESULT_OUT_OF_JOBS, r);
            out_of_jobs++;
        }
    }

    ASSERT_GE(out_of_jobs, 0);

    r = dmJob::Run(root);
    ASSERT_EQ(dmJob::RESULT_OK, r);
    r = dmJob::Wait(root);
    ASSERT_EQ(dmJob::RESULT_OK, r);

    ASSERT_EQ(0, dmJob::GetActiveJobCount());
    ASSERT_EQ(job_count, g_counter);
}

int main(int argc, char **argv)
{
    dmJob::Init(4, 1024);
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
