// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../font.h"
#include "../fontgen.h"

extern "C" void ResourceTypeTTF();

TEST(FontGenNull, IsUnsupportedAndCannotCreateJobs)
{
    ASSERT_FALSE(FontGenIsSupported());

    FontGenParams params = {};
    ASSERT_EQ((FontGenJobData*)0, FontGenCreateJobData(&params, 1));
    ASSERT_EQ((HJob)0, FontGenAddGlyphByIndex(0, 0, 1));
}

TEST(FontGenNull, TTFLoadingDoesNotUseTheRealParser)
{
    // Force the null archive member into the link, as the engine does through
    // its exported resource-type symbol, before FontLoadFromMemory is resolved.
    ResourceTypeTTF();

    uint8_t data[4] = {};
    ASSERT_EQ((HFont)0, FontLoadFromMemory("test.ttf", data, sizeof(data), false));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
