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

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include <jni/Texc_jni.h>

#include <stdlib.h>

#include <dlib/log.h>

#include <texc.h>

JNIEXPORT jlong JNICALL Java_TexcLibraryJni_CreateImage(JNIEnv* env, jclass cls, jstring _path, jint width, jint height, jint pixelFormat, jint colorSpace, jbyteArray array)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    if (!array)
    {
        dmLogError("%s: Image data array was null!", __FUNCTION__);
        return 0;
    }

    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        //dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmJNI::ScopedString j_path(env, _path);
        const char* path = strdup(j_path.m_String ? j_path.m_String : "null");

        dmJNI::ScopedByteArrayCritical j_array(env, array);

        dmTexc::Image* image = dmTexc::CreateImage(path, width, height, (dmTexc::PixelFormat)pixelFormat, (dmTexc::ColorSpace)colorSpace, j_array.m_ArraySize, (uint8_t*)j_array.m_Array);
        obj = (jlong)image;

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_CreatePreviewImage(JNIEnv* env, jclass cls, jint width, jint height, jbyteArray inputArray, jbyteArray outputArray)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    // DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    if (!inputArray)
    {
        dmLogError("%s: Image data array was null!", __FUNCTION__);
        return 0;
    }

    DM_JNI_GUARD_SCOPE_BEGIN();

    dmTexc::jni::ScopedContext     jni_scope(env);

    dmJNI::ScopedByteArrayCritical j_input_array(env, inputArray);
    dmJNI::ScopedByteArrayCritical j_output_array(env, outputArray);

    dmTexc::CreatePreviewImage(width, height, j_input_array.m_ArraySize, (uint8_t*)j_input_array.m_Array, (uint8_t*)j_output_array.m_Array);

    DM_JNI_GUARD_SCOPE_END(return -1;);
    return 0;
}

