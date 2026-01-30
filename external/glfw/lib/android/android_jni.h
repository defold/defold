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

#ifndef _ANDROID_JNI_H_
#define _ANDROID_JNI_H_

#include "internal.h"
#include <jni.h>

extern struct android_app* g_AndroidApp;

JNIEnv* JNIAttachCurrentThread();
void JNIDetachCurrentThread();
void JNIAttachCurrentThreadIfNeeded(int* did_attach);
void JNIDetachCurrentThreadIfNeeded(int did_attach);
jmethodID JNIGetMethodID(JNIEnv* env, jobject instance, char* method, char* signature);

#endif // _ANDROID_JNI_H_
