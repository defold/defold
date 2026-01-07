// Copyright 2020-2025 The Defold Foundation
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


#ifndef DM_JNI_H
#define DM_JNI_H

// https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/functions.html#Set_type_Field_routines

#include <stdint.h>
#include <jni.h>
#include <setjmp.h>

#if defined(_WIN32)
    #include <Windows.h>
    #include <Dbghelp.h>
    #include <excpt.h>
#endif

namespace dmJNI
{
    // Jni helper functions
    jclass GetClass(JNIEnv* env, const char* basecls, const char* clsname);
    jclass GetFieldType(JNIEnv* env, jobject obj, jfieldID fieldID);
    jfieldID GetFieldFromString(JNIEnv* env, jclass cls, const char* field_name, const char* type_name);

    void SetObject(JNIEnv* env, jobject obj, jfieldID field, jobject value);
    void SetObjectDeref(JNIEnv* env, jobject obj, jfieldID field, jobject value);
    void SetBoolean(JNIEnv* env, jobject obj, jfieldID field, jboolean value);
    void SetByte(JNIEnv* env, jobject obj, jfieldID field, jbyte value);
    void SetUByte(JNIEnv* env, jobject obj, jfieldID field, jbyte value);
    void SetChar(JNIEnv* env, jobject obj, jfieldID field, jchar value);
    void SetShort(JNIEnv* env, jobject obj, jfieldID field, jshort value);
    void SetUShort(JNIEnv* env, jobject obj, jfieldID field, jshort value);
    void SetInt(JNIEnv* env, jobject obj, jfieldID field, jint value);
    void SetUInt(JNIEnv* env, jobject obj, jfieldID field, jint value);
    void SetLong(JNIEnv* env, jobject obj, jfieldID field, jlong value);
    void SetULong(JNIEnv* env, jobject obj, jfieldID field, jlong value);
    void SetFloat(JNIEnv* env, jobject obj, jfieldID field, jfloat value);
    void SetDouble(JNIEnv* env, jobject obj, jfieldID field, jdouble value);
    void SetString(JNIEnv* env, jobject obj, jfieldID field, const char* value);

    // Requires that the enum class has a "static Enum fromValue(int value)" function.
    void SetEnum(JNIEnv* env, jobject obj, jfieldID field, int value);

    bool        GetBoolean(JNIEnv* env, jobject obj, jfieldID field);
    char        GetChar(JNIEnv* env, jobject obj, jfieldID field);
    uint8_t     GetByte(JNIEnv* env, jobject obj, jfieldID field);
    uint8_t     GetUByte(JNIEnv* env, jobject obj, jfieldID field);
    int16_t     GetShort(JNIEnv* env, jobject obj, jfieldID field);
    uint16_t    GetUShort(JNIEnv* env, jobject obj, jfieldID field);
    int32_t     GetInt(JNIEnv* env, jobject obj, jfieldID field);
    uint32_t    GetUInt(JNIEnv* env, jobject obj, jfieldID field);
    int64_t     GetLong(JNIEnv* env, jobject obj, jfieldID field);
    uint64_t    GetULong(JNIEnv* env, jobject obj, jfieldID field);
    float       GetFloat(JNIEnv* env, jobject obj, jfieldID field);
    double      GetDouble(JNIEnv* env, jobject obj, jfieldID field);
    char*       GetString(JNIEnv* env, jobject obj, jfieldID field);

    // Requires that the enum class has a "int getValue()" function.
    int     GetEnum(JNIEnv* env, jobject obj, jfieldID field);

    jbooleanArray   C2J_CreateBooleanArray(JNIEnv* env, const bool* data, uint32_t data_count);
    jbyteArray      C2J_CreateByteArray(JNIEnv* env, const int8_t* data, uint32_t data_count);
    jbyteArray      C2J_CreateUByteArray(JNIEnv* env, const uint8_t* data, uint32_t data_count);
    jcharArray      C2J_CreateCharArray(JNIEnv* env, const char* data, uint32_t data_count);
    jshortArray     C2J_CreateShortArray(JNIEnv* env, const int16_t* data, uint32_t data_count);
    jshortArray     C2J_CreateUShortArray(JNIEnv* env, const uint16_t* data, uint32_t data_count);
    jintArray       C2J_CreateIntArray(JNIEnv* env, const int32_t* data, uint32_t data_count);
    jintArray       C2J_CreateUIntArray(JNIEnv* env, const uint32_t* data, uint32_t data_count);
    jlongArray      C2J_CreateLongArray(JNIEnv* env, const int64_t* data, uint32_t data_count);
    jlongArray      C2J_CreateULongArray(JNIEnv* env, const uint64_t* data, uint32_t data_count);
    jfloatArray     C2J_CreateFloatArray(JNIEnv* env, const float* data, uint32_t data_count);
    jdoubleArray    C2J_CreateDoubleArray(JNIEnv* env, const double* data, uint32_t data_count);

    bool*           J2C_CreateBooleanArray(JNIEnv* env, jbooleanArray arr, uint32_t* out_count);
    uint8_t*        J2C_CreateByteArray(JNIEnv* env, jbyteArray arr, uint32_t* out_count);
    uint8_t*        J2C_CreateUByteArray(JNIEnv* env, jbyteArray arr, uint32_t* out_count);
    char*           J2C_CreateCharArray(JNIEnv* env, jcharArray arr, uint32_t* out_count);
    int16_t*        J2C_CreateShortArray(JNIEnv* env, jshortArray arr, uint32_t* out_count);
    uint16_t*       J2C_CreateUShortArray(JNIEnv* env, jshortArray arr, uint32_t* out_count);
    int32_t*        J2C_CreateIntArray(JNIEnv* env, jintArray arr, uint32_t* out_count);
    uint32_t*       J2C_CreateUIntArray(JNIEnv* env, jintArray arr, uint32_t* out_count);
    int64_t*        J2C_CreateLongArray(JNIEnv* env, jlongArray arr, uint32_t* out_count);
    uint64_t*       J2C_CreateULongArray(JNIEnv* env, jlongArray arr, uint32_t* out_count);
    float*          J2C_CreateFloatArray(JNIEnv* env, jfloatArray arr, uint32_t* out_count);
    double*         J2C_CreateDoubleArray(JNIEnv* env, jdoubleArray arr, uint32_t* out_count);