JNIEXPORT void JNICALL Java_TexcLibraryJni_DestroyImage(JNIEnv* env, jclass cls, jlong _image)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::DestroyImage((dmTexc::Image*)_image);
    DM_JNI_GUARD_SCOPE_END(return;);
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetWidth(JNIEnv* env, jclass cls, jlong _image)
{
    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            obj = (jlong)dmTexc::GetWidth(image);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetHeight(JNIEnv* env, jclass cls, jlong _image)
{
    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            obj = (jlong)dmTexc::GetHeight(image);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}


JNIEXPORT jobject JNICALL Java_TexcLibraryJni_GetData(JNIEnv* env, jclass cls, jlong _image)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            obj = dmJNI::C2J_CreateUByteArray(env, image->m_Data, image->m_DataCount);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jlong JNICALL Java_TexcLibraryJni_Resize(JNIEnv* env, jclass cls, jlong _image, jint width, jint height)
{
    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            obj = (jlong)dmTexc::Resize(image, width, height);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_PreMultiplyAlpha(JNIEnv* env, jclass cls, jlong _image)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            result = dmTexc::PreMultiplyAlpha(image);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_Flip(JNIEnv* env, jclass cls, jlong _image, jint flip_axis)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            result = dmTexc::Flip(image, (dmTexc::FlipAxis)flip_axis);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_Dither(JNIEnv* env, jclass cls, jlong _image, jint pixel_format)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Image* image = (dmTexc::Image*)_image;
        if (image)
        {
            result = dmTexc::Dither(image, (dmTexc::PixelFormat)pixel_format);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

// *************************************************************************************************************
// FONTS

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_CompressBuffer(JNIEnv* env, jclass cls, jbyteArray array)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmJNI::ScopedByteArray j_array(env, array);

        dmTexc::Buffer* buffer = dmTexc::CompressBuffer((uint8_t*)j_array.m_Array, (uint32_t)j_array.m_ArraySize);
        if (buffer)
        {
            obj = C2J_CreateBuffer(env, types, buffer);
            dmTexc::DestroyBuffer(buffer);
        }

        //env->ReleaseByteArrayElements(array, array_data, JNI_ABORT);
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

// *************************************************************************************************************

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_BasisUEncode(JNIEnv* env, jclass cls, jobject settings)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::BasisUEncodeSettings input;
        bool result = J2C_CreateBasisUEncodeSettings(env, types, settings, &input);
        if (!result)
        {
            dmLogError("%s: J2C_CreateBasisUEncodeSettings. settings == %p", __FUNCTION__, settings);
            return 0;
        }

        // todo: support debug prints etc from commandline/environment variables
        // printf("BasisUEncode:\n");
        // printf("    path: %s\n",  input.m_Path?input.m_Path:"null");
        // printf("   width: %d\n", input.m_Width);
        // printf("  height: %d\n", input.m_Height);
        // printf("      pf: %d\n", input.m_PixelFormat);
        // printf("      cs: %d\n", input.m_ColorSpace);
        // printf("    data: %p\n", input.m_Data);
        // printf("      sz: %u\n", input.m_DataCount);

        uint8_t* out = 0;
        uint32_t out_size = 0;
        result = dmTexc::BasisUEncode(&input, &out, &out_size);
        if (!result)
        {
            dmLogError("%s: dmTexc::BasisUEncode failed", __FUNCTION__);
            return 0;
        }

        obj = dmJNI::C2J_CreateUByteArray(env, out, out_size);

        if (out)
            free(out);

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_ASTCEncode(JNIEnv* env, jclass cls, jobject settings)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::ASTCEncodeSettings input;
        bool result = J2C_CreateASTCEncodeSettings(env, types, settings, &input);
        if (!result)
        {
            dmLogError("%s: J2C_CreateASTCEncodeSettings. settings == %p", __FUNCTION__, settings);
            return 0;
        }

        uint8_t* out = 0;
        uint32_t out_size = 0;
        result = dmTexc::ASTCEncode(&input, &out, &out_size);
        if (!result)
        {
            dmLogError("%s: dmTexc::ASTCEncode failed", __FUNCTION__);
            return 0;
        }

        obj = dmJNI::C2J_CreateUByteArray(env, out, out_size);

        if (out)
            free(out);

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_DefaultEncode(JNIEnv* env, jclass cls, jobject settings)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::DefaultEncodeSettings input;
        bool result = J2C_CreateDefaultEncodeSettings(env, types, settings, &input);
        if (!result)
        {
            dmLogError("%s: J2C_CreateDefaultEncodeSettings. settings == %p", __FUNCTION__, settings);
            return 0;
        }

        // todo: support debug prints etc from commandline/environment variables
        // printf("DefaultEncode:\n");
        // printf("    path: %s\n",  input.m_Path?input.m_Path:"null");
        // printf("   width: %d\n", input.m_Width);
        // printf("  height: %d\n", input.m_Height);
        // printf("      pf: %d\n", input.m_PixelFormat);
        // printf("      cs: %d\n", input.m_ColorSpace);
        // printf("    data: %p\n", input.m_Data);
        // printf("      sz: %u\n", input.m_DataCount);

        uint8_t* out = 0;
        uint32_t out_size = 0;
        result = dmTexc::DefaultEncode(&input, &out, &out_size);
        if (!result)
        {
            dmLogError("%s: dmTexc::BasisUEncode failed", __FUNCTION__);
            return 0;
        }

        obj = dmJNI::C2J_CreateUByteArray(env, out, out_size);

        if (out)
            free(out);

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}



JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    dmLogDebug("JNI_OnLoad ->\n");
    //dmJNI::EnableDefaultSignalHandlers(vm);

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        printf("JNI_OnLoad GetEnv error\n");
        return JNI_ERR;
    }

    // Find your class. JNI_OnLoad is called from the correct class loader context for this to work.
    jclass c = env->FindClass( JAVA_PACKAGE_NAME "/TexcLibraryJni");
    dmLogDebug("JNI_OnLoad: c = %p\n", c);
    if (c == 0)
        return JNI_ERR;

    #define JNIFUNC(NAME, SIGNATURE) {(char*) #NAME, (char*)(SIGNATURE), reinterpret_cast<void*>(Java_TexcLibraryJni_ ## NAME )}

    // Register your class' native methods.
    // Don't forget to add them to the corresponding java file (e.g. ModelImporter.java)
    static const JNINativeMethod methods[] = {
        // Image api
        JNIFUNC(CreateImage, "(Ljava/lang/String;IIII[B)J"),
        JNIFUNC(CreatePreviewImage, "(II[B[B)I"),
        JNIFUNC(DestroyImage, "(J)V"),
        JNIFUNC(GetWidth, "(J)I"),
        JNIFUNC(GetHeight, "(J)I"),
        JNIFUNC(GetData, "(J)[B"),
        JNIFUNC(Resize, "(JII)J"),
        JNIFUNC(PreMultiplyAlpha, "(J)Z"),
        JNIFUNC(Flip, "(JI)Z"),
        JNIFUNC(Dither, "(JI)Z"),

        // Font glyph buffers
        JNIFUNC(CompressBuffer,         "([B)L" CLASS_NAME "$Buffer;"),

        // Compressor api
        JNIFUNC(BasisUEncode,           "(L" CLASS_NAME "$BasisUEncodeSettings;)[B"),
        JNIFUNC(ASTCEncode,             "(L" CLASS_NAME "$ASTCEncodeSettings;)[B"),
        JNIFUNC(DefaultEncode,          "(L" CLASS_NAME "$DefaultEncodeSettings;)[B"),
    };
    #undef JNIFUNC

    int rc = env->RegisterNatives(c, methods, sizeof(methods)/sizeof(JNINativeMethod));
    env->DeleteLocalRef(c);

    if (rc != JNI_OK) return rc;

    dmLogDebug("JNI_OnLoad return.\n");
    return JNI_VERSION_1_8;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    dmLogDebug("JNI_OnUnload ->\n");
}
