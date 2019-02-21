#include <gtest/gtest.h>

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
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
