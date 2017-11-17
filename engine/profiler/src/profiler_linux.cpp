#include <dlib/time.h>
#include <unistd.h>

#include "profiler_private.h"

// Inspired by this post: http://stackoverflow.com/questions/1420426/how-to-calculate-the-cpu-usage-of-a-process-by-pid-in-linux-from-c
static uint64_t _sample_cpu_last_t = 0;
static long int _sample_cpu_last_tot = 0;
static long int _sample_cpu_last_proc = 0;
static double _sample_cpu_usage = 0.0;

static long int ParseTotalCPUUsage()
{
    FILE* fp = NULL;
    if ((fp = fopen("/proc/stat", "r")) == NULL) {
        dmLogError("Could not open /proc/stat");
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

void dmProfilerExt::SampleCpuUsage()
{
    uint64_t time = dmTime::GetTime();
    if (_sample_cpu_last_t == 0) {
        _sample_cpu_last_t = time;
        return;
    }
    if ((time - _sample_cpu_last_t) * 0.000001 > SAMPLE_CPU_INTERVAL) {
        long int cur_tot = ParseTotalCPUUsage();
        long int cur_proc = ParseProcStat();

        double tot_delta = (double)cur_tot - (double)_sample_cpu_last_tot;
        if (tot_delta > 0) {
            _sample_cpu_usage = ((double)cur_proc - (double)_sample_cpu_last_proc) / tot_delta;
        }

        _sample_cpu_last_tot = cur_tot;
        _sample_cpu_last_proc = cur_proc;
        _sample_cpu_last_t = time;
    }
}

uint64_t dmProfilerExt::GetMemoryUsage()
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

    long page_size = sysconf( _SC_PAGESIZE);
    return rss * page_size;
}

double dmProfilerExt::GetCpuUsage()
{
    return _sample_cpu_usage;
}
