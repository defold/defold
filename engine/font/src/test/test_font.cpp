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

#include <stdio.h>
#include <stdint.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../font.h"
#include "../util.h"

// #include <dlib/memory.h>
// #include <dlib/dstrings.h>
// #include <dlib/sys.h>
#include <dlib/log.h>
#include <dlib/testutil.h>

TEST(Simple, LoadTTF)
{
    char buffer[512];
    const char* path = dmTestUtil::MakeHostPath(buffer, sizeof(buffer), "src/test/vera_mo_bd.ttf");

    dmFont::HFont font = dmFont::LoadFontFromPath(path);
    ASSERT_NE((dmFont::HFont)0, font);

    dmFont::DestroyFont(font);
}

static int TestStandalone(const char* path, float size, const char* text)
{
    dmFont::HFont font = dmFont::LoadFontFromPath(path);
    if (!font)
    {
        dmLogError("Failed to load font '%s'", path);
        return 1;
    }

    float scale = dmFont::GetPixelScaleFromSize(font, size);
    dmFont::DebugFont(font, scale, text);

    dmFont::DestroyFont(font);
    return 0;
}

int main(int argc, char **argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    if (argc > 1 && (strstr(argv[1], ".ttf") != 0 ||
                     strstr(argv[1], ".otf") != 0))
    {
        const char* path = argv[1];
        const char* text = "abcABC123åäö!\"";
        float size = 1.0f;

        if (argc > 2)
        {
            text = argv[2];
        }

        if (argc > 3)
        {
            int nresult = sscanf(argv[3], "%f", &size);
            if (nresult != 1)
            {
                dmLogError("Failed to parse size: '%s'", argv[3]);
                return 1;
            }
        }
        int ret = TestStandalone(path, size, text);
        dmLog::LogFinalize();
        return ret;
    }

    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();

    dmLog::LogFinalize();
    return ret;
}
