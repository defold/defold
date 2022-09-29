// Copyright 2020-2022 The Defold Foundation
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

#include <dlib/time.h>

#include <Shlobj.h>
#include <Shellapi.h>
#include <io.h>
#include <direct.h>
#include <psapi.h>

#include "profiler_private.h"

static uint64_t _sample_cpu_last_t = 0;
static FILETIME _sample_cpu_prev_SysKernel;
static FILETIME _sample_cpu_prev_SysUser;
static FILETIME _sample_cpu_prev_ProcKernel;
static FILETIME _sample_cpu_prev_ProcUser;
static double _sample_cpu_usage = 0.0;

static ULONGLONG SubtractTimes(const FILETIME& ftA, const FILETIME& ftB)
{
    LARGE_INTEGER a, b;
    a.LowPart = ftA.dwLowDateTime;
    a.HighPart = ftA.dwHighDateTime;

    b.LowPart = ftB.dwLowDateTime;
    b.HighPart = ftB.dwHighDateTime;

    return a.QuadPart - b.QuadPart;
}

void dmProfilerExt::SampleCpuUsage() {
    uint64_t time = dmTime::GetTime();
    if (_sample_cpu_last_t == 0 || (time - _sample_cpu_last_t) * 0.000001 > SAMPLE_CPU_INTERVAL) {
        FILETIME ftSysIdle, ftSysKernel, ftSysUser;
        FILETIME ftProcCreation, ftProcExit, ftProcKernel, ftProcUser;

        if (!GetSystemTimes(&ftSysIdle, &ftSysKernel, &ftSysUser) ||
            !GetProcessTimes(GetCurrentProcess(), &ftProcCreation, &ftProcExit, &ftProcKernel, &ftProcUser))
        {
            dmLogError("Could not get CPU usage sample.");
            return;
        }

        if (_sample_cpu_last_t != 0)
        {
            ULONGLONG ftSysKernelDiff =
                SubtractTimes(ftSysKernel, _sample_cpu_prev_SysKernel);
            ULONGLONG ftSysUserDiff =
                SubtractTimes(ftSysUser, _sample_cpu_prev_SysUser);

            ULONGLONG ftProcKernelDiff =
                SubtractTimes(ftProcKernel, _sample_cpu_prev_ProcKernel);
            ULONGLONG ftProcUserDiff =
                SubtractTimes(ftProcUser, _sample_cpu_prev_ProcUser);

            ULONGLONG nTotalSys =  ftSysKernelDiff + ftSysUserDiff;
            ULONGLONG nTotalProc = ftProcKernelDiff + ftProcUserDiff;

            if (nTotalSys > 0)
            {
                _sample_cpu_usage = ((double)nTotalProc) / ((double)nTotalSys);
            }
        }

        _sample_cpu_prev_SysKernel = ftSysKernel;
        _sample_cpu_prev_SysUser = ftSysUser;
        _sample_cpu_prev_ProcKernel = ftProcKernel;
        _sample_cpu_prev_ProcUser = ftProcUser;

        _sample_cpu_last_t = time;
    }
}

uint64_t dmProfilerExt::GetMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        dmLogError("Could not get memory information.");
    }
    return pmc.WorkingSetSize;
}

double dmProfilerExt::GetCpuUsage()
{
    return _sample_cpu_usage;
}

void dmProfilerExt::UpdatePlatformProfiler()
{
    // nop
}
