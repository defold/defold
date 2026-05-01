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

#include "jni_util.h"
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/mutex.h>

#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <Windows.h>
    #include <Dbghelp.h>

#else
    #include <cxxabi.h>
    #include <execinfo.h>
#endif


namespace dmJNI
{

struct ContextPair
{
    JNIEnv* m_Env;
    void*   m_UserData;
};

dmMutex::HMutex g_AllowedContextsMutex = dmMutex::New();
static dmArray<ContextPair> g_AllowedContexts;

static void* g_UserContext = 0;
static int (*g_UserCallback)(int, void*) = 0;


// Helpers
jclass GetClass(JNIEnv* env, const char* basecls, const char* clsname)
{
    if (clsname)
    {
        char buffer[128];
        dmSnPrintf(buffer, sizeof(buffer), "%s$%s", basecls, clsname);
        return env->FindClass(buffer);
    }
    return env->FindClass(basecls);
}

jclass GetFieldType(JNIEnv* env, jobject obj, jfieldID fieldID)
{
    jclass cls = env->GetObjectClass(obj);
    jclass field_cls = env->FindClass("java/lang/reflect/Field");
    jmethodID getType = env->GetMethodID(field_cls, "getType", "()Ljava/lang/Class;");

    jobject field = env->ToReflectedField(cls, fieldID, JNI_FALSE); // java.lang.reflect.Field
    jobject type = env->CallObjectMethod(field, getType);

    env->DeleteLocalRef(field_cls);
    env->DeleteLocalRef(cls);
    return (jclass)type;
}

jfieldID GetFieldFromString(JNIEnv* env, jclass cls, const char* field_name, const char* type_name)
{
    jfieldID field = env->GetFieldID(cls, field_name, type_name);
    if (!field)
    {
        printf("ERROR: Field '%s' does not have type '%s'.\n", field_name, type_name);
        assert(0);
        return 0;
    }
    return field;
}

// *******************************************************************************

void SetObject(JNIEnv* env, jobject obj, jfieldID field, jobject value)
{
    env->SetObjectField(obj, field, value);
}

void SetObjectDeref(JNIEnv* env, jobject obj, jfieldID field, jobject value)
{
    env->SetObjectField(obj, field, value);
    env->DeleteLocalRef(value);
}

void SetBoolean(JNIEnv* env, jobject obj, jfieldID field, jboolean value)
{
    env->SetBooleanField(obj, field, value);
}

void SetByte(JNIEnv* env, jobject obj, jfieldID field, jbyte value)
{
    env->SetByteField(obj, field, value);
}

void SetUByte(JNIEnv* env, jobject obj, jfieldID field, jbyte value)
{
    SetByte(env, obj, field, value);
}

void SetChar(JNIEnv* env, jobject obj, jfieldID field, jchar value)
{
    env->SetCharField(obj, field, value);
}

void SetShort(JNIEnv* env, jobject obj, jfieldID field, jshort value)
{
    env->SetShortField(obj, field, value);
}

void SetUShort(JNIEnv* env, jobject obj, jfieldID field, jshort value)
{
    SetShort(env, obj, field, value);
}

void SetInt(JNIEnv* env, jobject obj, jfieldID field, jint value)
{
    env->SetIntField(obj, field, value);
}

void SetUInt(JNIEnv* env, jobject obj, jfieldID field, jint value)
{
    SetInt(env, obj, field, value);
}

void SetLong(JNIEnv* env, jobject obj, jfieldID field, jlong value)
{
    env->SetLongField(obj, field, value);
}

void SetULong(JNIEnv* env, jobject obj, jfieldID field, jlong value)
{
    SetLong(env, obj, field, value);
}

void SetFloat(JNIEnv* env, jobject obj, jfieldID field, jfloat value)
{
    env->SetFloatField(obj, field, value);
}

void SetDouble(JNIEnv* env, jobject obj, jfieldID field, jdouble value)
{
    env->SetDoubleField(obj, field, value);
}

void SetString(JNIEnv* env, jobject obj, jfieldID field, const char* value)
{
    if (value)
    {
        jstring jstr = env->NewStringUTF(value);
        env->SetObjectField(obj, field, jstr);
        env->DeleteLocalRef(jstr);
    }
}

char* GetClassName(JNIEnv* env, jclass cls, char* buffer, uint32_t buffer_len)
{
    jclass ccls = env->FindClass("java/lang/Class");
    jmethodID mid_getName = env->GetMethodID(ccls, "getName", "()Ljava/lang/String;");
    jstring jstr = (jstring)env->CallObjectMethod(cls, mid_getName);
    const char* str = env->GetStringUTFChars(jstr, 0);
    dmSnPrintf(buffer, buffer_len, "%s", str);
    env->ReleaseStringUTFChars(jstr, str);
    return buffer;
}

static jobject GetEnumObject(JNIEnv* env, jclass cls, int value)
{
    char signature[1024] = "(I)L";
    GetClassName(env, cls, signature+4, sizeof(signature)-4);
    signature[sizeof(signature)-1] = 0;

    uint32_t len = strlen(signature);
    if (len >= (sizeof(signature)-1))
    {
        fprintf(stderr, "Class name buffer too small: '%s'\n", signature);
        return 0;
    }
    signature[len++] = ';';
    signature[len] = 0;

    for (uint32_t i = 0; i < len; ++i)
    {
        if (signature[i] == '.')
            signature[i] = '/';
    }

    jmethodID fromValue = env->GetStaticMethodID(cls, "fromValue", signature);
    assert(fromValue != 0);
    return env->CallStaticObjectMethod(cls, fromValue, value);
}

void SetEnum(JNIEnv* env, jobject obj, jfieldID field, int value)
{
    jclass field_cls = GetFieldType(env, obj, field);
    jobject enumValue = GetEnumObject(env, field_cls, value);
    dmJNI::SetObjectDeref(env, obj, field, enumValue);
}

// **********************************************************************************

bool GetBoolean(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetBooleanField(obj, field);
}

char GetChar(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetCharField(obj, field);
}

uint8_t GetByte(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetByteField(obj, field);
}
uint8_t GetUByte(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetByteField(obj, field);
}

int16_t GetShort(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetShortField(obj, field);
}
uint16_t GetUShort(JNIEnv* env, jobject obj, jfieldID field)
{
    return (uint16_t)GetShort(env, obj, field);
}

int32_t GetInt(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetIntField(obj, field);
}
uint32_t GetUInt(JNIEnv* env, jobject obj, jfieldID field)
{
    return (uint32_t)GetInt(env, obj, field);
}

int64_t GetLong(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetLongField(obj, field);
}
uint64_t GetULong(JNIEnv* env, jobject obj, jfieldID field)
{
    return (uint64_t)GetLong(env, obj, field);
}

float GetFloat(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetFloatField(obj, field);
}

double GetDouble(JNIEnv* env, jobject obj, jfieldID field)
{
    return env->GetDoubleField(obj, field);
}

char* GetString(JNIEnv* env, jobject obj, jfieldID field)
{
    jstring jstr = (jstring) env->GetObjectField(obj, field);
    const char* str = env->GetStringUTFChars(jstr, 0);
    char* out = strdup(str);
    env->ReleaseStringUTFChars(jstr, str);
    env->DeleteLocalRef(jstr);
    return out;
}

int GetEnum(JNIEnv* env, jobject obj, jfieldID field)
{
    jclass field_cls = GetFieldType(env, obj, field);;
    jmethodID getValueMethod = env->GetMethodID(field_cls, "getValue", "()I");
    jobject value = env->GetObjectField(obj, field);
    int result = env->CallIntMethod(value, getValueMethod);
    env->DeleteLocalRef(field_cls);
    return result;
}

// **********************************************************************************

jbooleanArray C2J_CreateBooleanArray(JNIEnv* env, const bool* data, uint32_t data_count)
{
    jbooleanArray arr = env->NewBooleanArray(data_count);
    env->SetBooleanArrayRegion(arr, 0, data_count, (const jboolean*)data);
    return arr;
}

jbyteArray C2J_CreateByteArray(JNIEnv* env, const int8_t* data, uint32_t data_count)
{
    return C2J_CreateUByteArray(env, (uint8_t*)data, data_count);
}

jbyteArray C2J_CreateUByteArray(JNIEnv* env, const uint8_t* data, uint32_t data_count)
{
    jbyteArray arr = env->NewByteArray(data_count);
    env->SetByteArrayRegion(arr, 0, data_count, (const jbyte*)data);
    return arr;
}

jcharArray C2J_CreateCharArray(JNIEnv* env, const char* data, uint32_t data_count)
{
    jcharArray arr = env->NewCharArray(data_count);
    env->SetCharArrayRegion(arr, 0, data_count, (const jchar*)data);
    return arr;
}

jshortArray C2J_CreateShortArray(JNIEnv* env, const int16_t* data, uint32_t data_count)
{
    jshortArray arr = env->NewShortArray(data_count);
    env->SetShortArrayRegion(arr, 0, data_count, (const jshort*)data);
    return arr;
}

jshortArray C2J_CreateUShortArray(JNIEnv* env, const uint16_t* data, uint32_t data_count)
{
    return C2J_CreateShortArray(env, (int16_t*)data, data_count);
}

jintArray C2J_CreateIntArray(JNIEnv* env, const int32_t* data, uint32_t data_count)
{
    jintArray arr = env->NewIntArray(data_count);
    env->SetIntArrayRegion(arr, 0, data_count, (const jint*)data);
    return arr;
}

jintArray C2J_CreateUIntArray(JNIEnv* env, const uint32_t* data, uint32_t data_count)
{
    return C2J_CreateIntArray(env, (int32_t*)data, data_count);
}

jlongArray C2J_CreateLongArray(JNIEnv* env, const int64_t* data, uint32_t data_count)
{
    jlongArray arr = env->NewLongArray(data_count);
    env->SetLongArrayRegion(arr, 0, data_count, (const jlong*)data);
    return arr;
}

jlongArray C2J_CreateULongArray(JNIEnv* env, const uint64_t* data, uint32_t data_count)
{
    return C2J_CreateLongArray(env, (int64_t*)data, data_count);
}

jfloatArray C2J_CreateFloatArray(JNIEnv* env, const float* data, uint32_t data_count)
{
    jfloatArray arr = env->NewFloatArray(data_count);
    env->SetFloatArrayRegion(arr, 0, data_count, (const jfloat*)data);
    return arr;
}

jdoubleArray C2J_CreateDoubleArray(JNIEnv* env, const double* data, uint32_t data_count)
{
    jdoubleArray arr = env->NewDoubleArray(data_count);
    env->SetDoubleArrayRegion(arr, 0, data_count, (const jdouble*)data);
    return arr;
}

bool* J2C_CreateBooleanArray(JNIEnv* env, jbooleanArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jboolean* carr = env->GetBooleanArrayElements(arr, 0);
    bool* out = new bool[len];
    memcpy(out, carr, sizeof(bool)*len);
    env->ReleaseBooleanArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
uint8_t* J2C_CreateByteArray(JNIEnv* env, jbyteArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jbyte* carr = env->GetByteArrayElements(arr, 0);
    uint8_t* out = new uint8_t[len];
    memcpy(out, carr, sizeof(uint8_t)*len);
    env->ReleaseByteArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
uint8_t* J2C_CreateUByteArray(JNIEnv* env, jbyteArray arr, uint32_t* out_count)
{
    return J2C_CreateByteArray(env, arr, out_count);
}
char* J2C_CreateCharArray(JNIEnv* env, jcharArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jchar* carr = env->GetCharArrayElements(arr, 0);
    char* out = new char[len];
    memcpy(out, carr, sizeof(char)*len);
    env->ReleaseCharArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
int16_t* J2C_CreateShortArray(JNIEnv* env, jshortArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jshort* carr = env->GetShortArrayElements(arr, 0);
    int16_t* out = new int16_t[len];
    memcpy(out, carr, sizeof(int16_t)*len);
    env->ReleaseShortArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
uint16_t* J2C_CreateUShortArray(JNIEnv* env, jshortArray arr, uint32_t* out_count)
{
    return (uint16_t*)J2C_CreateShortArray(env, arr, out_count);
}
int32_t* J2C_CreateIntArray(JNIEnv* env, jintArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jint* carr = env->GetIntArrayElements(arr, 0);
    int32_t* out = new int32_t[len];
    memcpy(out, carr, sizeof(int32_t)*len);
    env->ReleaseIntArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
uint32_t* J2C_CreateUIntArray(JNIEnv* env, jintArray arr, uint32_t* out_count)
{
    return (uint32_t*)J2C_CreateIntArray(env, arr, out_count);
}
int64_t* J2C_CreateLongArray(JNIEnv* env, jlongArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jlong* carr = env->GetLongArrayElements(arr, 0);
    int64_t* out = new int64_t[len];
    memcpy(out, carr, sizeof(int64_t)*len);
    env->ReleaseLongArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
uint64_t* J2C_CreateULongArray(JNIEnv* env, jlongArray arr, uint32_t* out_count)
{
    return (uint64_t*)J2C_CreateLongArray(env, arr, out_count);
}
float* J2C_CreateFloatArray(JNIEnv* env, jfloatArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jfloat* carr = env->GetFloatArrayElements(arr, 0);
    float* out = new float[len];
    memcpy(out, carr, sizeof(float)*len);
    env->ReleaseFloatArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}
double* J2C_CreateDoubleArray(JNIEnv* env, jdoubleArray arr, uint32_t* out_count)
{
    jsize len = env->GetArrayLength(arr);
    jdouble* carr = env->GetDoubleArrayElements(arr, 0);
    double* out = new double[len];
    memcpy(out, carr, sizeof(double)*len);
    env->ReleaseDoubleArrayElements(arr, carr, 0);
    *out_count = (uint32_t)len;
    return out;
}

static uint32_t CheckArrayLen(JNIEnv* env, jarray arr, uint32_t expected)
{
    uint32_t len = (uint32_t)env->GetArrayLength(arr);
    if (len < expected)
    {
        printf("Number of elements are fewer than expected. Expected %u, but got %u\n", expected, len);
        return len;
    }
    return expected;
}

void J2C_CopyBooleanArray(JNIEnv* env, jbooleanArray arr, bool* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jboolean* carr = env->GetBooleanArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jboolean)*len);
    env->ReleaseBooleanArrayElements(arr, carr, 0);
}
void J2C_CopyByteArray(JNIEnv* env, jbyteArray arr, uint8_t* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jbyte* carr = env->GetByteArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jbyte)*len);
    env->ReleaseByteArrayElements(arr, carr, 0);
}
void J2C_CopyCharArray(JNIEnv* env, jcharArray arr, char* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jchar* carr = env->GetCharArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jchar)*len);
    env->ReleaseCharArrayElements(arr, carr, 0);
}
void J2C_CopyShortArray(JNIEnv* env, jshortArray arr, int16_t* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jshort* carr = env->GetShortArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jshort)*len);
    env->ReleaseShortArrayElements(arr, carr, 0);
}
void J2C_CopyIntArray(JNIEnv* env, jintArray arr, int32_t* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jint* carr = env->GetIntArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jint)*len);
    env->ReleaseIntArrayElements(arr, carr, 0);
}
void J2C_CopyLongArray(JNIEnv* env, jlongArray arr, int64_t* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jlong* carr = env->GetLongArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jlong)*len);
    env->ReleaseLongArrayElements(arr, carr, 0);
}
void J2C_CopyFloatArray(JNIEnv* env, jfloatArray arr, float* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jfloat* carr = env->GetFloatArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jfloat)*len);
    env->ReleaseFloatArrayElements(arr, carr, 0);
}
void J2C_CopyDoubleArray(JNIEnv* env, jdoubleArray arr, double* dst, uint32_t dst_count)
{
    uint32_t len = CheckArrayLen(env, arr, dst_count);
    jdouble* carr = env->GetDoubleArrayElements(arr, 0);
    memcpy(dst, carr, sizeof(jdouble)*len);
    env->ReleaseDoubleArrayElements(arr, carr, 0);
}

