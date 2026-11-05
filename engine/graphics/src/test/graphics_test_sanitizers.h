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

#ifndef DM_GRAPHICS_TEST_SANITIZERS_H
#define DM_GRAPHICS_TEST_SANITIZERS_H

#if defined(DM_PLATFORM_LINUX) && defined(DM_SANITIZE_ADDRESS)
#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <string.h>

extern "C" const char* __lsan_default_suppressions()
{
    // Xlib retains an input-method allocation after glfwTerminate(). Keep this
    // suppression local to GPU test executables, preserving engine leak checks.
    // https://github.com/glfw/glfw/issues/2546
    return "leak:_XimOpenIM\n";
}

static int GraphicsTestRetainDriLibrary(struct dl_phdr_info* info, size_t, void*)
{
    const char suffix[] = "_dri.so";
    const size_t length = strlen(info->dlpi_name);
    if (length < sizeof(suffix) - 1 || strcmp(info->dlpi_name + length - (sizeof(suffix) - 1), suffix) != 0)
        return 0;

    void* library = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD | RTLD_NODELETE);
    if (!library)
    {
        fprintf(stderr, "Cannot retain graphics test driver %s: %s\n", info->dlpi_name, dlerror());
        return 1;
    }
    // Balance the handle; RTLD_NODELETE keeps the module mapped after dlclose.
    if (dlclose(library) != 0)
    {
        fprintf(stderr, "Cannot close graphics test driver handle %s: %s\n", info->dlpi_name, dlerror());
        return 1;
    }
    return 0;
}
#endif

// Call after context creation, before GLFW can unload the DRI driver. Mesa 23.2.1
// retains CPU topology and executable-heap allocations in driver globals
// (get_cpu_topology and rtasm_execmem's init_heap). Keep those roots and their
// allocation stacks available to LSan without suppressing application leaks.
static inline bool GraphicsTestRetainDriLibraries()
{
#if defined(DM_PLATFORM_LINUX) && defined(DM_SANITIZE_ADDRESS)
    return dl_iterate_phdr(GraphicsTestRetainDriLibrary, 0) == 0;
#else
    return true;
#endif
}

#endif
