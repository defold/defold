// Copyright 2020-2024 The Defold Foundation
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

#include "shaderc.h"

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include <Shaderc_jni.h>

#include <dlib/array.h>
#include <dlib/log.h>

namespace dmShaderc
{

}

static jobject GetReflection(JNIEnv* env, jclass cls, jlong context)
{
    dmShaderc::jni::ScopedContext jni_scope(env);
    dmShaderc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

    dmShaderc::HShaderContext shader_ctx = (dmShaderc::HShaderContext) context;
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    return C2J_CreateShaderReflection(env, types, reflection);
}

static jlong CreateShaderContext(JNIEnv* env, jclass cls, jbyteArray array)
{
    jsize file_size = env->GetArrayLength(array);
    jbyte* file_data = env->GetByteArrayElements(array, 0);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext((const void*) file_data, (uint32_t) file_size);

    env->ReleaseByteArrayElements(array, file_data, JNI_ABORT);

    if (!shader_ctx)
    {
        dmLogError("Failed to load shader");
        return 0;
    }

    return (jlong) shader_ctx;
}

JNIEXPORT jobject JNICALL Java_ShadercJni_GetReflection(JNIEnv* env, jclass cls, jlong context)
{
    jobject reflection;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        reflection = GetReflection(env, cls, context);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return reflection;
}

// public static native Shaderc.ShaderContext CreateShaderContext(byte[] buffer);
JNIEXPORT jlong JNICALL Java_ShadercJni_CreateShaderContext(JNIEnv* env, jclass cls, jbyteArray array)
{
    jlong context;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        context = CreateShaderContext(env, cls, array);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return context;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad ->");
    //dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        dmLogDebug("JNI_OnLoad GetEnv error");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass( JAVA_PACKAGE_NAME "/ShadercJni");
    dmLogDebug("JNI_OnLoad: c = %p", c);

    if (c == 0)
    {
        return JNI_ERR;
    }

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. ModelImporter.java)
    static const JNINativeMethod methods[] = {
        { (char*) "CreateShaderContext", (char*) "([B)J", reinterpret_cast<void*>(Java_ShadercJni_CreateShaderContext)},
        { (char*) "GetReflection", (char*) "(J)L" CLASS_NAME "$ShaderReflection;", reinterpret_cast<void*>(Java_ShadercJni_GetReflection)},
    };
    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK)
    {
        return rc;
    }

    dmLogDebug("JNI_OnLoad return.");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogDebug("JNI_OnUnload ->");
}
