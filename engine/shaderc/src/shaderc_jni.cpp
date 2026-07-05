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
#include "shaderc_private.h"

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include "jni/Shaderc_jni.h"

#include <dlib/array.h>
#include <dlib/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static jfieldID FindFieldIdSafe(JNIEnv* env, jclass cls, const char* field_name, const char* signature, bool required)
{
    jfieldID field = env->GetFieldID(cls, field_name, signature);
    if (!field)
    {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        if (required)
            dmLogError("Missing required ShaderCompilerOptions field '%s' with signature '%s'", field_name, signature);
    }
    return field;
}

static bool ReadIntFieldSafe(JNIEnv* env, jobject obj, jclass cls, const char* field_name, int32_t* out_value, bool required, int32_t default_value)
{
    *out_value = default_value;
    jfieldID field = FindFieldIdSafe(env, cls, field_name, "I", required);
    if (!field)
        return !required;
    *out_value = env->GetIntField(obj, field);
    return true;
}

static bool ReadByteFieldSafe(JNIEnv* env, jobject obj, jclass cls, const char* field_name, uint8_t* out_value, bool required, uint8_t default_value)
{
    *out_value = default_value;
    jfieldID field = FindFieldIdSafe(env, cls, field_name, "B", required);
    if (!field)
        return !required;
    *out_value = (uint8_t) env->GetByteField(obj, field);
    return true;
}

static bool ReadEnumFieldSafe(JNIEnv* env, jobject obj, jclass cls, const char* field_name, const char* signature,
                              int32_t* out_value, bool required, int32_t default_value)
{
    *out_value = default_value;
    jfieldID field = FindFieldIdSafe(env, cls, field_name, signature, required);
    if (!field)
        return !required;

    jobject value_obj = env->GetObjectField(obj, field);
    if (!value_obj)
    {
        if (required)
            dmLogError("Required ShaderCompilerOptions enum field '%s' was null", field_name);
        return !required;
    }
    env->DeleteLocalRef(value_obj);

    *out_value = dmJNI::GetEnum(env, obj, field);
    return true;
}

static bool ReadStringFieldSafe(JNIEnv* env, jobject obj, jclass cls, const char* field_name, char** out_value, bool required)
{
    *out_value = 0;
    jfieldID field = FindFieldIdSafe(env, cls, field_name, "Ljava/lang/String;", required);
    if (!field)
        return !required;

    jobject value_obj = env->GetObjectField(obj, field);
    if (!value_obj)
    {
        if (required)
            dmLogError("Required ShaderCompilerOptions string field '%s' was null", field_name);
        return !required;
    }

    jclass string_class = env->FindClass("java/lang/String");
    if (!string_class)
    {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        env->DeleteLocalRef(value_obj);
        dmLogError("Failed to resolve java/lang/String while reading ShaderCompilerOptions.%s", field_name);
        return false;
    }

    bool is_string = env->IsInstanceOf(value_obj, string_class) == JNI_TRUE;
    env->DeleteLocalRef(string_class);
    if (!is_string)
    {
        env->DeleteLocalRef(value_obj);
        dmLogError("ShaderCompilerOptions.%s is not a java.lang.String", field_name);
        return false;
    }

    const char* utf = env->GetStringUTFChars((jstring) value_obj, 0);
    if (!utf)
    {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        env->DeleteLocalRef(value_obj);
        dmLogError("Failed to read UTF chars from ShaderCompilerOptions.%s", field_name);
        return false;
    }

    *out_value = strdup(utf);
    env->ReleaseStringUTFChars((jstring) value_obj, utf);
    env->DeleteLocalRef(value_obj);

    if (!(*out_value))
    {
        dmLogError("Out of memory duplicating ShaderCompilerOptions.%s", field_name);
        return false;
    }

    return true;
}

