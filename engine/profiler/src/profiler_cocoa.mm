#import <mach/mach.h>
#include "profiler_private.h"

void dmProfilerExt::SampleCpuUsage() {
    // nop
}

uint64_t dmProfilerExt::GetMemoryUsage()
{
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(),
                                  TASK_BASIC_INFO, (task_info_t)&t_info,
                                  &t_info_count))
    {
        dmLogError("Could not get task information (for memory usage).");
    }
    return t_info.resident_size;
}

double dmProfilerExt::GetCpuUsage()
{
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(),
                                  TASK_BASIC_INFO, (task_info_t)&t_info,
                                  &t_info_count))
    {
        dmLogError("Could not get task information (for CPU usage).");
    }

    thread_array_t         thread_list;
    mach_msg_type_number_t thread_count;

    // get threads in the task
    if (KERN_SUCCESS != task_threads(mach_task_self(), &thread_list, &thread_count))
    {
        dmLogError("Could not get thread information (for CPU usage).");
        return 0.0;
    }

    thread_info_data_t     thinfo;
    mach_msg_type_number_t thread_info_count;
    thread_basic_info_t basic_info_th;

    // loop over threads and get cpu usage
    long total_cpu = 0;
    for (int j = 0; j < thread_count; j++) {
        thread_info_count = THREAD_INFO_MAX;
        if (KERN_SUCCESS != thread_info(thread_list[j], THREAD_BASIC_INFO,
                         (thread_info_t)thinfo, &thread_info_count)) {
            dmLogError("Could not get CPU usage information for thread %d.", j);
            continue;
        }
        basic_info_th = (thread_basic_info_t)thinfo;

        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            total_cpu = total_cpu + basic_info_th->cpu_usage;
        }

    }
    vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));

    return double(total_cpu) / double(TH_USAGE_SCALE);
}