// ************************************************************************************

void PrintString(JNIEnv* env, jstring string)
{
    const char* str = env->GetStringUTFChars(string, 0);
    printf("%s\n", str);
    env->ReleaseStringUTFChars(string, str);
}

#if defined(_WIN32) || defined(__CYGWIN__)

typedef void (*FSignalHandler)(int);
static FSignalHandler   g_SignalHandlers[64];

static void DefaultSignalHandler(int sig) {
    int handled = g_UserCallback == 0 ? 0 : g_UserCallback(sig, g_UserContext);
    if (!handled)
    {
        g_SignalHandlers[sig](sig);
    }
}

#else

typedef void ( *sigaction_handler_t )( int, siginfo_t *, void * );
static struct sigaction g_SignalHandlers[64];

static void DefaultSignalHandler(int sig, siginfo_t* info, void* arg) {
    int handled = g_UserCallback == 0 ? 0 : g_UserCallback(sig, g_UserContext);
    if (!handled)
    {
        if ( ( g_SignalHandlers[sig].sa_sigaction != (sigaction_handler_t)SIG_IGN ) &&
             ( g_SignalHandlers[sig].sa_sigaction != (sigaction_handler_t)SIG_DFL ) )
        {
            g_SignalHandlers[sig].sa_sigaction(sig, info, arg);
        }
    }
}