static bool ReadShaderCompilerOptionsSafe(JNIEnv* env, jobject options_obj, dmShaderc::ShaderCompilerOptions* out_options)
{
    if (!out_options || !options_obj)
    {
        dmLogError("Invalid arguments while reading ShaderCompilerOptions from JNI");
        return false;
    }

    *out_options = dmShaderc::ShaderCompilerOptions();

    jclass options_cls = env->GetObjectClass(options_obj);
    if (!options_cls)
    {
        dmLogError("Failed to get ShaderCompilerOptions class");
        return false;
    }

    bool ok = true;
    int32_t version = 0;
    uint8_t remove_unused = 0;
    uint8_t no_420_pack = 0;
    uint8_t glsl_emit_ubo = 0;
    uint8_t glsl_es = 0;
    uint8_t hlsl_move_sv_position_to_front = 0;
    int32_t glsl_es_default_float_precision = out_options->m_GlslEsDefaultFloatPrecision;
    int32_t glsl_es_default_int_precision = out_options->m_GlslEsDefaultIntPrecision;
    int32_t target_platform = out_options->m_TargetPlatform;

    ok = ok && ReadIntFieldSafe(env, options_obj, options_cls, "version", &version, true, 0);
    ok = ok && ReadEnumFieldSafe(env, options_obj, options_cls, "glslEsDefaultFloatPrecision", "L" CLASS_NAME "$ShaderPrecision;",
                                 &glsl_es_default_float_precision, false, glsl_es_default_float_precision);
    ok = ok && ReadEnumFieldSafe(env, options_obj, options_cls, "glslEsDefaultIntPrecision", "L" CLASS_NAME "$ShaderPrecision;",
                                 &glsl_es_default_int_precision, false, glsl_es_default_int_precision);
    ok = ok && ReadEnumFieldSafe(env, options_obj, options_cls, "targetPlatform", "L" CLASS_NAME "$ShaderCompilerPlatform;",
                                 &target_platform, false, target_platform);
    ok = ok && ReadByteFieldSafe(env, options_obj, options_cls, "removeUnusedVariables", &remove_unused, false, 0);
    ok = ok && ReadByteFieldSafe(env, options_obj, options_cls, "no420PackExtension", &no_420_pack, false, 0);
    ok = ok && ReadByteFieldSafe(env, options_obj, options_cls, "glslEmitUboAsPlainUniforms", &glsl_emit_ubo, false, 0);
    ok = ok && ReadByteFieldSafe(env, options_obj, options_cls, "glslEs", &glsl_es, false, 0);
    ok = ok && ReadByteFieldSafe(env, options_obj, options_cls, "hLSLMoveSVPositionToFront", &hlsl_move_sv_position_to_front, false, 0);
    ok = ok && ReadStringFieldSafe(env, options_obj, options_cls, "entryPoint", (char**) &out_options->m_EntryPoint, false);
    ok = ok && ReadStringFieldSafe(env, options_obj, options_cls, "externalCompilerPath", (char**) &out_options->m_ExternalCompilerPath, false);
    ok = ok && ReadStringFieldSafe(env, options_obj, options_cls, "externalCompilerArgs", (char**) &out_options->m_ExternalCompilerArgs, false);
    ok = ok && ReadStringFieldSafe(env, options_obj, options_cls, "rootSignatureOverride", (char**) &out_options->m_RootSignatureOverride, false);

    out_options->m_Version = (uint32_t) version;
    out_options->m_GlslEsDefaultFloatPrecision = (dmShaderc::ShaderPrecision) glsl_es_default_float_precision;
    out_options->m_GlslEsDefaultIntPrecision = (dmShaderc::ShaderPrecision) glsl_es_default_int_precision;
    out_options->m_TargetPlatform = (dmShaderc::ShaderCompilerPlatform) target_platform;
    out_options->m_RemoveUnusedVariables = remove_unused;
    out_options->m_No420PackExtension = no_420_pack;
    out_options->m_GlslEmitUboAsPlainUniforms = glsl_emit_ubo;
    out_options->m_GlslEs = glsl_es;
    out_options->m_HLSLMoveSVPositionToFront = hlsl_move_sv_position_to_front;

    env->DeleteLocalRef(options_cls);
    return ok;
}

