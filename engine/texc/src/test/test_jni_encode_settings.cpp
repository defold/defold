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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <sanitizer/asan_interface.h>
#include <string.h>

#include "texc.h"
#include "texc_jni_private.h"

template <typename T>
static void CheckCleanup(bool copy_data, bool copy_path)
{
    uint8_t* data = copy_data ? new uint8_t[16] : 0;
    char* path = copy_path ? strdup("test-texture") : 0;

    {
        dmTexc::ScopedEncodeSettings<T> input;
        ASSERT_EQ((uint8_t*)0, input.m_Settings.m_Data);
        ASSERT_EQ((const char*)0, input.m_Settings.m_Path);
        ASSERT_EQ(0u, input.m_Settings.m_DataCount);

        input.m_Settings.m_Data = data;
        input.m_Settings.m_DataCount = copy_data ? 16 : 0;
        input.m_Settings.m_Path = path;

        if (data)
        {
            ASSERT_FALSE(__asan_address_is_poisoned(data));
            memset(data, 0xff, input.m_Settings.m_DataCount);
        }
        if (path)
        {
            ASSERT_FALSE(__asan_address_is_poisoned(path));
            ASSERT_STREQ("test-texture", input.m_Settings.m_Path);
        }
    }

    if (data)
    {
        ASSERT_TRUE(__asan_address_is_poisoned(data));
    }
    if (path)
    {
        ASSERT_TRUE(__asan_address_is_poisoned(path));
    }
}

static void CheckAllEncoders(bool copy_data, bool copy_path)
{
    CheckCleanup<dmTexc::ASTCEncodeSettings>(copy_data, copy_path);
    CheckCleanup<dmTexc::BasisUEncodeSettings>(copy_data, copy_path);
    CheckCleanup<dmTexc::DefaultEncodeSettings>(copy_data, copy_path);
}

// Verify input copies remain usable in scope and are both released when the call ends.
TEST(TexcJniEncodeSettings, ReleasesInputAndPath)
{
    CheckAllEncoders(true, true);
}

// Verify a conversion that only copied the path does not retain it on an early exit.
TEST(TexcJniEncodeSettings, ReleasesPathWithoutData)
{
    CheckAllEncoders(false, true);
}

// Verify data is released even when the Java settings have no path.
TEST(TexcJniEncodeSettings, ReleasesDataWithoutPath)
{
    CheckAllEncoders(true, false);
}

// Verify cleanup is safe when conversion exits before either allocation is made.
TEST(TexcJniEncodeSettings, EmptySettings)
{
    CheckAllEncoders(false, false);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
