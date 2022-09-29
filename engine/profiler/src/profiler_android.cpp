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

#include <dlib/profile.h>
#include <dmsdk/dlib/android.h>

#include "profiler_private.h"
#include "profiler_proc_utils.h"

DM_PROPERTY_EXTERN(rmtp_Profiler);
DM_PROPERTY_BOOL(rmtp_AttachedToJVM, false, FrameReset, "Thread attached to JVM", &rmtp_Profiler);

extern struct android_app* __attribute__((weak)) g_AndroidApp;

void dmProfilerExt::SampleCpuUsage()
{
    // Should be implemented using JNI for Android 8+
    // see https://github.com/defold/defold/issues/3385
    dmProfilerExt::SampleProcCpuUsage();
}

uint64_t dmProfilerExt::GetMemoryUsage()
{
    return dmProfilerExt::GetProcMemoryUsage();
}

double dmProfilerExt::GetCpuUsage()
{
    return dmProfilerExt::GetProcCpuUsage();
}

void dmProfilerExt::UpdatePlatformProfiler()
{
    // Shows if thread was attached to JVM and wasn't detached
    JNIEnv* env = 0;
    DM_PROPERTY_SET_BOOL(rmtp_AttachedToJVM, g_AndroidApp->activity->vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK);
}
