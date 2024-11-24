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

#include <jni.h> // JDK
#include <jni/jni_util.h> // defold

#include <jni/Texc_jni.h>

#include <dlib/log.h>

#include <texc.h>

JNIEXPORT jlong JNICALL Java_TexcLibraryJni_CreateTexture(JNIEnv* env, jclass cls, jstring _path, jint width, jint height, jint pixelFormat, jint colorSpace, jint compressionType, jbyteArray array)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    if (!array)
    {
        dmLogError("%s: Texture data array was null!", __FUNCTION__);
        return 0;
    }

    jlong obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        //dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmJNI::ScopedString j_path(env, _path);
        const char* path = strdup(j_path.m_String ? j_path.m_String : "null");

        dmJNI::ScopedByteArray j_array(env, array);

        dmTexc::Texture* texture = dmTexc::CreateTexture(path, width, height, (dmTexc::PixelFormat)pixelFormat, (dmTexc::ColorSpace)colorSpace, (dmTexc::CompressionType)compressionType, j_array.m_Array);
        obj = (jlong)texture;

    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT void JNICALL Java_TexcLibraryJni_DestroyTexture(JNIEnv* env, jclass cls, jlong texture)
{
    dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::DestroyTexture((dmTexc::Texture*)texture);
    DM_JNI_GUARD_SCOPE_END(return;);
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_GetHeader(JNIEnv* env, jclass cls, jlong texture)
{
    //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            dmTexc::Header header;
            if (dmTexc::GetHeader(ptexture, &header))
            {
                obj = C2J_CreateHeader(env, types, &header);
            }
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetDataSizeCompressed(JNIEnv* env, jclass cls, jlong texture, jint mip_map)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetDataSizeCompressed(ptexture, mip_map);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetDataSizeUncompressed(JNIEnv* env, jclass cls, jlong texture, jint mip_map)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetDataSizeUncompressed(ptexture, mip_map);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jint JNICALL Java_TexcLibraryJni_GetCompressionFlags(JNIEnv* env, jclass cls, jlong texture)
{
    jint result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        //dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GetCompressionFlags(ptexture);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jobject JNICALL Java_TexcLibraryJni_GetData(JNIEnv* env, jclass cls, jlong texture)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);

        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            uint32_t data_count = dmTexc::GetTotalDataSize(ptexture);

            //obj = dmJNI::C2J_CreateByteArray(env, );

            jbyteArray arr = env->NewByteArray(data_count);
            jbyte* data = env->GetByteArrayElements(arr, 0);
            dmTexc::GetData(ptexture, data, data_count);
            env->ReleaseByteArrayElements(arr, data, 0);

            obj = arr;
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return obj;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_Resize(JNIEnv* env, jclass cls, jlong texture, jint width, jint height)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::Resize(ptexture, width, height);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_PreMultiplyAlpha(JNIEnv* env, jclass cls, jlong texture)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::PreMultiplyAlpha(ptexture);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_GenMipMaps(JNIEnv* env, jclass cls, jlong texture)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::GenMipMaps(ptexture);
        }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jboolean JNICALL Java_TexcLibraryJni_Flip(JNIEnv* env, jclass cls, jlong texture, jint flip_axis)
{
    jboolean result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::Texture* ptexture = (dmTexc::Texture*)texture;
        if (ptexture)
        {
            result = dmTexc::Flip(ptexture, (dmTexc::FlipAxis)flip_axis);
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

// JNIEXPORT jobject JNICALL Java_TexcLibraryJni_CreateImage(JNIEnv* env, jclass cls, jstring path, jbyteArray bytes, jint width, jint height, jint depth, jint numChannels)
// {
//     dmLogDebug("%s: env = %p\n", __FUNCTION__, env);
//     //DM_SCOPED_SIGNAL_CONTEXT(env, return 0;);

//     jobject jimage;
//     DM_JNI_GUARD_SCOPE_BEGIN();

//         dmTexc::jni::ScopedContext jni_scope(env);
//         dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;
//         //jimage = LoadFromBufferInternal(env, cls, _path, array, data_resolver);
//         jimage = dmTexc::jni::C2J_CreateImage(env, types, path, bytes, width, height, depth, numChannels);
//     DM_JNI_GUARD_SCOPE_END(return 0;);
//     return jimage;
// }


JNIEXPORT jobject JNICALL Java_TexcLibraryJni_BasisUEncode(JNIEnv* env, jclass cls, jobject settings)
{
    jobject obj = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
        dmTexc::jni::ScopedContext jni_scope(env);
        dmTexc::jni::TypeInfos* types = &jni_scope.m_TypeInfos;

        dmTexc::BasisUSettings input;
        bool result = J2C_CreateBasisUSettings(env, types, settings, &input);
        if (!result)
        {
            dmLogError("%s: J2C_CreateBasisUSettings. settings == %p", __FUNCTION__, settings);
            return 0;
        }

        // todo: support debug prints etc from commandline/environment variables
        // printf("EncodeBasisU:\n");
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
        // OLD API
        JNIFUNC(CreateTexture,          "(Ljava/lang/String;IIIII[B)J"),
        JNIFUNC(DestroyTexture,         "(J)V"),
        JNIFUNC(GetHeader,              "(J)L" CLASS_NAME "$Header;"),
        JNIFUNC(GetDataSizeCompressed,  "(JI)I"),
        JNIFUNC(GetDataSizeUncompressed,"(JI)I"),
        JNIFUNC(GetData,                "(J)[B"),
        JNIFUNC(GetCompressionFlags,    "(J)I"),

        JNIFUNC(Resize,                 "(JII)Z"),
        JNIFUNC(PreMultiplyAlpha,       "(J)Z"),
        JNIFUNC(GenMipMaps,             "(J)Z"),
        JNIFUNC(Flip,                   "(JI)Z"),

        // Font glyph buffers
        JNIFUNC(CompressBuffer,         "([B)L" CLASS_NAME "$Buffer;"),

        // Compressor api
        JNIFUNC(BasisUEncode,           "(L" CLASS_NAME "$BasisUSettings;)[B"),
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
