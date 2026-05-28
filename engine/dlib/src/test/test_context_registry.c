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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/context_registry.h>

#if defined(_WIN32)
    #include <io.h>
    #include <windows.h>
    #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
    #endif
#else
    #include <unistd.h>
#endif

#define TEST_COLOR_RESET "\033[0m"
#define TEST_COLOR_RED   "\033[31m"
#define TEST_COLOR_GREEN "\033[32m"
#define TEST_COLOR_CYAN  "\033[36m"

static int g_TestColorOutput = 0;

#if defined(_WIN32)
static int TestEnableVirtualTerminalProcessing(void)
{
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (stdout_handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    if (!GetConsoleMode(stdout_handle, &mode))
    {
        return 0;
    }

    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
    {
        return 1;
    }

    return SetConsoleMode(stdout_handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

static int TestStdoutSupportsColor(void)
{
    const char* no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != 0)
    {
        return 0;
    }

#if defined(_WIN32)
    if (_isatty(_fileno(stdout)) == 0)
    {
        return 0;
    }

    return TestEnableVirtualTerminalProcessing();
#else
    const char* term = getenv("TERM");
    return isatty(STDOUT_FILENO) && term && strcmp(term, "dumb") != 0;
#endif
}

static void PrintTestStatus(const char* color, const char* tag, const char* name)
{
    if (g_TestColorOutput)
    {
        printf("%s%s%s %s\n", color, tag, TEST_COLOR_RESET, name);
    }
    else
    {
        printf("%s %s\n", tag, name);
    }
    fflush(stdout);
}

static void PrintTestFailed(const char* name, int result)
{
    if (g_TestColorOutput)
    {
        fprintf(stderr, "%s[  FAILED  ]%s %s: %d\n", TEST_COLOR_RED, TEST_COLOR_RESET, name, result);
    }
    else
    {
        fprintf(stderr, "[  FAILED  ] %s: %d\n", name, result);
    }
}

#define TEST_CHECK(_EXPR) do { if (!(_EXPR)) { fprintf(stderr, "TEST_CHECK failed at line %s:%d: %s\n", __FILE__, __LINE__, #_EXPR); return __LINE__; } } while (0)
#define RUN_TEST(_NAME, _CALL) do { PrintTestStatus(TEST_COLOR_CYAN, "[ RUN      ]", (_NAME)); result = (_CALL); if (result != 0) { PrintTestFailed((_NAME), result); return result; } PrintTestStatus(TEST_COLOR_GREEN, "[       OK ]", (_NAME)); } while (0)

static int TestContextRegistry(void)
{
    HContextRegistry registry = ContextRegistryCreate();

    int alpha = 1;
    int beta = 2;

    TEST_CHECK(ContextRegistrySet(registry, "alpha", &alpha) == 0);
    TEST_CHECK(ContextRegistryGetByName(registry, "alpha") == &alpha);
    TEST_CHECK(ContextRegistryGetByHash(registry, dmHashString64("alpha")) == &alpha);

    TEST_CHECK(ContextRegistrySetByHash(registry, dmHashString64("beta"), &beta) == 0);
    TEST_CHECK(ContextRegistryGetByName(registry, "beta") == &beta);
    TEST_CHECK(ContextRegistryGetByHash(registry, dmHashString64("beta")) == &beta);

    TEST_CHECK(ContextRegistrySet(registry, "alpha", 0) == 0);
    TEST_CHECK(ContextRegistryGetByName(registry, "alpha") == 0);

    TEST_CHECK(ContextRegistrySetByHash(registry, dmHashString64("beta"), 0) == 0);
    TEST_CHECK(ContextRegistryGetByHash(registry, dmHashString64("beta")) == 0);

    ContextRegistryDestroy(registry);

    return 0;
}

int main(int argc, char** argv)
{
    int result;

    (void) argc;
    (void) argv;

    g_TestColorOutput = TestStdoutSupportsColor();

    RUN_TEST("TestContextRegistry", TestContextRegistry());

    PrintTestStatus(TEST_COLOR_GREEN, "[  PASSED  ]", "test_context_registry");

    return 0;
}
