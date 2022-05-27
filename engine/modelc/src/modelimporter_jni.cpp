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

#include "modelimporter.h"

#include <jni.h>

#include <dlib/array.h>
#include <dlib/log.h>

#define CLASS_SCENE  "com/dynamo/bob/pipeline/ModelImporter$Scene"


static jobject CreateScene(JNIEnv* env)
{
    jclass clazz = env->FindClass(CLASS_SCENE);
    jobject obj = env->AllocObject(clazz);

    int ints[4] = { 2, 5, 8, 9 };
    jintArray rootNodeIndices = env->NewIntArray(4);
    jfieldID field = env->GetFieldID(clazz, "rootNodeIndices", "[I");
    env->SetIntArrayRegion(rootNodeIndices, 0, 4, (jint*)ints);
    env->SetObjectField(obj, field, rootNodeIndices);
    env->DeleteLocalRef(rootNodeIndices);

    return obj;
}

JNIEXPORT jobject JNICALL Java_ModelImporter_LoadFromBufferInternal(JNIEnv* env, jclass cls, jstring _path, jint buffer_len, jobject buffer)
{
    const char* path = env->GetStringUTFChars(_path, JNI_FALSE);

    /* Call into external dylib function */
    printf("MAWE: jni LoadScene: %s bytes: %d\n", path, buffer_len);

    env->ReleaseStringUTFChars(_path, path);

    jobject test = CreateScene(env);
    return test;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    printf("JNI_OnLoad ->\n");

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass("com/dynamo/bob/pipeline/ModelImporter");
    printf("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    // Register your class' native methods.
    static const JNINativeMethod methods[] = {
        {"LoadFromBufferInternal", "(Ljava/lang/String;I[B)L" CLASS_SCENE ";", reinterpret_cast<void*>(Java_ModelImporter_LoadFromBufferInternal)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    if (rc != JNI_OK) return rc;

    printf("JNI_OnLoad return.\n");
    return JNI_VERSION_1_6;
}

namespace dmModelImporter
{

}