#endif

// ************************************************************************************

#if defined(_WIN32) || defined(__CYGWIN__)

EXCEPTION_POINTERS* g_ExceptionPointers = 0;

LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ptr)
{
    g_ExceptionPointers = ptr;
    g_UserCallback(0xDEAD, g_UserContext);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void SetHandlers()
{
    g_SignalHandlers[SIGILL] = signal(SIGILL, DefaultSignalHandler);
    g_SignalHandlers[SIGABRT] = signal(SIGABRT, DefaultSignalHandler);
    g_SignalHandlers[SIGFPE] = signal(SIGFPE, DefaultSignalHandler);
    g_SignalHandlers[SIGSEGV] = signal(SIGSEGV, DefaultSignalHandler);
}

static void UnsetHandlers()
{
    signal(SIGILL, g_SignalHandlers[SIGILL]);
    signal(SIGABRT, g_SignalHandlers[SIGABRT]);
    signal(SIGFPE, g_SignalHandlers[SIGFPE]);
    signal(SIGSEGV, g_SignalHandlers[SIGSEGV]);
}
#else

static void SetHandler(int sig, struct sigaction* old)
{
    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
    #endif

    struct sigaction handler;
    memset(&handler, 0, sizeof(struct sigaction));
    sigemptyset(&handler.sa_mask);
    handler.sa_sigaction = DefaultSignalHandler;
    handler.sa_flags = SA_SIGINFO;

    if (old)
    {
        memset(old, 0, sizeof(struct sigaction));
        sigemptyset(&old->sa_mask);
    }

    sigaction(sig, &handler, old);

    #if !defined(_MSC_VER) && defined(__clang__)
        #pragma GCC diagnostic pop
    #endif
}

static void SetHandlers()
{
    SetHandler(SIGILL, &g_SignalHandlers[SIGILL]);
    SetHandler(SIGABRT, &g_SignalHandlers[SIGABRT]);
    SetHandler(SIGFPE, &g_SignalHandlers[SIGFPE]);
    SetHandler(SIGSEGV, &g_SignalHandlers[SIGSEGV]);
    SetHandler(SIGPIPE, &g_SignalHandlers[SIGPIPE]);
}
static void UnsetHandlers()
{
    SetHandler(SIGILL, 0);
    SetHandler(SIGABRT, 0);
    SetHandler(SIGFPE, 0);
    SetHandler(SIGSEGV, 0);
    SetHandler(SIGPIPE, 0);
}
#endif

bool IsContextAdded(JNIEnv* env)
{
    DM_MUTEX_SCOPED_LOCK(g_AllowedContextsMutex);
    for (uint32_t i = 0; i < g_AllowedContexts.Size(); ++i)
    {
        if (g_AllowedContexts[i].m_Env == env)
            return true;
    }
    return false;
}

void AddContext(JNIEnv* env, void* user_data)
{
    DM_MUTEX_SCOPED_LOCK(g_AllowedContextsMutex);
    if (IsContextAdded(env))
        return;
    if (g_AllowedContexts.Full())
        g_AllowedContexts.OffsetCapacity(4);
    ContextPair pair;
    pair.m_Env = env;
    pair.m_UserData = user_data;
    g_AllowedContexts.Push(pair);
}

void RemoveContext(JNIEnv* env)
{
    DM_MUTEX_SCOPED_LOCK(g_AllowedContextsMutex);
    for (uint32_t i = 0; i < g_AllowedContexts.Size(); ++i)
    {
        if (g_AllowedContexts[i].m_Env == env)
        {
            g_AllowedContexts.EraseSwap(i);
            return;
        }
    }
}

void* GetContextUserData(JNIEnv* env)
{
    DM_MUTEX_SCOPED_LOCK(g_AllowedContextsMutex);
    for (uint32_t i = 0; i < g_AllowedContexts.Size(); ++i)
    {
        if (g_AllowedContexts[i].m_Env == env)
        {
            return g_AllowedContexts[i].m_UserData;
        }
    }
    return 0;
}

static int DefaultJniSignalHandler(int sig, void* ctx)
{
    JavaVM* vm = (JavaVM*)ctx;
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK)
    {
        return 0;
    }

    if (env->ExceptionCheck())
    {
        return 1;
    }

    SignalContextScope* signal_scope = (SignalContextScope*)GetContextUserData(env);
    if (!signal_scope)
    {
        return 0;
    }

    char message[1024] = {0};
    uint32_t written = 0;
    written += dmSnPrintf(message+written, sizeof(message), "Exception in Defold JNI code. Signal %d\n", sig);

    GenerateCallstack(message+written, sizeof(message)-written);

    env->ThrowNew(env->FindClass("java/lang/Exception"), message);
    signal_scope->LongJmp(1);
    return 1;
}

