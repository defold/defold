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

#include <dlib/time.h>
#include <unistd.h>

#include "profiler_private.h"
#include "profiler_proc_utils.h"

// Inspired by this post: http://stackoverflow.com/questions/1420426/how-to-calculate-the-cpu-usage-of-a-process-by-pid-in-linux-from-c
static uint64_t _sample_cpu_last_t = 0;
static long int _sample_cpu_last_tot = 0;
static long int _sample_cpu_last_proc = 0;
static double _sample_cpu_usage = 0.0;
static bool _sample_cpu_enabled = true;
static int _cpu_count = -1;

static long int ParseTotalCPUUsage()
{
    FILE* fp = NULL;
    if ((fp = fopen("/proc/stat", "r")) == NULL) {
        dmLogError("Could not open /proc/stat");
        // If we can't read /proc/stat we might not have permission
        // which is the case on Android 8+. For now we disable the
        // CPU usage alltogether to avoid spamming the log with errors.
        // There is a new issue for actually solving this and getting
        // the data from another source: DEF-3497
        _sample_cpu_enabled = false;
        return 0;
    }

    // Find cpu line and first jiffies entry
    long int jiffies = 0;
    if (fscanf(fp, "cpu %ld", &jiffies) != 1) {
        dmLogError("Could not parse CPU information.");
        fclose(fp);
        return 0;
    }

    // Sum rest of entries
    long int part = 0;
    while (fscanf(fp, "%ld", &part) == 1) {
        jiffies += part;
    }
    fclose(fp);
    return jiffies;
}

static long int ParseProcStat()
{
    FILE* fp = NULL;
    if ((fp = fopen("/proc/self/stat", "r")) == NULL) {
        dmLogError("Could not open /proc/self/stat");
        return 0;
    }

    long int utime = 0;
    long int stime = 0;
    if (fscanf(fp, "%*d %*s %*s %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %ld %ld", &utime, &stime) != 2) {
        dmLogError("Could not parse process CPU information.");
        fclose(fp);
        return 0;
    }
    fclose(fp);

    return utime + stime;
}

static int ParseCpuCount()
{
    FILE* fp = NULL;
    if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
            dmLogError("Could not open /proc/cpuinfo");
            return 0;
    }

    char buffer[4096];
    int result = 1; // a valid value to start with
    int cpuid = -1;
    int max_cpuid = -1;

    // go for "processor : *" lines and keep the processor id.
    while ( result != EOF)
    {
            result = fscanf(fp, "processor : %d \n", &cpuid);
            if (result == EOF)
            {
                    break;
            }
            if (result != 1) // discard the line
            {
                    result = fscanf(fp, "%4095[^\n]\n", buffer);
            } else
            {
                if (cpuid > max_cpuid)
                    max_cpuid = cpuid;
            }
    }
    fclose(fp);
    // processor ids start from 0
    max_cpuid ++;

    return max_cpuid; // in case no processor is detected, 0 is returned
}

static long int VirtualTotalCpuUsageDelta(uint64_t interval_us)
{
    // lazily initialize processor count
    if (_cpu_count == -1)
    {
        _cpu_count = ParseCpuCount();
    }

    // convert from microsecs to tick count (tick in userland lasts 1/USER_HZ sec). USER_HZ should be 100 in linux/android
    long int ticks = interval_us / 10000;

    // multiply by number of cpus for virtual number of total ticks
    return ticks * _cpu_count;
}

void dmProfilerExt::SampleProcCpuUsage(bool use_virtual_metric)
{
    if (!_sample_cpu_enabled) {
        return;
    }

    uint64_t time = dmTime::GetMonotonicTime();
    if (_sample_cpu_last_t == 0) {
        _sample_cpu_last_t = time;
        return;
    }

    uint64_t time_interval_us = time - _sample_cpu_last_t; // in micro seconds
    if (time_interval_us * 0.000001 > SAMPLE_CPU_INTERVAL) {

        long int cur_proc = ParseProcStat();

        double tot_delta;
        if (use_virtual_metric)
        {
            tot_delta = VirtualTotalCpuUsageDelta(time_interval_us);
        } else
        {
            long int cur_tot = ParseTotalCPUUsage();
            tot_delta = (double)cur_tot - (double)_sample_cpu_last_tot;
            _sample_cpu_last_tot = cur_tot;
        }

        if (tot_delta > 0) {
            _sample_cpu_usage = ((double)cur_proc - (double)_sample_cpu_last_proc) / tot_delta;
        }

        _sample_cpu_last_proc = cur_proc;
        _sample_cpu_last_t = time;
    }
}

uint64_t dmProfilerExt::GetProcMemoryUsage()
{
    long rss = 0;
    long vmu = 0;
    FILE* fp = NULL;
    if ((fp = fopen( "/proc/self/statm", "r" )) == NULL) {
        dmLogError("Could not open /proc/self/statm");
        return 0;
    }

    if (fscanf(fp, "%ld %ld", &vmu, &rss) != 2)
    {
        dmLogError("Could not parse memory information.");
        fclose(fp);
        return 0;
    }
    fclose(fp);

    long page_size = sysconf(_SC_PAGESIZE);
    return rss * page_size;
}

double dmProfilerExt::GetProcCpuUsage()
{
    return _sample_cpu_usage;
}
