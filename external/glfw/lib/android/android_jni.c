// Copyright 2020-2023 The Defold Foundation
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

#include "android_jni.h"
#include <assert.h>

// Embed hosts often call into glfw/engine from the Java UI thread (already
// attached by ART). Unconditionally DetachCurrentThread() on that thread
// aborts with: "attempting to detach while still running code".
static __thread int t_jni_lock_count = 0;
static __thread int t_jni_we_attached = 0;

JNIEnv* JNIAttachCurrentThread()
{
    JavaVM* vm = g_AndroidApp->activity->vm;
    JNIEnv* env = 0;

    jint res = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED)
    {
        JavaVMAttachArgs lJavaVMAttachArgs;
        lJavaVMAttachArgs.version = JNI_VERSION_1_6;
        lJavaVMAttachArgs.name = "NativeThread";
        lJavaVMAttachArgs.group = NULL;

        (*vm)->AttachCurrentThread(vm, &env, &lJavaVMAttachArgs);
        t_jni_we_attached = 1;
    }
    t_jni_lock_count++;
    return env;
}

void JNIDetachCurrentThread()
{
    if (t_jni_lock_count <= 0)
        return;

    t_jni_lock_count--;
    if (t_jni_lock_count == 0 && t_jni_we_attached)
    {
        JavaVM* vm = g_AndroidApp->activity->vm;
        (*vm)->DetachCurrentThread(vm);
        t_jni_we_attached = 0;
    }
}

void JNIAttachCurrentThreadIfNeeded(int* did_attach)
{
    JavaVM* vm = g_AndroidApp->activity->vm;
    JNIEnv* env = 0;
    assert(did_attach != NULL);
    *did_attach = 0;

    jint res = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED)
    {
        JavaVMAttachArgs lJavaVMAttachArgs;
        lJavaVMAttachArgs.version = JNI_VERSION_1_6;
        lJavaVMAttachArgs.name = "NativeThread";
        lJavaVMAttachArgs.group = NULL;

        if ((*vm)->AttachCurrentThread(vm, &env, &lJavaVMAttachArgs) == JNI_OK)
        {
            *did_attach = 1;
        }
    }
}

void JNIDetachCurrentThreadIfNeeded(int did_attach)
{
    if (did_attach)
    {
        JavaVM* vm = g_AndroidApp->activity->vm;
        (*vm)->DetachCurrentThread(vm);
    }
}

JNIEnv* JNIBeginActivity(int* did_attach)
{
    assert(did_attach != NULL);
    *did_attach = 0;
    if (!g_AndroidApp || !g_AndroidApp->activity || !g_AndroidApp->activity->vm)
        return 0;

    JNIAttachCurrentThreadIfNeeded(did_attach);

    JNIEnv* env = 0;
    if ((*g_AndroidApp->activity->vm)->GetEnv(g_AndroidApp->activity->vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
    {
        JNIDetachCurrentThreadIfNeeded(*did_attach);
        *did_attach = 0;
        return 0;
    }
    return env;
}

jmethodID JNIGetMethodID(JNIEnv* env, jobject instance, const char* method, const char* signature)
{
    if (instance == 0 || !env) return 0;
    jclass clazz = (*env)->GetObjectClass(env, instance);
    jmethodID mid = (*env)->GetMethodID(env, clazz, method, signature);
    if ((*env)->ExceptionCheck(env))
    {
        // Method may be absent on embed host Activities — soft-fail.
        (*env)->ExceptionClear(env);
        mid = 0;
    }
    (*env)->DeleteLocalRef(env, clazz);
    return mid;
}