void EnableSignalHandlers(void* ctx, int (*callback)(int signal, void* ctx))
{
    g_UserCallback = callback;
    g_UserContext = ctx;
    g_AllowedContexts.SetCapacity(4);

    memset(&g_SignalHandlers[0], 0, sizeof(g_SignalHandlers));
    SetHandlers();
}

void EnableDefaultSignalHandlers(JavaVM* vm)
{
    EnableSignalHandlers((void*)vm, DefaultJniSignalHandler);
}

void DisableSignalHandlers()
{
    UnsetHandlers();
    g_UserCallback = 0;
    g_UserContext = 0;
}

// For unit tests to make sure it still works
void TestSignalFromString(const char* message)
{
    if (strstr(message, "SIGILL"))    raise(SIGILL);
    if (strstr(message, "SIGABRT"))   raise(SIGABRT);
    if (strstr(message, "SIGFPE"))    raise(SIGFPE);
    if (strstr(message, "SIGSEGV"))   raise(SIGSEGV);
#if !defined(_WIN32)
    if (strstr(message, "SIGPIPE"))   raise(SIGPIPE);
    if (strstr(message, "SIGBUS"))    raise(SIGBUS);
#endif

    printf("No signal found in string: %s!\n", message);
}


#define APPEND_STRING(...) \
    if (output_cursor < output_size) \
    { \
        int result = dmSnPrintf(output + output_cursor, output_size - output_cursor, __VA_ARGS__); \
        if (result == -1) { \
            output_cursor = output_size; \
        } else { \
            output_cursor += result; \
        } \
    }


