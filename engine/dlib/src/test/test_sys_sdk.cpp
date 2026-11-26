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

#include <dmsdk/dlib/sys.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// Native extensions must be able to compile and link the path APIs using only the public header.
TEST(dmSysSdk, WorkingDirectoryFunctions)
{
    char cwd[1024];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetCwd(cwd, sizeof(cwd)));
    char absolute_path[sizeof(cwd)];
    ASSERT_EQ(dmSys::RESULT_OK, dmSys::GetAbsolutePath(cwd, absolute_path, sizeof(absolute_path)));
    EXPECT_STREQ(cwd, absolute_path);
    EXPECT_EQ(dmSys::RESULT_OK, dmSys::ChDir(cwd));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
