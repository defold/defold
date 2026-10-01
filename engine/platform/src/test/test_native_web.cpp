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

#include "../native/native.h"

// Native web lookup must reach the linked Emscripten resolver after the GLFW API rename.
TEST(NativeWeb, GetProcAddress)
{
    ASSERT_NE((void*)0, dmNativeGetProcAddress("glClear"));
}

// Extension suffixes must keep resolving to the same functions as before the GLFW API rename.
TEST(NativeWeb, GetProcAddressExtensionAlias)
{
    void* address = dmNativeGetProcAddress("glActiveTexture");
    ASSERT_NE((void*)0, address);
    ASSERT_EQ(address, dmNativeGetProcAddress("glActiveTextureARB"));
}

// Unsupported function names must return null so callers can detect unavailable functionality.
TEST(NativeWeb, GetProcAddressUnknownFunction)
{
    ASSERT_EQ((void*)0, dmNativeGetProcAddress("glNonexistentFunction"));
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