#if defined(_WIN32)


static uint32_t GetCallstackPointers(EXCEPTION_POINTERS* pep, void** ptrs, uint32_t num_ptrs)
{
    if (!pep)
    {
        // The API only accepts 62 or less
        uint32_t max = num_ptrs > 62 ? 62 : num_ptrs;
        num_ptrs = CaptureStackBackTrace(0, max, ptrs, 0);
    } else
    {
        HANDLE process = ::GetCurrentProcess();
        HANDLE thread = ::GetCurrentThread();
        uint32_t count = 0;

        // Initialize stack walking.
        STACKFRAME64 stack_frame;
        memset(&stack_frame, 0, sizeof(stack_frame));
        #if defined(_WIN64)
            int machine_type = IMAGE_FILE_MACHINE_AMD64;
            stack_frame.AddrPC.Offset = pep->ContextRecord->Rip;
            stack_frame.AddrFrame.Offset = pep->ContextRecord->Rbp;
            stack_frame.AddrStack.Offset = pep->ContextRecord->Rsp;
        #else
            int machine_type = IMAGE_FILE_MACHINE_I386;
            stack_frame.AddrPC.Offset = pep->ContextRecord->Eip;
            stack_frame.AddrFrame.Offset = pep->ContextRecord->Ebp;
            stack_frame.AddrStack.Offset = pep->ContextRecord->Esp;
        #endif
        stack_frame.AddrPC.Mode = AddrModeFlat;
        stack_frame.AddrFrame.Mode = AddrModeFlat;
        stack_frame.AddrStack.Mode = AddrModeFlat;
        while (StackWalk64(machine_type,
            process,
            thread,
            &stack_frame,
            pep->ContextRecord,
            NULL,
            &SymFunctionTableAccess64,
            &SymGetModuleBase64,
            NULL) &&
            count < num_ptrs) {
            ptrs[count++] = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
        }
        num_ptrs = count;
    }
    return num_ptrs;
}

