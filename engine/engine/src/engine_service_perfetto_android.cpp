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

#include "engine_service_perfetto.h"

#include <android/api-level.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dlib/dstrings.h>
#include <dmsdk/dlib/android.h>

namespace dmEngineService
{
    static const char TRIGGER_HELPER[] = "/system/bin/trigger_perfetto";

    bool SystemTraceAppInfo(char* package, uint32_t package_size)
    {
        if (!package[0])
        {
            if (!dmAndroid::GetAndroidApp())
            {
                return false;
            }

            dmAndroid::ThreadAttacher thread;
            JNIEnv* env = thread.GetEnv();
            if (!env)
            {
                return false;
            }

            jclass activity_class = env->GetObjectClass(thread.GetActivity()->clazz);
            jmethodID get_package = env->GetMethodID(activity_class, "getPackageName", "()Ljava/lang/String;");
            jstring name = (jstring)env->CallObjectMethod(thread.GetActivity()->clazz, get_package);
            if (name)
            {
                const char* text = env->GetStringUTFChars(name, 0);
                dmStrlCpy(package, text, package_size);
                env->ReleaseStringUTFChars(name, text);
                env->DeleteLocalRef(name);
            }

            env->DeleteLocalRef(activity_class);
        }

        return package[0] && access(TRIGGER_HELPER, X_OK) == 0;
    }

    int SystemTraceGetAndroidApiLevel()
    {
        return android_get_device_api_level();
    }

    int SystemTraceStartTrigger(const char* trigger, char* error, uint32_t error_size)
    {
        char* const arguments[] = { (char*)TRIGGER_HELPER, (char*)trigger, 0 };
        // Only async-signal-safe calls are made between fork and exec. In
        // particular, the child never enters JNI or the engine's allocators.
        pid_t process = fork();
        if (process == 0)
        {
            execv(TRIGGER_HELPER, arguments);
            _exit(127);
        }

        if (process < 0)
        {
            dmSnPrintf(error, error_size, "unable to start trigger_perfetto: %s", strerror(errno));
        }

        return (int)process;
    }

    SystemTraceState SystemTracePollTrigger(int* process, char* error, uint32_t error_size)
    {
        int status;
        pid_t result = waitpid((pid_t)*process, &status, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR))
        {
            return SYSTEM_TRACE_TRIGGER_PENDING;
        }

        *process = 0;
        if (result < 0)
        {
            dmSnPrintf(error, error_size, "unable to wait for trigger_perfetto: %s", strerror(errno));
            return SYSTEM_TRACE_FAILED;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            return SYSTEM_TRACE_TRIGGER_SENT;
        }

        if (WIFEXITED(status))
        {
            dmSnPrintf(error, error_size, "trigger_perfetto exited with code %d", WEXITSTATUS(status));
        }
        else
        {
            dmSnPrintf(error, error_size, "trigger_perfetto terminated before reporting success");
        }

        return SYSTEM_TRACE_FAILED;
    }

    void SystemTraceStopTrigger(int* process)
    {
        // This terminates only our signal-delivery helper. The privileged
        // recorder belongs to the host and must not be stopped by the app.
        kill((pid_t)*process, SIGKILL);
        while (waitpid((pid_t)*process, 0, 0) < 0 && errno == EINTR)
        {
        }

        *process = 0;
    }

}
