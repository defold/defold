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

#include <stdio.h>

#include "jni/jni_util.h"

TEST(JNI, TestException)
{
    ASSERT_DEATH( dmJNI::TestSignalFromString("SIGSEGV"), "Testing exception");
}

TEST(JNI, Callstack)
{
    printf("Callstack ->\n");

    char buffer[2048];
    char* s = dmJNI::GenerateCallstack(buffer, sizeof(buffer));
    printf("%s\n", s);

    printf("<-- Callstack>\n");
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