HMODULE GetCurrentModule()
{   // NB: XP+ solution!
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)GetCurrentModule, &hModule);
    return hModule;
}

char* GenerateCallstack(char* output, uint32_t output_size)
{
    int output_cursor = 0;

    HANDLE process = ::GetCurrentProcess();

    ::SymSetOptions(SYMOPT_DEBUG);
    BOOL initialized = ::SymInitialize(process, 0, TRUE);

    if (!initialized)
    {
        APPEND_STRING("No symbols available (Failed to initialize: %d)\n", GetLastError());
    }

    void* stack[62];
    uint32_t count = GetCallstackPointers(g_ExceptionPointers, stack, sizeof(stack)/sizeof(stack[0]));

    // Get a nicer printout as well
    const int name_length = 1024;
    char symbolbuffer[sizeof(SYMBOL_INFO) + name_length * sizeof(char)*2];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolbuffer;
    symbol->MaxNameLen = name_length;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    DWORD displacement;
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for(uint32_t c = 0; c < count; c++)
    {
        DWORD64 address = (DWORD64)stack[c];
        APPEND_STRING("    %d: %p: ", c, address);

        const char* symbolname = "<unknown symbol>";
        DWORD64 symboladdress = address;

        DWORD64 symoffset = 0;

        if (::SymFromAddr(process, address, &symoffset, symbol))
        {
            symbolname = symbol->Name;
            symboladdress = symbol->Address;
        }

        const char* filename = "<unknown>";
        int line_number = 0;
        if (::SymGetLineFromAddr64(process, address, &displacement, &line))
        {
            filename = line.FileName;
            line_number = line.LineNumber;
        }

        APPEND_STRING("%s(%d): %s\n", filename, line_number, symbolname);
    }

    ::SymCleanup(process);

    APPEND_STRING("    # <- native");
    return output;
}

