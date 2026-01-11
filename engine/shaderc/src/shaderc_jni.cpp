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

static jlong NewShaderContext(JNIEnv* env, jclass cls, jint stage, jbyteArray array)
{
    jsize file_size = env->GetArrayLength(array);
    jbyte* file_data = env->GetByteArrayElements(array, 0);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext((dmShaderc::ShaderStage) stage, (const void*) file_data, (uint32_t) file_size);

    env->ReleaseByteArrayElements(array, file_data, JNI_ABORT);

    if (!shader_ctx)
    {
        dmLogError("Failed to load shader");
        return 0;
    }

    return (jlong) shader_ctx;
}

// HShaderCompiler NewShaderCompiler(HShaderContext context, ShaderLanguage language);
static jlong NewShaderCompiler(JNIEnv* env, jclass cls, jlong context, jint language)
{
    dmShaderc::HShaderContext shader_ctx = (dmShaderc::HShaderContext) context;
    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, (dmShaderc::ShaderLanguage) language);

    if (!compiler)
    {
        dmLogError("Failed to create shader compiler");
        return 0;
    }

    return (jlong) compiler;
}

static jobject Compile(JNIEnv* env, jclass cls, jlong context, jlong compiler, jobject options)
{
    dmShaderc::jni::ScopedContext jni_scope(env);
    dmShaderc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

    dmShaderc::HShaderContext shader_ctx = (dmShaderc::HShaderContext) context;
    dmShaderc::HShaderCompiler shader_compiler = (dmShaderc::HShaderCompiler) compiler;

    dmShaderc::ShaderCompilerOptions shader_options;
    dmShaderc::jni::J2C_CreateShaderCompilerOptions(env, types, options, &shader_options);

    dmShaderc::ShaderCompileResult* res = dmShaderc::Compile(shader_ctx, shader_compiler, shader_options);

    jobject result = C2J_CreateShaderCompileResult(env, types, res);

    dmShaderc::FreeShaderCompileResult(res);

    return result;
}

// public static native byte[] Compile(long context, long compiler, Shaderc.ShaderCompilerOptions options);
JNIEXPORT jobject JNICALL Java_ShadercJni_Compile(JNIEnv* env, jclass cls, jlong context, jlong compiler, jobject options)
{
    jobject result;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        result = Compile(env, cls, context, compiler, options);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

// public static native void DeleteShaderContext(long context);
JNIEXPORT void JNICALL Java_ShadercJni_DeleteShaderContext(JNIEnv* env, jclass cls, jlong context)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::DeleteShaderContext((dmShaderc::HShaderContext) context);
    }
    DM_JNI_GUARD_SCOPE_END();
}

// public static native Shaderc.ShaderContext NewShaderContext(int stage, byte[] buffer);
JNIEXPORT jlong JNICALL Java_ShadercJni_NewShaderContext(JNIEnv* env, jclass cls, jint stage, jbyteArray array)
{
    jlong context;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        context = NewShaderContext(env, cls, stage, array);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return context;
}


// public static native Shaderc.ShaderCompiler NewShaderCompiler(Shaderc.ShaderContext context, int language);
JNIEXPORT jlong JNICALL Java_ShadercJni_NewShaderCompiler(JNIEnv* env, jclass cls, jlong context, jint language)
{
    jlong compiler;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        compiler = NewShaderCompiler(env, cls, context, language);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return compiler;
}

// public static native void DeleteShaderCompiler(long compiler);
JNIEXPORT void JNICALL Java_ShadercJni_DeleteShaderCompiler(JNIEnv* env, jclass cls, jlong compiler)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::DeleteShaderCompiler((dmShaderc::HShaderCompiler) compiler);
    }
    DM_JNI_GUARD_SCOPE_END();
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

// void SetResourceLocation(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t location);
JNIEXPORT void JNICALL Java_ShadercJni_SetResourceLocation(JNIEnv* env, jclass cls, jlong context, jlong compiler, jlong name_hash, jint location)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::SetResourceLocation((dmShaderc::HShaderContext) context, (dmShaderc::HShaderCompiler) compiler, (uint64_t) name_hash, (uint8_t) location);
    }
    DM_JNI_GUARD_SCOPE_END();
}

// void SetResourceBinding(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t binding);
JNIEXPORT void JNICALL Java_ShadercJni_SetResourceBinding(JNIEnv* env, jclass cls, jlong context, jlong compiler, jlong name_hash, jint binding)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::SetResourceBinding((dmShaderc::HShaderContext) context, (dmShaderc::HShaderCompiler) compiler, (uint64_t) name_hash, (uint8_t) binding);
    }
    DM_JNI_GUARD_SCOPE_END();
}

// void SetResourceSet(HShaderContext context, HShaderCompiler compiler, uint64_t name_hash, uint8_t set);
JNIEXPORT void JNICALL Java_ShadercJni_SetResourceSet(JNIEnv* env, jclass cls, jlong context, jlong compiler, jlong name_hash, jint set)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::SetResourceSet((dmShaderc::HShaderContext) context, (dmShaderc::HShaderCompiler) compiler, (uint64_t) name_hash, (uint8_t) set);
    }
    DM_JNI_GUARD_SCOPE_END();
}

// void SetResourceStageFlags(HShaderContext context, uint64_t name_hash, uint8_t stage_flags);
JNIEXPORT void JNICALL Java_ShadercJni_SetResourceStageFlags(JNIEnv* env, jclass cls, jlong context, jlong name_hash, jint stage_flags)
{
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmShaderc::SetResourceStageFlags((dmShaderc::HShaderContext) context, (uint64_t) name_hash, (uint8_t) stage_flags);
    }
    DM_JNI_GUARD_SCOPE_END();
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
    // Don't forget to add them to the corresponding java file (e.g. Shaderc.java)
    static const JNINativeMethod methods[] = {
        { (char*) "NewShaderContext", (char*) "(I[B)J", reinterpret_cast<void*>(Java_ShadercJni_NewShaderContext)},
        { (char*) "DeleteShaderContext", (char*) "(J)V", reinterpret_cast<void*>(Java_ShadercJni_DeleteShaderContext)},
        { (char*) "NewShaderCompiler", (char*) "(JI)J", reinterpret_cast<void*>(Java_ShadercJni_NewShaderCompiler)},
        { (char*) "DeleteShaderCompiler", (char*) "(J)V", reinterpret_cast<void*>(Java_ShadercJni_DeleteShaderCompiler)},
        { (char*) "Compile", (char*) "(JJL" CLASS_NAME "$ShaderCompilerOptions;)L" CLASS_NAME "$ShaderCompileResult;", reinterpret_cast<void*>(Java_ShadercJni_Compile)},
        { (char*) "GetReflection", (char*) "(J)L" CLASS_NAME "$ShaderReflection;", reinterpret_cast<void*>(Java_ShadercJni_GetReflection)},
        { (char*) "SetResourceStageFlags", (char*) "(JJI)V", reinterpret_cast<void*>(Java_ShadercJni_SetResourceStageFlags)},
        { (char*) "SetResourceLocation", (char*) "(JJJI)V", reinterpret_cast<void*>(Java_ShadercJni_SetResourceLocation)},
        { (char*) "SetResourceBinding", (char*) "(JJJI)V", reinterpret_cast<void*>(Java_ShadercJni_SetResourceBinding)},
        { (char*) "SetResourceSet", (char*) "(JJJI)V", reinterpret_cast<void*>(Java_ShadercJni_SetResourceSet)},
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
