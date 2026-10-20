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

#include "gamesys_jni.h"
#include "Geometry_jni.h"
#include "../geometry.h"

namespace dmGameSystem
{
    static jfloatArray JNICALL GeneratePositions(JNIEnv* env, jclass cls, jobject params)
    {
        jni::ScopedContext context(env);
        Slice9Params native_params = {};
        jni::J2C_CreateSlice9Params(env, &context.m_TypeInfos, params, &native_params);
        if (env->ExceptionCheck())
            return 0;

        float positions[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9Positions(native_params, positions);
        jfloatArray result = env->NewFloatArray(SLICE9_VERTEX_COUNT * 2);
        if (result)
            env->SetFloatArrayRegion(result, 0, SLICE9_VERTEX_COUNT * 2, positions);
        return result;
    }

    static jfloatArray JNICALL GenerateTexCoords(JNIEnv* env, jclass cls, jobject params,
                                                 jfloat texture_width, jfloat texture_height, jfloatArray tex_coords)
    {
        jni::ScopedContext context(env);
        Slice9Params native_params = {};
        jni::J2C_CreateSlice9Params(env, &context.m_TypeInfos, params, &native_params);
        if (env->ExceptionCheck())
            return 0;

        float corners[8];
        env->GetFloatArrayRegion(tex_coords, 0, 8, corners);
        if (env->ExceptionCheck())
            return 0;

        float uvs[SLICE9_VERTEX_COUNT * 2];
        GenerateSlice9TexCoords(native_params, texture_width, texture_height, corners, uvs);
        jfloatArray result = env->NewFloatArray(SLICE9_VERTEX_COUNT * 2);
        if (result)
            env->SetFloatArrayRegion(result, 0, SLICE9_VERTEX_COUNT * 2, uvs);
        return result;
    }

    static jintArray JNICALL GetIndices(JNIEnv* env, jclass cls)
    {
        const uint16_t* indices = GetSlice9Indices();
        jint values[SLICE9_INDEX_COUNT];
        for (uint32_t i = 0; i < SLICE9_INDEX_COUNT; ++i)
            values[i] = indices[i];
        jintArray result = env->NewIntArray(SLICE9_INDEX_COUNT);
        if (result)
            env->SetIntArrayRegion(result, 0, SLICE9_INDEX_COUNT, values);
        return result;
    }

    jint RegisterGeometryNatives(JNIEnv* env)
    {
        jclass cls = env->FindClass("com/defold/engine/GeometryJni");
        if (!cls)
            return JNI_ERR;
        const JNINativeMethod methods[] = {
            {(char*)"GenerateSlice9PositionsInternal", (char*)"(Lcom/defold/engine/Geometry$Slice9Params;)[F", (void*)GeneratePositions},
            {(char*)"GenerateSlice9TexCoordsInternal", (char*)"(Lcom/defold/engine/Geometry$Slice9Params;FF[F)[F", (void*)GenerateTexCoords},
            {(char*)"GetSlice9IndicesInternal", (char*)"()[I", (void*)GetIndices},
        };
        jint result = env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
        env->DeleteLocalRef(cls);
        return result;
    }
}
