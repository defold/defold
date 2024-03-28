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

jmethodID JNIGetMethodID(JNIEnv* env, jobject instance, char* method, char* signature)
{
    if (instance == 0) return 0;
    jclass clazz = (*env)->GetObjectClass(env, instance);
    return (*env)->GetMethodID(env, clazz, method, signature);
}