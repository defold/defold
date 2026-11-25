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

#include <jni.h>
#include <jni/jni_util.h>
#include <ddf/ddf.h>
#include <render/material_render_state.h>

JNIEXPORT jbyteArray JNICALL BuildMaterialRenderState(JNIEnv* env, jclass cls, jbyteArray material_bytes, jbyteArray reflection_bytes)
{
    jbyteArray result = 0;
    DM_JNI_GUARD_SCOPE_BEGIN();
    {
        dmRenderDDF::MaterialDesc* material = 0;
        dmGraphics::ShaderDesc::ShaderReflection* reflection = 0;
        jbyte* material_data = env->GetByteArrayElements(material_bytes, 0);
        dmDDF::Result material_result = dmDDF::LoadMessage(material_data, env->GetArrayLength(material_bytes), &material);
        env->ReleaseByteArrayElements(material_bytes, material_data, JNI_ABORT);
        jbyte* reflection_data = env->GetByteArrayElements(reflection_bytes, 0);
        dmDDF::Result reflection_result = dmDDF::LoadMessage(reflection_data, env->GetArrayLength(reflection_bytes), &reflection);
        env->ReleaseByteArrayElements(reflection_bytes, reflection_data, JNI_ABORT);
        if (material_result == dmDDF::RESULT_OK && reflection_result == dmDDF::RESULT_OK)
        {
            dmRender::MaterialRenderState state;
            dmRender::BuildMaterialRenderState(*material, *reflection, &state);
            // Serialize a borrowed view without changing ownership of the input DDF.
            dmRenderDDF::MaterialDesc compiled = *material;
            dmRenderDDF::MaterialRenderState state_ddf = dmRender::GetMaterialRenderStateDDF(&state);
            compiled.m_RenderState = &state_ddf;
            compiled.m_RenderHash = dmRender::HashMaterialRenderState(state);
            dmArray<uint8_t> bytes;
            if (dmDDF::SaveMessageToArray(&compiled, dmRenderDDF::MaterialDesc::m_DDFDescriptor, bytes) == dmDDF::RESULT_OK)
            {
                result = env->NewByteArray(bytes.Size());
                if (result)
                    env->SetByteArrayRegion(result, 0, bytes.Size(), (const jbyte*)bytes.Begin());
            }
        }
        if (material)
            dmDDF::FreeMessage(material);
        if (reflection)
            dmDDF::FreeMessage(reflection);
    }
    DM_JNI_GUARD_SCOPE_END(return 0;);
    return result;
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK)
        return JNI_ERR;
    jclass cls = env->FindClass("com/dynamo/bob/pipeline/RenderJni");
    if (!cls)
        return JNI_ERR;
    static const JNINativeMethod methods[] = {
        { (char*) "BuildMaterialRenderState", (char*) "([B[B)[B", reinterpret_cast<void*>(BuildMaterialRenderState) },
    };
    int result = env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    env->DeleteLocalRef(cls);
    return result == 0 ? JNI_VERSION_1_8 : JNI_ERR;
}
