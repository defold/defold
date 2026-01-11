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

#include "testapi.h"
#include "Testapi_jni.h"
#include <jni/jni_util.h>

#include <jni.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>

#define CLASS_NAME_JNI_TEST "com/dynamo/bob/pipeline/JniTest"

dmJniTest::Arrays::Arrays()
: m_Data(0)
, m_DataCount(0)
, m_Rects(0)
, m_RectsCount(0)
, m_RectPtrs1(0)
, m_RectPtrs1Count(0)
{
    memset(m_Array1D_float, 0, sizeof(m_Array1D_float));
    memset(m_Array1D_vec2i, 0, sizeof(m_Array1D_vec2i));
    memset(m_Array1D_vec2i_ptr, 0, sizeof(m_Array1D_vec2i_ptr));
}

dmJniTest::Arrays::~Arrays()
{
    delete[] m_Data;
    delete[] m_Rects;

    for (uint32_t i = 0; i < m_RectPtrs1Count; ++i)
    {
        delete m_RectPtrs1[i];
    }

    for (uint32_t i = 0; i < m_RectPtrs2.Size(); ++i)
    {
        delete m_RectPtrs2[i];
    }
    m_RectPtrs2.SetSize(0);

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Array1D_vec2i_ptr); ++i)
    {
        delete m_Array1D_vec2i_ptr[i];
    }
}

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateVec2i(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreateVec2i: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jvec = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmJniTest::Vec2i vec;
        vec.x = 1;
        vec.y = 2;
        jvec = dmJniTest::jni::C2J_CreateVec2i(env, &jni_scope.m_TypeInfos, &vec);
    DM_JNI_GUARD_SCOPE_END(return 0;);

    return jvec;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateRecti(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreatRect2i: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jrect = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmJniTest::Recti rect;
        rect.m_Min.x = -2;
        rect.m_Min.y = -3;
        rect.m_Max.x = 4;
        rect.m_Max.y = 5;
        jrect = dmJniTest::jni::C2J_CreateRecti(env, &jni_scope.m_TypeInfos, &rect);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jrect;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateMisc(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreateMisc:\n");
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jmisc = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        const char* s = "Hello World!";
        dmJniTest::Misc misc;
        misc.m_TestEnum = dmJniTest::TE_VALUE_B;
        misc.m_String = s;
        misc.m_Opaque = (void*)(uintptr_t)42;
        misc.m_NullPtr = 0;
        misc.m_Forward1 = new dmJniTest::Forward();
        misc.m_Forward1->i8 = (int8_t)-0x7F;
        misc.m_Forward1->u8 = (uint8_t)0xFF; // -1
        misc.m_Forward1->i16 = (int16_t)-0x7FFF;
        misc.m_Forward1->u16 = (uint16_t)0xFFFF; // -1
        misc.m_Forward1->i32 = (int32_t)-0x7FFFFFFF;
        misc.m_Forward1->u32 = (uint32_t)0xFFFFFFFF; // -1
        misc.m_Forward1->i64 = (int64_t)-0x7FFFFFFFFFFFFFFF;
        misc.m_Forward1->u64 = (uint64_t)0xFFFFFFFFFFFFFFFF; // -1
        misc.m_Forward1->b   = true;

        jmisc = dmJniTest::jni::C2J_CreateMisc(env, &jni_scope.m_TypeInfos, &misc);
        delete misc.m_Forward1;
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jmisc;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestDuplicateMisc(JNIEnv* env, jclass cls, jobject jni_misc)
{
    dmLogInfo("Java_JniTest_TestDuplicateMisc: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jni_out_misc = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();

        dmJniTest::Misc in_misc = {};
        dmJniTest::jni::J2C_CreateMisc(env, &jni_scope.m_TypeInfos, jni_misc, &in_misc);

        // copy and modify
        dmJniTest::Misc out_misc = {};
        out_misc.m_TestEnum = (dmJniTest::TestEnum)(in_misc.m_TestEnum + 1);
        out_misc.m_String = strdup(in_misc.m_String);
        char* s = strstr((char*)out_misc.m_String, "Java");
        if (s)
        {
            s[0] = 'C';
            s[1] = '!';
            s[2] = '\0';
        }

        out_misc.m_Opaque = (void*)((uintptr_t)in_misc.m_Opaque + 1);
        out_misc.m_NullPtr = 0;

        out_misc.m_Forward1 = new dmJniTest::Forward();
        memcpy(&out_misc.m_Forward1, &in_misc.m_Forward1, sizeof(out_misc.m_Forward1));

        jni_out_misc = dmJniTest::jni::C2J_CreateMisc(env, &jni_scope.m_TypeInfos, &out_misc);

        delete out_misc.m_Forward1;
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jni_out_misc;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestDuplicateRecti(JNIEnv* env, jclass cls, jobject jni_rect)
{
    dmLogInfo("Java_JniTest_TestDuplicateRecti: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jni_out_rect = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();

        dmJniTest::Recti in_rect = {};
        dmJniTest::jni::J2C_CreateRecti(env, &jni_scope.m_TypeInfos, jni_rect, &in_rect);

        // copy and modify
        dmJniTest::Recti out_rect;
        out_rect.m_Min.x = in_rect.m_Min.x + 1;
        out_rect.m_Min.y = in_rect.m_Min.y + 1;
        out_rect.m_Max.x = in_rect.m_Max.x + 1;
        out_rect.m_Max.y = in_rect.m_Max.y + 1;
        jni_out_rect = dmJniTest::jni::C2J_CreateRecti(env, &jni_scope.m_TypeInfos, &out_rect);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jni_out_rect;
}

JNIEXPORT jobject JNICALL Java_JniTest_TestCreateArrays(JNIEnv* env, jclass cls)
{
    dmLogInfo("Java_JniTest_TestCreateArrays: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jdata = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmJniTest::Arrays arrays;

        uint8_t data[] = {1,2,4,8};
        arrays.m_Data = data;
        arrays.m_DataCount = DM_ARRAY_SIZE(data);

        uint8_t data2[] = {2,4,8,16,32};
        arrays.m_Data2.Set(data2, DM_ARRAY_SIZE(data2), DM_ARRAY_SIZE(data2), true);

        dmJniTest::Recti rects[] = {
            { {1,2}, {3,4} },
            { {5,6}, {7,8} },
            { {9,10}, {11,12} }
        };

        arrays.m_Rects = rects;
        arrays.m_RectsCount = DM_ARRAY_SIZE(rects);
        arrays.m_Rects2.Set(rects, DM_ARRAY_SIZE(rects), DM_ARRAY_SIZE(rects), true);

        dmJniTest::Recti* rect_ptrs[] = {
            &rects[0],
            &rects[1],
            &rects[2]
        };

        arrays.m_RectPtrs1 = rect_ptrs;
        arrays.m_RectPtrs1Count = DM_ARRAY_SIZE(rect_ptrs);

        arrays.m_RectPtrs2.Set(rect_ptrs, DM_ARRAY_SIZE(rect_ptrs), DM_ARRAY_SIZE(rect_ptrs), true);

        arrays.m_Array1D_float[0] = 1.2f;
        arrays.m_Array1D_float[1] = 2.5f;
        arrays.m_Array1D_float[2] = 3.1f;
        arrays.m_Array1D_vec2i[0] = dmJniTest::Vec2i(1,2);
        arrays.m_Array1D_vec2i[1] = dmJniTest::Vec2i(3,4);
        arrays.m_Array1D_vec2i_ptr[0] = &arrays.m_Array1D_vec2i[1];
        arrays.m_Array1D_vec2i_ptr[1] = &arrays.m_Array1D_vec2i[0];

        jdata = dmJniTest::jni::C2J_CreateArrays(env, &jni_scope.m_TypeInfos, &arrays);

        // Reset these since they're stack pointers
        arrays.m_Data = 0;
        arrays.m_Rects = 0;
        arrays.m_RectPtrs1 = 0;
        arrays.m_RectPtrs1Count = 0;
        arrays.m_RectPtrs2.SetSize(0);
        arrays.m_Array1D_vec2i_ptr[0] = 0;
        arrays.m_Array1D_vec2i_ptr[1] = 0;

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jdata;
}


JNIEXPORT jobject JNICALL Java_JniTest_TestDuplicateArrays(JNIEnv* env, jclass cls, jobject jni_arrays)
{
    dmLogInfo("Java_JniTest_TestDuplicateArrays: env = %p\n", env);
    DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);
    dmJniTest::jni::ScopedContext jni_scope(env);

    jobject jni_out_arrays = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();

        dmJniTest::Arrays in_arrays;
        dmJniTest::jni::J2C_CreateArrays(env, &jni_scope.m_TypeInfos, jni_arrays, &in_arrays);

        // copy and modify
        dmJniTest::Arrays out_arrays;
        out_arrays.m_DataCount = in_arrays.m_DataCount;
        out_arrays.m_Data = new uint8_t[out_arrays.m_DataCount];
        for (uint32_t i = 0; i < in_arrays.m_DataCount; ++i)
        {
            out_arrays.m_Data[i] = in_arrays.m_Data[i]+1;
        }

        out_arrays.m_RectsCount = in_arrays.m_RectsCount;
        out_arrays.m_Rects = new dmJniTest::Recti[out_arrays.m_RectsCount];
        for (uint32_t i = 0; i < in_arrays.m_RectsCount; ++i)
        {
            out_arrays.m_Rects[i].m_Min.x = in_arrays.m_Rects[i].m_Min.x+1;
            out_arrays.m_Rects[i].m_Min.y = in_arrays.m_Rects[i].m_Min.y+1;
            out_arrays.m_Rects[i].m_Max.x = in_arrays.m_Rects[i].m_Max.x+1;
            out_arrays.m_Rects[i].m_Max.y = in_arrays.m_Rects[i].m_Max.y+1;
        }

        out_arrays.m_Data2.SetCapacity(in_arrays.m_Data2.Capacity());
        out_arrays.m_Data2.SetSize(in_arrays.m_Data2.Size());
        for (uint32_t i = 0; i < in_arrays.m_Data2.Size(); ++i)
        {
            out_arrays.m_Data2[i] = in_arrays.m_Data2[i] + 1;
        }

        out_arrays.m_Rects2.SetCapacity(in_arrays.m_Rects2.Capacity());
        out_arrays.m_Rects2.SetSize(in_arrays.m_Rects2.Size());
        for (uint32_t i = 0; i < in_arrays.m_Rects2.Size(); ++i)
        {
            out_arrays.m_Rects2[i].m_Min.x = in_arrays.m_Rects2[i].m_Min.x+1;
            out_arrays.m_Rects2[i].m_Min.y = in_arrays.m_Rects2[i].m_Min.y+1;
            out_arrays.m_Rects2[i].m_Max.x = in_arrays.m_Rects2[i].m_Max.x+1;
            out_arrays.m_Rects2[i].m_Max.y = in_arrays.m_Rects2[i].m_Max.y+1;
        }

        out_arrays.m_RectPtrs1Count = in_arrays.m_RectPtrs1Count;
        out_arrays.m_RectPtrs1 = new dmJniTest::Recti*[in_arrays.m_RectPtrs1Count];
        for (uint32_t i = 0; i < in_arrays.m_RectPtrs1Count; ++i)
        {
            out_arrays.m_RectPtrs1[i] = new dmJniTest::Recti();
            out_arrays.m_RectPtrs1[i]->m_Min.x = in_arrays.m_RectPtrs1[i]->m_Min.x+1;
            out_arrays.m_RectPtrs1[i]->m_Min.y = in_arrays.m_RectPtrs1[i]->m_Min.y+1;
            out_arrays.m_RectPtrs1[i]->m_Max.x = in_arrays.m_RectPtrs1[i]->m_Max.x+1;
            out_arrays.m_RectPtrs1[i]->m_Max.y = in_arrays.m_RectPtrs1[i]->m_Max.y+1;
        }

        out_arrays.m_RectPtrs2.SetCapacity(in_arrays.m_RectPtrs2.Capacity());
        out_arrays.m_RectPtrs2.SetSize(in_arrays.m_RectPtrs2.Size());
        for (uint32_t i = 0; i < in_arrays.m_RectPtrs2.Size(); ++i)
        {
            out_arrays.m_RectPtrs2[i] = new dmJniTest::Recti();
            out_arrays.m_RectPtrs2[i]->m_Min.x = in_arrays.m_RectPtrs2[i]->m_Min.x+1;
            out_arrays.m_RectPtrs2[i]->m_Min.y = in_arrays.m_RectPtrs2[i]->m_Min.y+1;
            out_arrays.m_RectPtrs2[i]->m_Max.x = in_arrays.m_RectPtrs2[i]->m_Max.x+1;
            out_arrays.m_RectPtrs2[i]->m_Max.y = in_arrays.m_RectPtrs2[i]->m_Max.y+1;
        }

        for (uint32_t i = 0; i < DM_ARRAY_SIZE(in_arrays.m_Array1D_float); ++i)
        {
            out_arrays.m_Array1D_float[i] = in_arrays.m_Array1D_float[i] + 1.0f;
        }

        for (uint32_t i = 0; i < DM_ARRAY_SIZE(in_arrays.m_Array1D_vec2i); ++i)
        {
            out_arrays.m_Array1D_vec2i[i].x = in_arrays.m_Array1D_vec2i[i].x + 1.0f;
            out_arrays.m_Array1D_vec2i[i].y = in_arrays.m_Array1D_vec2i[i].y + 1.0f;
        }

        for (uint32_t i = 0; i < DM_ARRAY_SIZE(in_arrays.m_Array1D_vec2i_ptr); ++i)
        {
            out_arrays.m_Array1D_vec2i_ptr[i] = new dmJniTest::Vec2i(
                                                    in_arrays.m_Array1D_vec2i_ptr[i]->x + 1.0f,
                                                    in_arrays.m_Array1D_vec2i_ptr[i]->y + 1.0f);
        }

        jni_out_arrays = dmJniTest::jni::C2J_CreateArrays(env, &jni_scope.m_TypeInfos, &out_arrays);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return jni_out_arrays;
}

JNIEXPORT void JNICALL Java_JniTest_TestException(JNIEnv* env, jclass cls, jstring j_message)
{
    DM_SCOPED_SIGNAL_CONTEXT(env, return;);
    dmJNI::ScopedString s_message(env, j_message);
    const char* message = s_message.m_String;
    printf("Received message: '%s'\n", message); // Good when testing
    dmJNI::TestSignalFromString(message);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {

    dmLogInfo("JNI_OnLoad ->\n");
    dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass(CLASS_NAME_JNI_TEST);
    dmLogInfo("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
      return JNI_ERR;

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. JniTest.java)
    static const JNINativeMethod methods[] = {
        {(char*)"TestCreateVec2i", (char*)"()L" CLASS_NAME "$Vec2i;", reinterpret_cast<void*>(Java_JniTest_TestCreateVec2i)},
        {(char*)"TestCreateRecti", (char*)"()L" CLASS_NAME "$Recti;", reinterpret_cast<void*>(Java_JniTest_TestCreateRecti)},
        {(char*)"TestCreateArrays", (char*)"()L" CLASS_NAME "$Arrays;", reinterpret_cast<void*>(Java_JniTest_TestCreateArrays)},
        {(char*)"TestCreateMisc", (char*)"()L" CLASS_NAME "$Misc;", reinterpret_cast<void*>(Java_JniTest_TestCreateMisc)},

        {(char*)"TestDuplicateRecti", (char*)"(L" CLASS_NAME "$Recti;)L" CLASS_NAME "$Recti;", reinterpret_cast<void*>(Java_JniTest_TestDuplicateRecti)},
        {(char*)"TestDuplicateArrays", (char*)"(L" CLASS_NAME "$Arrays;)L" CLASS_NAME "$Arrays;", reinterpret_cast<void*>(Java_JniTest_TestDuplicateArrays)},
        {(char*)"TestDuplicateMisc", (char*)"(L" CLASS_NAME "$Misc;)L" CLASS_NAME "$Misc;", reinterpret_cast<void*>(Java_JniTest_TestDuplicateMisc)},

        {(char*)"TestException", (char*)"(Ljava/lang/String;)V", reinterpret_cast<void*>(Java_JniTest_TestException)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogInfo("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnUnload: GetEnv error\n");
        return;
    }

    jclass c = env->FindClass(CLASS_NAME_JNI_TEST);
    dmLogInfo("JNI_OnUnload: c = %p\n", c);
    if (c == 0)
      return;

    env->UnregisterNatives(c);
    env->DeleteLocalRef(c);
}
