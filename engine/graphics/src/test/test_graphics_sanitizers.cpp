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

#include "graphics_test_sanitizers.h"
#include <stdlib.h>

static __attribute__((noinline)) void LeakApplicationMemory()
{
    void* allocation = malloc(139);
    // Keep the allocation observable without leaving a pointer in a global root.
    asm volatile("" : : "r"(allocation) : "memory");
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;

    void* library = dlopen(argv[1], RTLD_LAZY | RTLD_LOCAL);
    if (!library)
    {
        fprintf(stderr, "Cannot load fixture: %s\n", dlerror());
        return 2;
    }
    typedef void* (*AllocateMemory)();
    AllocateMemory allocate = (AllocateMemory)dlsym(library, "GraphicsTestAllocateDriverMemory");
    if (!allocate)
        return 2;
    allocate();

    if (strcmp(argv[2], "unload") != 0)
    {
        if (!GraphicsTestRetainDriLibraries() || !GraphicsTestRetainDriLibraries())
            return 2;
    }
    if (dlclose(library) != 0)
        return 2;
    if (strcmp(argv[2], "leak") == 0)
        LeakApplicationMemory();
    return 0;
}
