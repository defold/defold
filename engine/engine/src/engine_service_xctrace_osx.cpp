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

#include "engine_service.h"
#include "engine_service_private.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <notify.h>
#include <os/log.h>
#include <os/signpost.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern unsigned char DEFOLD_INSTRUMENT_PACKAGE[];
extern uint32_t      DEFOLD_INSTRUMENT_PACKAGE_SIZE;
extern unsigned char DEFOLD_RESOURCE_RULES[];
extern uint32_t      DEFOLD_RESOURCE_RULES_SIZE;
extern unsigned char DEFOLD_ENERGY_SAMPLER[];
extern uint32_t      DEFOLD_ENERGY_SAMPLER_SIZE;

namespace dmEngineService
{
    static pthread_mutex_t g_TraceMutex = PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t  g_TraceCondition = PTHREAD_COND_INITIALIZER;
    static XcTraceStatus   g_TraceStatus;
    static pthread_t       g_TraceThread;
    static bool            g_TraceJoinable;
    static bool            g_TraceCancelled;
    static uint32_t        g_TraceDelayMs;
    static uint32_t        g_TraceDurationMs;
    static bool            g_TraceEnergy;
    static char            g_XcTrace[PATH_MAX];

    static bool            IsBusy()
    {
        return g_TraceStatus.m_State >= XCTRACE_SCHEDULED && g_TraceStatus.m_State <= XCTRACE_STOPPING;
    }

    static void SetState(XcTraceState state, const char* error)
    {
        pthread_mutex_lock(&g_TraceMutex);
        g_TraceStatus.m_State = state;
        dmStrlCpy(g_TraceStatus.m_Error, error ? error : "", sizeof(g_TraceStatus.m_Error));
        pthread_cond_broadcast(&g_TraceCondition);
        pthread_mutex_unlock(&g_TraceMutex);
        if (error)
        {
            dmLogError("Native trace: %s", error);
        }
        else if (state == XCTRACE_COMPLETE)
        {
            char  path[sizeof(g_TraceStatus.m_Path) * 4];
            char* out = path;
            for (const char* p = g_TraceStatus.m_Path; *p; ++p)
            {
                if (*p == '\'')
                {
                    memcpy(out, "'\\''", 4);
                    out += 4;
                }
                else
                    *out++ = *p;
            }
            *out = 0;
            dmLogInfo("Native trace complete: %s", g_TraceStatus.m_Path);
            dmLogInfo("Open in Instruments: open -a Instruments '%s'", path);
        }
    }

    static bool Cancelled()
    {
        pthread_mutex_lock(&g_TraceMutex);
        bool cancelled = g_TraceCancelled;
        pthread_mutex_unlock(&g_TraceMutex);
        return cancelled;
    }

    static void Wait(uint64_t microseconds)
    {
        struct timespec timeout = { (time_t)(microseconds / 1000000), (long)(microseconds % 1000000) * 1000 };
        pthread_mutex_lock(&g_TraceMutex);
        pthread_cond_timedwait_relative_np(&g_TraceCondition, &g_TraceMutex, &timeout);
        pthread_mutex_unlock(&g_TraceMutex);
    }

    static bool Executable(const char* path)
    {
        struct stat info;
        return path[0] == '/' && stat(path, &info) == 0 && S_ISREG(info.st_mode) && access(path, X_OK) == 0;
    }

