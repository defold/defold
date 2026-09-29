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


JNIEnv* JNIAttachCurrentThread()
{
    JavaVM* vm = g_AndroidApp->activity->vm;
    JNIEnv* env = 0;

    JavaVMAttachArgs lJavaVMAttachArgs;
    lJavaVMAttachArgs.version = JNI_VERSION_1_6;
    lJavaVMAttachArgs.name = "NativeThread";
    lJavaVMAttachArgs.group = NULL;

    (*vm)->AttachCurrentThread(vm, &env, &lJavaVMAttachArgs);
    return env;
}

void JNIDetachCurrentThread()
{
    JavaVM* vm = g_AndroidApp->activity->vm;
    (*vm)->DetachCurrentThread(vm);
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

int JNICheckAndClearException(JNIEnv* env)
{
    if (!(*env)->ExceptionCheck(env))
    {
        return 0;
    }

    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return 1;
}

jmethodID JNIGetMethodID(JNIEnv* env, jobject instance, const char* method, const char* signature)
{
    if (instance == 0) return 0;
    jclass clazz = (*env)->GetObjectClass(env, instance);
    if (JNICheckAndClearException(env) || clazz == 0)
    {
        if (clazz != 0)
        {
            (*env)->DeleteLocalRef(env, clazz);
        }
        return 0;
    }

    jmethodID method_id = (*env)->GetMethodID(env, clazz, method, signature);
    int exception = JNICheckAndClearException(env);
    (*env)->DeleteLocalRef(env, clazz);
    return exception ? 0 : method_id;
}
