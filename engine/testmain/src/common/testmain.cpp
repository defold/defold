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

#if !defined(DM_PLATFORM_VENDOR)

#include "testmain.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(DM_PLATFORM_IOS)
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>

static bool TestMainPathExists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static bool TestMainReadyPathsExist(const char* paths)
{
    size_t paths_len = strlen(paths);
    char* paths_copy = (char*) malloc(paths_len + 1);
    if (!paths_copy)
        return true;

    memcpy(paths_copy, paths, paths_len + 1);

    bool exists = true;
    char* save = 0;
    char* path = strtok_r(paths_copy, ";", &save);
    while (path)
    {
        if (path[0] != '\0' && !TestMainPathExists(path))
        {
            exists = false;
            break;
        }
        path = strtok_r(0, ";", &save);
    }

    free(paths_copy);
    return exists;
}

static void TestMainWaitForReadyPaths()
{
    const char* ready_paths = getenv("DEFOLD_TEST_READY_PATHS");
    if (!ready_paths || ready_paths[0] == '\0')
        return;

    for (uint32_t i = 0; i < 40; ++i)
    {
        if (TestMainReadyPathsExist(ready_paths))
            return;
        usleep(50000);
    }

    fprintf(stderr, "Timed out waiting for DEFOLD_TEST_READY_PATHS=%s\n", ready_paths);
}

static bool TestMainResolveExecutablePath(const char* suffix, char* path, size_t path_size)
{
    uint32_t size = (uint32_t) path_size;
    if (_NSGetExecutablePath(path, &size) != 0)
        return false;

    char* slash = strrchr(path, '/');
    if (!slash)
        return false;
    *slash = '\0';

    size_t path_len = strlen(path);
    if (snprintf(path + path_len, path_size - path_len, "/%s", suffix) >= (int) (path_size - path_len))
        return false;
    return true;
}

__attribute__((constructor))
static void TestMainSetWorkingDirectory()
{
    const char* workdir = getenv("DEFOLD_TEST_WORKDIR");
    if (!workdir || workdir[0] == '\0')
        return;

    char path[PATH_MAX];
    const char* executable_path_prefix = "@executable_path/";
    size_t executable_path_prefix_len = strlen(executable_path_prefix);
    if (strncmp(workdir, executable_path_prefix, executable_path_prefix_len) == 0)
    {
        if (!TestMainResolveExecutablePath(workdir + executable_path_prefix_len, path, sizeof(path)))
            return;
    }
    else if (workdir[0] == '/')
    {
        if (snprintf(path, sizeof(path), "%s", workdir) >= (int) sizeof(path))
            return;
    }
    else
    {
        const char* home = getenv("HOME");
        if (!home || home[0] == '\0')
            return;
        if (snprintf(path, sizeof(path), "%s/%s", home, workdir) >= (int) sizeof(path))
            return;
    }

    if (chdir(path) != 0)
    {
        fprintf(stderr, "Unable to chdir to DEFOLD_TEST_WORKDIR=%s\n", path);
        return;
    }

    TestMainWaitForReadyPaths();
}
#endif

extern "C" bool TestMainPlatformInit()
{
    return true;
}

extern "C" int TestMainIsDebuggerAttached()
{
#if defined(__APPLE__)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info = {};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, 0, 0) != 0)
        return 0;
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    return 0;
#endif
}

#endif
