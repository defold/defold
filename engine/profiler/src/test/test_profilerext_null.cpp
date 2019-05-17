#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../profiler_private.h"
#include "../profiler.h"

TEST(ProfilerExt, ProfilerStubs)
{
    dmProfiler::SetUpdateFrequency(30);
    dmProfiler::ToggleProfiler();
    dmProfiler::RenderProfiler(0, 0, 0, 0);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
