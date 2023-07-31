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

#include "testapi.h"
#include "Testapi_jni.h"
#include "jni_util.h"

#include <jni.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateVec2i(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreateVec2i: env = %p\n", env);
    dmJNI::SignalContextScope env_scope(env);

    dmJniTest::TypeInfos types;
    dmJniTest::InitializeJNITypes(env, &types);

    jobject jvec = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmJniTest::Vec2i vec;
        vec.x = 1;
        vec.y = 2;
        jvec = dmJniTest::CreateVec2i(env, &types, &vec);
    DM_JNI_GUARD_SCOPE_END(return 0;);

    //dmJniTest::FinalizeJNITypes(env, &types);
    return jvec;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateRecti(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreatRect2i: env = %p\n", env);
    dmJNI::SignalContextScope env_scope(env);

    dmJniTest::TypeInfos types;
    dmJniTest::InitializeJNITypes(env, &types);

    jobject jrect = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmJniTest::Recti rect;
        rect.m_Min.x = -2;
        rect.m_Min.y = -3;
        rect.m_Max.x = 4;
        rect.m_Max.y = 5;
        jrect = dmJniTest::CreateRecti(env, &types, &rect);
    DM_JNI_GUARD_SCOPE_END(return 0;);

    //dmJniTest::FinalizeJNITypes(env, &types);
    return jrect;
}

// JNIEXPORT void JNICALL Java_JniTest_TestException(JNIEnv* env, jclass cls, jstring j_message)
// {
//     dmJNI::SignalContextScope env_scope(env);
//     ScopedString s_message(env, j_message);
//     const char* message = s_message.m_String;
//     printf("Received message: %s\n", message);
//     dmJNI::TestSignalFromString(message);
// }

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

    dmLogInfo("JNI_OnLoad ->\n");
    dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass("com/dynamo/bob/pipeline/JniTest");
    dmLogInfo("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. JniTest.java)
    static const JNINativeMethod methods[] = {
        {"TestCreateVec2i", "()L" CLASS_NAME "$Vec2i;", reinterpret_cast<void*>(Java_JniTest_TestCreateVec2i)},
        {"TestCreateRecti", "()L" CLASS_NAME "$Recti;", reinterpret_cast<void*>(Java_JniTest_TestCreateRecti)},
        //{"TestException", "(Ljava/lang/String;)V", reinterpret_cast<void*>(Java_JniTest_TestException)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogInfo("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogInfo("JNI_OnUnload ->\n");
}