    // Use argv directly. In particular, output paths are never interpreted by a shell.
    static int Spawn(char* const* argv, int output, pid_t* pid)
    {
        posix_spawn_file_actions_t actions;
        posix_spawnattr_t          attributes;
        posix_spawn_file_actions_init(&actions);
        posix_spawnattr_init(&attributes);
        posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETPGROUP);
        posix_spawnattr_setpgroup(&attributes, 0);
        posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, output, STDOUT_FILENO);
        posix_spawn_file_actions_adddup2(&actions, output, STDERR_FILENO);
        const char* keys[] = { "HOME", "USER", "LOGNAME", "TMPDIR", "LANG", "DEVELOPER_DIR" };
        char*       environment[8];
        uint32_t    count = 0;
        environment[count++] = strdup("PATH=/usr/bin:/bin:/usr/sbin:/sbin");
        for (uint32_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        {
            const char* value = getenv(keys[i]);
            if (value)
            {
                uint32_t size = strlen(keys[i]) + strlen(value) + 2;
                environment[count] = (char*)malloc(size);
                dmSnPrintf(environment[count++], size, "%s=%s", keys[i], value);
            }
        }
        environment[count] = 0;
        int result = posix_spawn(pid, argv[0], &actions, &attributes, argv, environment);
        for (uint32_t i = 0; i < count; ++i)
            free(environment[i]);
        posix_spawnattr_destroy(&attributes);
        posix_spawn_file_actions_destroy(&actions);
        return result;
    }

    static bool FindXcTrace()
    {
        const char* override_path = getenv("DM_PROFILE_NATIVE_XCTRACE");
        if (override_path && override_path[0])
            return realpath(override_path, g_XcTrace) && Executable(g_XcTrace);

        int descriptors[2];
        if (pipe(descriptors) != 0)
            return false;
        char* arguments[] = { (char*)"/usr/bin/xcrun", (char*)"--find", (char*)"xctrace", 0 };
        pid_t pid;
        int   result = Spawn(arguments, descriptors[1], &pid);
        close(descriptors[1]);
        if (result != 0)
        {
            close(descriptors[0]);
            return false;
        }
        fcntl(descriptors[0], F_SETFL, O_NONBLOCK);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        int      status = 0;
        uint32_t size = 0;
        bool     exited = false;
        while (!exited)
        {
            ssize_t received = read(descriptors[0], g_XcTrace + size, sizeof(g_XcTrace) - size - 1);
            if (received > 0)
                size += received;
            exited = waitpid(pid, &status, WNOHANG) == pid;
            if (!exited && (Cancelled() || dmTime::GetMonotonicTime() >= deadline))
            {
                kill(pid, SIGKILL);
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
                {
                }
                status = -1;
                break;
            }
            if (!exited)
                Wait(10000);
        }
        // Drain the final output after the child exits.
        ssize_t received = read(descriptors[0], g_XcTrace + size, sizeof(g_XcTrace) - size - 1);
        if (received > 0)
            size += received;
        close(descriptors[0]);
        while (size && (g_XcTrace[size - 1] == '\n' || g_XcTrace[size - 1] == '\r'))
            --size;
        g_XcTrace[size] = 0;
        return status == 0 && Executable(g_XcTrace);
    }

    static bool MakeDirectories(char* path)
    {
        for (char* p = path + 1;; ++p)
        {
            if (*p != '/' && *p != 0)
                continue;
            char separator = *p;
            *p = 0;
            if (mkdir(path, 0755) != 0 && errno != EEXIST)
            {
                *p = separator;
                return false;
            }
            *p = separator;
            if (!separator)
                return true;
        }
    }

    static bool ResolveOutput(const char* requested, char* output, uint32_t capacity)
    {
        char path[1024];
        if (requested && requested[0])
        {
            if (requested[0] != '/' || strlen(requested) >= sizeof(path))
                return false;
            dmStrlCpy(path, requested, sizeof(path));
        }
        else
        {
            char        identifier[256] = "com.defold.dmengine";
            CFBundleRef bundle = CFBundleGetMainBundle();
            CFStringRef bundle_id = bundle ? CFBundleGetIdentifier(bundle) : 0;
            if (bundle_id)
                CFStringGetCString(bundle_id, identifier, sizeof(identifier), kCFStringEncodingUTF8);
            const char* home = getenv("HOME");
            if (!home || dmSnPrintf(path, sizeof(path), "%s/Library/Logs/%s/Profiling", home, identifier) >= (int)sizeof(path))
                return false;
        }
        for (const char* p = path; *p; ++p)
            if ((unsigned char)*p < 32)
                return false;
        uint32_t length = strlen(path);
        bool     trace_path = length >= 6 && strcmp(path + length - 6, ".trace") == 0;
        if (trace_path)
        {
            if (access(path, F_OK) == 0)
                return false;
            dmStrlCpy(output, path, capacity);
            char* slash = strrchr(path, '/');
            if (slash == path)
                slash[1] = 0;
            else
                *slash = 0;
        }
        else if (dmSnPrintf(output, capacity, "%s/dmengine-%d-%llu.trace", path, getpid(), (unsigned long long)dmTime::GetTime()) >= (int)capacity)
            return false;
        return MakeDirectories(path);
    }

    static bool WriteFile(const char* path, const void* data, uint32_t size)
    {
        FILE* file = fopen(path, "wb");
        if (!file)
            return false;
        bool success = fwrite(data, 1, size, file) == size;
        return fclose(file) == 0 && success;
    }

    struct EnergySample
    {
        uint64_t m_Window;
        double   m_Power[3];
        double   m_Impact;
        uint32_t m_Components;
        bool     m_HasImpact;
    };

    struct EnergyCapture
    {
        char             m_Directory[128];
        int              m_Control;
        int              m_Samples;
        pid_t            m_Process;
        CFMutableDataRef m_Buffer;
        os_log_t         m_Log;
        uint32_t         m_RecordedSamples;
        bool             m_Ready;
        const char*      m_Error;
    };

    static bool EnergyValue(CFTypeRef dictionary, CFStringRef key, double* value)
    {
        if (!dictionary || CFGetTypeID(dictionary) != CFDictionaryGetTypeID())
            return false;
        CFTypeRef number = CFDictionaryGetValue((CFDictionaryRef)dictionary, key);
        return number && CFGetTypeID(number) == CFNumberGetTypeID() &&
        CFNumberGetValue((CFNumberRef)number, kCFNumberDoubleType, value) && isfinite(*value) && *value >= 0;
    }

    static bool ParseEnergySample(const uint8_t* bytes, uint32_t size, pid_t pid, EnergySample* sample)
    {
        memset(sample, 0, sizeof(*sample));
        CFDataRef         data = CFDataCreateWithBytesNoCopy(0, bytes, size, kCFAllocatorNull);
        CFPropertyListRef root = CFPropertyListCreateWithData(0, data, kCFPropertyListImmutable, 0, 0);
        CFRelease(data);
        if (!root)
            return false;
        double window;
        bool   valid = EnergyValue(root, CFSTR("elapsed_ns"), &window) && window > 0 && window < (double)UINT64_MAX;
        if (valid)
        {
            sample->m_Window = (uint64_t)window;
            CFTypeRef         processor = CFDictionaryGetValue((CFDictionaryRef)root, CFSTR("processor"));
            const CFStringRef keys[] = { CFSTR("cpu_power"), CFSTR("gpu_power"), CFSTR("ane_power") };
            for (uint32_t i = 0; i < 3; ++i)
            {
                if (EnergyValue(processor, keys[i], &sample->m_Power[i]))
                {
                    sample->m_Power[i] /= 1000.0; // powermetrics reports milliwatts.
                    sample->m_Components |= 1 << i;
                }
            }
            CFTypeRef tasks = CFDictionaryGetValue((CFDictionaryRef)root, CFSTR("tasks"));
            if (tasks && CFGetTypeID(tasks) == CFArrayGetTypeID())
            {
                for (CFIndex i = 0; i < CFArrayGetCount((CFArrayRef)tasks); ++i)
                {
                    CFTypeRef task = CFArrayGetValueAtIndex((CFArrayRef)tasks, i);
                    double    task_pid;
                    if (EnergyValue(task, CFSTR("pid"), &task_pid) && task_pid == pid)
                    {
                        sample->m_HasImpact = EnergyValue(task, CFSTR("energy_impact_per_s"), &sample->m_Impact);
                        break;
                    }
                }
            }
            valid = sample->m_Components != 0 && sample->m_HasImpact;
        }
        CFRelease(root);
        return valid;
    }

    static bool ReadEnergy(EnergyCapture* energy, bool publish)
    {
        int   status;
        pid_t exited = waitpid(energy->m_Process, &status, WNOHANG);
        if (exited == energy->m_Process || (exited < 0 && errno != EINTR))
        {
            energy->m_Process = 0;
            energy->m_Error = "Energy capture failed. Authorize powermetrics in the macOS dialog or disable energy measurements; see the .xctrace.log.";
            return false;
        }
        uint8_t buffer[65536];
        ssize_t received;
        while ((received = read(energy->m_Samples, buffer, sizeof(buffer))) > 0)
        {
            CFDataAppendBytes(energy->m_Buffer, buffer, received);
            if (CFDataGetLength(energy->m_Buffer) > 16 * 1024 * 1024)
                break;
            const uint8_t* begin = CFDataGetBytePtr(energy->m_Buffer);
            const uint8_t* cursor = begin;
            const uint8_t* end = begin + CFDataGetLength(energy->m_Buffer);
            const uint8_t* separator;
            while ((separator = (const uint8_t*)memchr(cursor, 0, end - cursor)) != 0)
            {
                if (separator != cursor)
                {
                    EnergySample sample;
                    if (!ParseEnergySample(cursor, separator - cursor, getpid(), &sample))
                    {
                        energy->m_Error = "powermetrics did not provide valid CPU/GPU/ANE power and application energy impact samples on this Mac.";
                        return false;
                    }
                    energy->m_Ready = true;
                    if (publish)
                    {
                        const char* components[] = { "CPU", "GPU", "ANE" };
                        for (uint32_t i = 0; i < 3; ++i)
                            if (sample.m_Components & (1 << i))
                                os_signpost_event_emit(energy->m_Log, OS_SIGNPOST_ID_EXCLUSIVE, "System Power", "component: %{public}s, watts: %f, window: %llu ns", components[i], sample.m_Power[i], (unsigned long long)sample.m_Window);
                        os_signpost_event_emit(energy->m_Log, OS_SIGNPOST_ID_EXCLUSIVE, "Application Energy", "pid: %u, impact per second: %f, window: %llu ns", (unsigned int)getpid(), sample.m_Impact, (unsigned long long)sample.m_Window);
                        ++energy->m_RecordedSamples;
                    }
                }
                cursor = separator + 1;
            }
            CFDataDeleteBytes(energy->m_Buffer, CFRangeMake(0, cursor - begin));
        }
        if (CFDataGetLength(energy->m_Buffer) > 16 * 1024 * 1024 ||
            (received == 0 && energy->m_Ready) || (received < 0 && errno != EAGAIN && errno != EINTR))
        {
            energy->m_Error = "Energy sample stream failed; see the .xctrace.log beside the output.";
            return false;
        }
        return true;
    }

    static bool StartEnergy(EnergyCapture* energy, int log)
    {
        energy->m_Error = "Cannot start the energy sampler; see the .xctrace.log beside the output.";
        dmStrlCpy(energy->m_Directory, "/tmp/defold-energy-XXXXXX", sizeof(energy->m_Directory));
        if (!mkdtemp(energy->m_Directory))
            return false;
        char control[160], samples[160], supervisor[160];
        dmSnPrintf(control, sizeof(control), "%s/control", energy->m_Directory);
        dmSnPrintf(samples, sizeof(samples), "%s/samples", energy->m_Directory);
        dmSnPrintf(supervisor, sizeof(supervisor), "%s/sampler.py", energy->m_Directory);
        if (mkfifo(control, 0600) != 0 || mkfifo(samples, 0600) != 0 || !WriteFile(supervisor, DEFOLD_ENERGY_SAMPLER, DEFOLD_ENERGY_SAMPLER_SIZE))
            return false;
        // Only the engine owns the control writer. EOF also stops the privileged
        // supervisor after a crash; it owns and reaps powermetrics itself.
        energy->m_Control = open(control, O_RDWR | O_CLOEXEC);
        energy->m_Samples = open(samples, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (energy->m_Control < 0 || energy->m_Samples < 0)
            return false;
        energy->m_Buffer = CFDataCreateMutable(0, 0);
        energy->m_Log = os_log_create("com.defold.energy", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
        // These paths contain only the fixed prefix and mkdtemp's suffix. No
        // request data or trace output path is passed to the privileged command.
        char script[1024];
        dmSnPrintf(script, sizeof(script),
                   "with timeout of 2147483647 seconds\n"
                   "do shell script \"/usr/bin/python3 -I -S '%s' 0 --control '%s' --output '%s'\" with administrator privileges\n"
                   "end timeout",
                   supervisor,
                   control,
                   samples);
        char* arguments[] = { (char*)"/usr/bin/osascript", (char*)"-e", script, 0 };
        if (Spawn(arguments, log, &energy->m_Process) != 0)
            return false;
        uint64_t deadline = dmTime::GetMonotonicTime() + 120000000;
        while (!energy->m_Ready && !Cancelled() && dmTime::GetMonotonicTime() < deadline)
        {
            if (!ReadEnergy(energy, false))
                return false;
            if (!energy->m_Ready)
                Wait(10000);
        }
        if (!energy->m_Ready)
            return false;
        energy->m_Error = 0;
        return true;
    }

    static void StopEnergy(EnergyCapture* energy)
    {
        if (energy->m_Control >= 0)
            close(energy->m_Control);
        energy->m_Control = -1;
        const char* files[] = { "control", "samples", "sampler.py" };
        for (uint32_t i = 0; i < 3 && energy->m_Directory[0]; ++i)
        {
            char path[160];
            dmSnPrintf(path, sizeof(path), "%s/%s", energy->m_Directory, files[i]);
            unlink(path);
        }
        if (energy->m_Directory[0])
            rmdir(energy->m_Directory);
        if (energy->m_Process > 0)
        {
            int      status;
            uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
            for (;;)
            {
                pid_t exited = waitpid(energy->m_Process, &status, WNOHANG);
                if (exited == energy->m_Process || (exited < 0 && errno != EINTR))
                    break;
                if (dmTime::GetMonotonicTime() >= deadline)
                {
                    kill(energy->m_Process, SIGKILL);
                    while (waitpid(energy->m_Process, &status, 0) < 0 && errno == EINTR)
                    {
                    }
                    break;
                }
                Wait(10000);
            }
            energy->m_Process = 0;
        }
        if (energy->m_Samples >= 0)
            close(energy->m_Samples);
        energy->m_Samples = -1;
        if (energy->m_Buffer)
            CFRelease(energy->m_Buffer);
        if (energy->m_Log)
            os_release(energy->m_Log);
        energy->m_Buffer = 0;
        energy->m_Log = 0;
    }

    // Own and reap each child before publishing a terminal state. This keeps busy
    // true while xctrace is finalizing and prevents signals reaching a reused PID.
    static bool RunTool(char* const* arguments, int log, int notification, bool recording, bool* started, EnergyCapture* energy)
    {
        pid_t pid;
        int   result = Spawn(arguments, log, &pid);
        if (result != 0)
            return false;
        bool     ready = !recording;
        bool     stopping = false;
        bool     failed = false;
        uint64_t deadline = dmTime::GetMonotonicTime() + 30000000;
        for (;;)
        {
            int   status;
            pid_t exited = waitpid(pid, &status, WNOHANG);
            if (exited == pid)
                return !failed && ready && WIFEXITED(status) && WEXITSTATUS(status) == 0;
            if (exited < 0 && errno != EINTR)
                return false;
            uint64_t now = dmTime::GetMonotonicTime();
            if (recording && !ready && !stopping)
            {
                int notified = 0;
                if (notify_check(notification, &notified) == NOTIFY_STATUS_OK && notified)
                {
                    ready = true;
                    *started = true;
                    SetState(XCTRACE_RECORDING, 0);
                    deadline = g_TraceDurationMs ? now + (uint64_t)g_TraceDurationMs * 1000 : UINT64_MAX;
                }
            }
            if (energy && !stopping && !ReadEnergy(energy, ready))
                failed = true;
            if (!stopping && (failed || Cancelled() || now >= deadline))
            {
                failed = failed || !ready;
                stopping = true;
                if (recording)
                    SetState(XCTRACE_STOPPING, 0);
                // Stop recording before sampler cleanup can evict samples from a windowed trace.
                kill(pid, recording ? SIGINT : SIGTERM);
                if (energy)
                    StopEnergy(energy);
                deadline = now + 45000000;
            }
            else if (stopping && now >= deadline)
            {
                kill(pid, SIGKILL);
                while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
                {
                }
                return false;
            }
            Wait(10000);
        }
    }

    static void* Record(void*)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + (uint64_t)g_TraceDelayMs * 1000;
        while (!Cancelled() && dmTime::GetMonotonicTime() < deadline)
            Wait(10000);
        if (Cancelled())
        {
            SetState(XCTRACE_CANCELLED, 0);
            return 0;
        }
        SetState(XCTRACE_STARTING, 0);
        if (!FindXcTrace())
        {
            SetState(Cancelled() ? XCTRACE_CANCELLED : XCTRACE_ERROR,
                     Cancelled() ? 0 : "xctrace was not found. Set DM_PROFILE_NATIVE_XCTRACE or select a full Xcode installation.");
            return 0;
        }
        char log_path[1100];
        dmSnPrintf(log_path, sizeof(log_path), "%s.xctrace.log", g_TraceStatus.m_Path);
        int log = open(log_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (log < 0)
        {
            SetState(XCTRACE_ERROR, "Cannot create recorder log; the output may already exist.");
            return 0;
        }
        char sources[] = "/tmp/defold-instruments-XXXXXX";
        char package_source[256], rules[256], package[1100], builder[PATH_MAX];
        bool success = mkdtemp(sources) != 0;
        dmSnPrintf(package_source, sizeof(package_source), "%s/DefoldResources.instrpkg", sources);
        dmSnPrintf(rules, sizeof(rules), "%s/ResourceLoads.clp", sources);
        dmSnPrintf(package, sizeof(package), "%s.instrdst", g_TraceStatus.m_Path);
        dmStrlCpy(builder, g_XcTrace, sizeof(builder));
        char* filename = strrchr(builder, '/') + 1;
        dmStrlCpy(filename, "instrumentbuilder", sizeof(builder) - (filename - builder));
        success = success && WriteFile(package_source, DEFOLD_INSTRUMENT_PACKAGE, DEFOLD_INSTRUMENT_PACKAGE_SIZE) && WriteFile(rules, DEFOLD_RESOURCE_RULES, DEFOLD_RESOURCE_RULES_SIZE);
        char* compile[] = { builder, package_source, (char*)"-o", package, 0 };
        success = success && !Cancelled() && RunTool(compile, log, -1, false, 0, 0);
        unlink(package_source);
        unlink(rules);
        rmdir(sources);
        EnergyCapture energy;
        memset(&energy, 0, sizeof(energy));
        energy.m_Control = energy.m_Samples = -1;
        if (success && g_TraceEnergy && !Cancelled())
            success = StartEnergy(&energy, log);
        int  notification = -1;
        bool started = false;
        if (success && !Cancelled())
        {
            char name[128], pid[32];
            dmSnPrintf(name, sizeof(name), "com.defold.trace.%d.%llu", getpid(), (unsigned long long)dmTime::GetTime());
            dmSnPrintf(pid, sizeof(pid), "%d", getpid());
            success = notify_register_check(name, &notification) == NOTIFY_STATUS_OK;
            if (success)
            {
                int ignored;
                notify_check(notification, &ignored); // Consume the registration's initial notification.
                char* arguments[] = { g_XcTrace, (char*)"record", (char*)"--template", (char*)"Game Performance", (char*)"--instrument", (char*)"os_signpost", (char*)"--package", package, (char*)"--instrument", (char*)"Defold Resource Loading", (char*)"--attach", pid, (char*)"--output", g_TraceStatus.m_Path, (char*)"--notify-tracing-started", name, (char*)"--instrument", (char*)"Defold Energy", 0 };
                if (!g_TraceEnergy)
                    arguments[16] = 0;
                success = RunTool(arguments, log, notification, true, &started, g_TraceEnergy ? &energy : 0);
                notify_cancel(notification);
            }
        }
        StopEnergy(&energy);
        if (success && g_TraceEnergy && !energy.m_RecordedSamples)
        {
            energy.m_Error = "No energy samples were recorded. Use a longer capture or disable energy measurements.";
            success = false;
        }
        close(log);
        struct stat info;
        success = success && stat(g_TraceStatus.m_Path, &info) == 0 && S_ISDIR(info.st_mode);
        bool cancelled = !started && Cancelled();
        SetState(success ? XCTRACE_COMPLETE : cancelled ? XCTRACE_CANCELLED :
                                                          XCTRACE_ERROR,
                 success || cancelled ? 0 : energy.m_Error ? energy.m_Error :
                                                             "Recording failed; see the .xctrace.log beside the output.");
        return 0;
    }

    XcTraceResult StartXcTrace(const XcTraceParams& params)
    {
        pthread_mutex_lock(&g_TraceMutex);
        bool busy = IsBusy();
        pthread_mutex_unlock(&g_TraceMutex);
        if (busy)
            return XCTRACE_BUSY;
        if (g_TraceJoinable)
        {
            pthread_join(g_TraceThread, 0);
            g_TraceJoinable = false;
        }
        char path[1024];
        if (!ResolveOutput(params.m_Path, path, sizeof(path)))
            return XCTRACE_INVALID;
        if (!EnableXcTraceProfiler())
            return XCTRACE_UNAVAILABLE;
        pthread_mutex_lock(&g_TraceMutex);
        dmStrlCpy(g_TraceStatus.m_Path, path, sizeof(g_TraceStatus.m_Path));
        g_TraceStatus.m_Error[0] = 0;
        g_TraceStatus.m_State = XCTRACE_SCHEDULED;
        g_TraceDelayMs = params.m_DelayMs;
        g_TraceDurationMs = params.m_DurationMs;
        g_TraceEnergy = params.m_Energy;
        g_TraceCancelled = false;
        pthread_mutex_unlock(&g_TraceMutex);
        int result = pthread_create(&g_TraceThread, 0, Record, 0);
        if (result != 0)
        {
            SetState(XCTRACE_ERROR, strerror(result));
            return XCTRACE_UNAVAILABLE;
        }
        g_TraceJoinable = true;
        dmLogInfo("Native trace output: %s", path);
        return XCTRACE_OK;
    }

    void StopXcTrace()
    {
        pthread_mutex_lock(&g_TraceMutex);
        g_TraceCancelled = true;
        pthread_cond_broadcast(&g_TraceCondition);
        pthread_mutex_unlock(&g_TraceMutex);
    }

    void GetXcTraceStatus(XcTraceStatus* status)
    {
        pthread_mutex_lock(&g_TraceMutex);
        *status = g_TraceStatus;
        pthread_mutex_unlock(&g_TraceMutex);
    }

    static bool ReadTime(const char* name, uint32_t* value)
    {
        const char* text = getenv(name);
        return !text || ParseTraceTime(text, value);
    }

    void Initialize()
    {
        InitializeXcTraceProfiler();
        const char* enabled = getenv("DM_PROFILE_NATIVE_TRACE");
        if (!enabled || strcmp(enabled, "1") != 0)
            return;
        XcTraceParams params;
        params.m_Path = getenv("DM_PROFILE_NATIVE_TRACE_PATH");
        const char* energy = getenv("DM_PROFILE_NATIVE_TRACE_ENERGY");
        if (energy && strcmp(energy, "0") != 0 && strcmp(energy, "1") != 0)
        {
            SetState(XCTRACE_ERROR, "Trace energy must be 0 or 1.");
            return;
        }
        params.m_Energy = !energy || strcmp(energy, "1") == 0;
        if (!ReadTime("DM_PROFILE_NATIVE_TRACE_DELAY_MS", &params.m_DelayMs) ||
            !ReadTime("DM_PROFILE_NATIVE_TRACE_DURATION_MS", &params.m_DurationMs))
        {
            SetState(XCTRACE_ERROR, "Trace delay and duration must be nonnegative integer milliseconds.");
            return;
        }
        XcTraceResult result = StartXcTrace(params);
        if (result != XCTRACE_OK)
        {
            SetState(XCTRACE_ERROR, "Cannot start native trace: invalid output path or unavailable profiler.");
            return;
        }
        if (!params.m_DelayMs)
        {
            pthread_mutex_lock(&g_TraceMutex);
            while (g_TraceStatus.m_State == XCTRACE_SCHEDULED || g_TraceStatus.m_State == XCTRACE_STARTING)
                pthread_cond_wait(&g_TraceCondition, &g_TraceMutex);
            pthread_mutex_unlock(&g_TraceMutex);
        }
    }

    void Finalize()
    {
        StopXcTrace();
        if (g_TraceJoinable)
        {
            pthread_join(g_TraceThread, 0);
            g_TraceJoinable = false;
        }
        SetState(XCTRACE_IDLE, 0);
    }
} // namespace dmEngineService