    void            J2C_CopyBooleanArray(JNIEnv* env, jbooleanArray arr, bool* dst, uint32_t dst_count);
    void            J2C_CopyByteArray(JNIEnv* env, jbyteArray arr, uint8_t* dst, uint32_t dst_count);
    void            J2C_CopyCharArray(JNIEnv* env, jcharArray arr, char* dst, uint32_t dst_count);
    void            J2C_CopyShortArray(JNIEnv* env, jshortArray arr, int16_t* dst, uint32_t dst_count);
    void            J2C_CopyIntArray(JNIEnv* env, jintArray arr, int32_t* dst, uint32_t dst_count);
    void            J2C_CopyLongArray(JNIEnv* env, jlongArray arr, int64_t* dst, uint32_t dst_count);
    void            J2C_CopyFloatArray(JNIEnv* env, jfloatArray arr, float* dst, uint32_t dst_count);
    void            J2C_CopyDoubleArray(JNIEnv* env, jdoubleArray arr, double* dst, uint32_t dst_count);

    void EnableDefaultSignalHandlers(JavaVM* vm);
    void EnableSignalHandlers(void* ctx, void (*callback)(int signal, void* ctx));
    void DisableSignalHandlers();

    // Used to enable a jni context for a small period of time
    bool IsContextAdded(JNIEnv* env);
    void AddContext(JNIEnv* env, void* user_data);
    void RemoveContext(JNIEnv* env);
    void* GetContextUserData(JNIEnv* env);

    struct ScopedString
    {
        JNIEnv* m_Env;
        jstring m_JString;
        const char* m_String;
        ScopedString(JNIEnv* env, jstring str)
        : m_Env(env)
        , m_JString(str)
        , m_String(str ? env->GetStringUTFChars(str, JNI_FALSE) : 0)
        {
        }
        ~ScopedString()
        {
            if (m_String)
            {
                m_Env->ReleaseStringUTFChars(m_JString, m_String);
            }
        }
    };

    struct ScopedByteArray
    {
        JNIEnv*     m_Env;
        jbyte*      m_Array;
        jsize       m_ArraySize;
        jbyteArray  m_JArray;
        ScopedByteArray(JNIEnv* env, jbyteArray arr)
        : m_Env(env)
        , m_Array(arr ? env->GetByteArrayElements(arr, 0) : 0)
        , m_ArraySize(arr ? env->GetArrayLength(arr) : 0)
        , m_JArray(arr)
        {
        }
        ~ScopedByteArray()
        {
            if (m_Array && m_JArray)
            {
                m_Env->ReleaseByteArrayElements(m_JArray, m_Array, JNI_ABORT);
            }
        }
    };

    struct ScopedByteArrayCritical
    {
        JNIEnv*    m_Env;
        jbyte*     m_Array;
        jsize      m_ArraySize;
        jbyteArray m_JArray;
        ScopedByteArrayCritical(JNIEnv* env, jbyteArray arr)
            : m_Env(env)
            , m_Array(arr ? (jbyte*)env->GetPrimitiveArrayCritical(arr, 0) : 0)
            , m_ArraySize(arr ? env->GetArrayLength(arr) : 0)
            , m_JArray(arr)
        {
        }
        ~ScopedByteArrayCritical()
        {
            if (m_Array && m_JArray)
            {
                m_Env->ReleasePrimitiveArrayCritical(m_JArray, m_Array, JNI_ABORT);
            }
        }
    };

    struct SignalContextScope
    {
        JNIEnv* m_Env;
        jmp_buf m_JumpBuf;
        SignalContextScope(JNIEnv* env) : m_Env(env)
        {
            AddContext(m_Env, this);
        }
        ~SignalContextScope()
        {
            if (m_Env)
                RemoveContext(m_Env);
        }
        void LongJmp(int val) {
            RemoveContext(m_Env);
            m_Env = 0;
            longjmp(m_JumpBuf, val);
        }
    };

    #define DM_SCOPED_SIGNAL_CONTEXT(_ENV, ...)                     \
        dmJNI::SignalContextScope _signal_scope(_ENV);              \
        int _signal_scope_result = setjmp(_signal_scope.m_JumpBuf); \
        if (_signal_scope_result != 0) {                            \
            printf("Error occurred in %s\n", __FUNCTION__);         \
            __VA_ARGS__                                             \
        }

#if defined(_WIN32)
    LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ptr);

    #define DM_JNI_GUARD_SCOPE_BEGIN() \
        __try { \

    #define DM_JNI_GUARD_SCOPE_END(...) \
        } __except (dmJNI::ExceptionHandler(GetExceptionInformation())) { \
            __VA_ARGS__ \
        }

#else
    #define DM_JNI_GUARD_SCOPE_BEGIN() { int _scope_result = 0;
    #define DM_JNI_GUARD_SCOPE_END(...) \
            if (_scope_result) { \
                __VA_ARGS__ \
            } \
        }

#endif

    // The output is the same as "buffer"
    char* GenerateCallstack(char* buffer, uint32_t buffer_length);

// for testing
// Pass in a string containing any of the SIG* numbers
    void TestSignalFromString(const char* signal);

    void PrintString(JNIEnv* env, jstring string);
}

#endif