static void FreeShaderCompilerOptionsStrings(dmShaderc::ShaderCompilerOptions* options)
{
    if (!options)
        return;

    free((void*) options->m_EntryPoint);
    free((void*) options->m_ExternalCompilerPath);
    free((void*) options->m_ExternalCompilerArgs);
    free((void*) options->m_RootSignatureOverride);

    options->m_EntryPoint = 0;
    options->m_ExternalCompilerPath = 0;
    options->m_ExternalCompilerArgs = 0;
    options->m_RootSignatureOverride = 0;
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
    if (!ReadShaderCompilerOptionsSafe(env, options, &shader_options))
    {
        return 0;
    }

    dmShaderc::ShaderCompileResult* res = dmShaderc::Compile(shader_ctx, shader_compiler, shader_options);
    FreeShaderCompilerOptionsStrings(&shader_options);

    if (!res)
    {
        return 0;
    }

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

static jobject HLSLMergeRootSignatures(JNIEnv* env, jclass cls, jobjectArray shader_results)
{
    dmShaderc::jni::ScopedContext jni_scope(env);
    dmShaderc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

    uint32_t results_count = 0;
    dmShaderc::ShaderCompileResult* results = dmShaderc::jni::J2C_CreateShaderCompileResultArray(env, types, shader_results, &results_count);
    if (!results)
        return 0;

    dmShaderc::HLSLRootSignature* root_signature = dmShaderc::HLSLMergeRootSignatures(results, results_count);

    // Copy patched shader blobs/signatures back to the Java objects.
    for (uint32_t i = 0; i < results_count; ++i)
    {
        jobject obj = env->GetObjectArrayElement(shader_results, i);
        if (!obj)
            continue;

        dmJNI::SetObjectDeref(env, obj, types->m_ShaderCompileResultJNI.data, dmJNI::C2J_CreateUByteArray(env, results[i].m_Data.Begin(), results[i].m_Data.Size()));
        dmJNI::SetObjectDeref(env, obj, types->m_ShaderCompileResultJNI.hLSLRootSignature, dmJNI::C2J_CreateUByteArray(env, results[i].m_HLSLRootSignature.Begin(), results[i].m_HLSLRootSignature.Size()));
        env->DeleteLocalRef(obj);
    }

    jobject out = C2J_CreateHLSLRootSignature(env, types, root_signature);
    delete root_signature;
    delete[] results;
    return out;
}

JNIEXPORT jobject JNICALL Java_ShadercJni_HLSLMergeRootSignatures(JNIEnv* env, jclass cls, jobjectArray shader_results)
{
    jobject reflection;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        reflection = HLSLMergeRootSignatures(env, cls, shader_results);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return reflection;
}

JNIEXPORT jstring JNICALL Java_ShadercJni_HLSLRootSignatureToString(JNIEnv* env, jclass cls, jbyteArray root_signature_blob)
{
    jstring out_string = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
#if defined(_WIN32) && defined(DM_BINARY_HLSL_SUPPORTED)
        if (root_signature_blob != 0)
        {
            jsize blob_size = env->GetArrayLength(root_signature_blob);
            if (blob_size > 0)
            {
                jbyte* blob_data = env->GetByteArrayElements(root_signature_blob, 0);
                dmArray<char> root_signature_text;
                bool ok = dmShaderc::RootSignatureBlobToText(blob_data, (uint32_t) blob_size, root_signature_text);
                env->ReleaseByteArrayElements(root_signature_blob, blob_data, JNI_ABORT);

                if (ok && root_signature_text.Size() > 1)
                {
                    out_string = env->NewStringUTF(root_signature_text.Begin());
                }
            }
        }
#else
        (void) cls;
        (void) root_signature_blob;
#endif
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return out_string;
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
        { (char*) "HLSLMergeRootSignatures", (char*) "([L" CLASS_NAME "$ShaderCompileResult;)L" CLASS_NAME "$HLSLRootSignature;", reinterpret_cast<void*>(Java_ShadercJni_HLSLMergeRootSignatures)},
        { (char*) "HLSLRootSignatureToString", (char*) "([B)Ljava/lang/String;", reinterpret_cast<void*>(Java_ShadercJni_HLSLRootSignatureToString)},
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
