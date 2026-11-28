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

#include <signal.h>
#include <dxc/dxcapi.h>

#define JC_TEST_IMPLEMENTATION
#define JC_TEST_USE_DEFAULT_MAIN
#include <jc_test/jc_test.h>

// Compiling to an in-memory DXIL blob must preserve the signal handlers the JVM uses for safepoints.
TEST(Dxc, CompilePreservesSignalHandlers)
{
    const int signals[] = { SIGBUS, SIGSEGV };
    const unsigned signal_count = sizeof(signals) / sizeof(signals[0]);
    struct sigaction before[signal_count];
    for (unsigned i = 0; i < signal_count; ++i)
    {
        ASSERT_EQ(0, sigaction(signals[i], 0, &before[i]));
    }

    const char source[] = "float4 main() : SV_Target { return 1; }";
    DxcBuffer buffer = { source, sizeof(source) - 1, DXC_CP_UTF8 };
    const wchar_t* args[] = { L"-E", L"main", L"-T", L"ps_6_0" };
    IDxcCompiler3* compiler = 0;
    ASSERT_EQ(S_OK, DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
    IDxcResult* result = 0;
    HRESULT hr = compiler->Compile(&buffer, args, sizeof(args) / sizeof(args[0]), 0, IID_PPV_ARGS(&result));
    compiler->Release();
    ASSERT_EQ(S_OK, hr);
    HRESULT status;
    ASSERT_EQ(S_OK, result->GetStatus(&status));
    ASSERT_EQ(S_OK, status);
    ASSERT_TRUE(result->HasOutput(DXC_OUT_OBJECT));
    result->Release();

    for (unsigned i = 0; i < signal_count; ++i)
    {
        struct sigaction after;
        ASSERT_EQ(0, sigaction(signals[i], 0, &after));
        ASSERT_EQ(before[i].sa_flags, after.sa_flags);
        if (after.sa_flags & SA_SIGINFO)
        {
            ASSERT_TRUE(before[i].sa_sigaction == after.sa_sigaction);
        }
        else
        {
            ASSERT_TRUE(before[i].sa_handler == after.sa_handler);
        }
    }
}
