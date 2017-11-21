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

TEST(dmJob, Basic)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        dmJob::New(&BasicEntry, &job, 0);
        dmJob::AddParam(job, 123.0f);
        dmJob::Run(job);
        jobs[i] = job;
    }

    for (int i = 0; i < job_count; ++i) {
        dmJob::Wait(jobs[i]);
    }

    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

TEST(dmJob, Parent)
{
    g_counter = 0;
    int job_count = 100;
    dmJob::HJob* jobs = new dmJob::HJob[job_count];
    dmJob::HJob root;
    dmJob::New(&DummyEntry, &root, 0);

    for (int i = 0; i < job_count; ++i) {
        dmJob::HJob job;
        dmJob::New(&BasicEntry, &job, root);
        dmJob::AddParam(job, 123.0f);
        dmJob::Run(job);
        jobs[i] = job;
    }

    dmJob::Run(root);
    dmJob::Wait(root);

    /*for (int i = 0; i < job_count; ++i) {
        dmJob::Wait(jobs[i]);
    }*/

    ASSERT_EQ(job_count, g_counter);

    delete[] jobs;
}

int main(int argc, char **argv)
{
    dmJob::Init(4);
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