#else

static void ParseStacktraceLine(char* s, char** frame, char** module, char** address, char** symbol)
{
    char* iter;
    *frame = dmStrTok(s, " ", &iter);
    *module = dmStrTok(0, " ", &iter);
    *address = dmStrTok(0, " ", &iter);
    *symbol = dmStrTok(0, " ", &iter);
}

char* GenerateCallstack(char* output, uint32_t output_size)
{
    int output_cursor = 0;

    void* buffer[64];
    int num_pointers = backtrace(buffer, sizeof(buffer)/sizeof(buffer[0]));

    char** stacktrace = backtrace_symbols(buffer, num_pointers);
    for (uint32_t i = 0; i < num_pointers; ++i)
    {
        char* frame = 0;
        char* module = 0;
        char* address = 0;
        char* symbol = 0;
        ParseStacktraceLine(stacktrace[i], &frame, &module, &address, &symbol);
        if (!symbol)
        {
            APPEND_STRING("    %p: %s\n", buffer[i], stacktrace[i]);
        }
        else
        {
            int status;
            char* name = symbol;
#if defined(_WIN32)
            char* demangled = 0;
#else
            char* demangled = abi::__cxa_demangle(symbol, 0, 0, &status);
            if (status == 0 && demangled)
                name = demangled;
#endif
            APPEND_STRING("    %p: %.2s %20s %s %s\n", buffer[i], frame, module, address, name);

            free((void*)demangled);
        }
    }

    free(stacktrace);

    APPEND_STRING("    # <- native");
    return output;
}

#endif

#undef APPEND_STRING

} // namespace
