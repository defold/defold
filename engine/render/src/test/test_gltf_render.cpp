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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <testmain/testmain.h>
#include <ddf/ddf.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/time.h>
#include <platform/window.hpp>
#include <graphics/graphics.h>
#include <graphics/graphics_test_sanitizers.h>
#include <render/material_ddf.h>

using namespace dmGraphics;

extern "C" void dmExportedSymbols();

struct Vertex
{
    float m_Position[4];
    float m_Normal[3];
    float m_Tangent[4];
    float m_Color[4];
    float m_Uv[2];
    float m_Weights[4];
    float m_Bones[4];
};

static bool ReadFile(const char* path, dmArray<uint8_t>& bytes)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        dmLogError("Cannot read %s", path);
        return false;
    }
    fseek(file, 0, SEEK_END);
    uint32_t size = (uint32_t)ftell(file);
    rewind(file);
    bytes.SetCapacity(size);
    bytes.SetSize(size);
    bool success = fread(bytes.Begin(), 1, size, file) == size;
    fclose(file);
    return success;
}

static void SetVector(HContext context, HProgram program, const char* name, const dmVMath::Vector4& value)
{
    // A regression control may use an older shader without the new uniforms.
    HUniformLocation location = FindUniformLocation(program, name);
    if (location != INVALID_UNIFORM_LOCATION)
    {
        SetConstantV4(context, &value, 1, location);
    }
}

static void SetMatrix(HContext context, HProgram program, const char* name, const dmVMath::Matrix4& value)
{
    SetConstantM4(context, (const dmVMath::Vector4*)&value, 1, FindUniformLocation(program, name));
}

static void SetProbeTransform(HContext context, HProgram program, HVertexBuffer instance_buffer, const dmVMath::Matrix4& world, const dmVMath::Matrix4& normal)
{
    if (instance_buffer)
    {
        float data[29];
        memcpy(data, &world, sizeof(world));
        for (uint32_t column = 0; column < 3; ++column)
        {
            memcpy(data + 16 + column * 3, (const float*)&normal + column * 4, 3 * sizeof(float));
        }
        data[25] = 0;
        data[26] = 1;
        data[27] = 6;
        data[28] = 1;
        SetVertexBufferData(instance_buffer, GetVertexBufferSize(instance_buffer), data, BUFFER_USAGE_DYNAMIC_DRAW);
    }
    else
    {
        SetMatrix(context, program, "mtx_worldview", world);
        SetMatrix(context, program, "mtx_normal", normal);
    }
}

static void SaveCapture(const char* name, const uint8_t* pixels, uint32_t width, uint32_t height)
{
    const char* directory = getenv("DM_TEST_GLTF_CAPTURE_DIR");
    if (!directory)
    {
        return;
    }
    char path[2048];
    dmSnPrintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE* file = fopen(path, "wb");
    if (!file)
    {
        return;
    }
    fprintf(file, "P6\n%u %u\n255\n", width, height);
    for (uint32_t i = 0; i < width * height; ++i)
    {
        const uint8_t rgb[] = { pixels[i * 4 + 2], pixels[i * 4 + 1], pixels[i * 4] };
        fwrite(rgb, 1, sizeof(rgb), file);
    }
    fclose(file);
}


// Compare all four production shaders on a hidden render target. The first
// variant supplies reference pixels; the others must preserve its transforms,
// normal lighting, material factors and texture decoding with packed instances.
static bool RunProbes(HContext context, HProgram program, const char* name, bool skinned, bool instanced, uint8_t reference[6][4])
{
    uint32_t slots = 0;
    bool packed_normal = false;
    for (uint32_t i = 0; i < GetAttributeCount(program); ++i)
    {
        dmhash_t attribute_name;
        Type type;
        uint32_t elements, values;
        int32_t location;
        GetAttribute(program, i, &attribute_name, &type, &elements, &values, &location);
        uint32_t columns = type == TYPE_FLOAT_MAT4 ? 4 : type == TYPE_FLOAT_MAT3 ? 3 : 1;
        slots = dmMath::Max(slots, (uint32_t)location + columns * values);
        if (attribute_name == dmHashString64("mtx_normal"))
        {
            packed_normal = type == TYPE_FLOAT_MAT3;
        }
    }
    printf("%s vertex attribute slots: %u/16\n", name, slots);
    if (slots > 16 || (instanced && !packed_normal))
    {
        return false;
    }

    Vertex vertices[] = {
        { { -8, -8, -1, 1 }, { 0.3f, 0.4f, 0.8660254f }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
        { { 24, -8, -1, 1 }, { 0.3f, 0.4f, 0.8660254f }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
        { { -8, 24, -1, 1 }, { 0.3f, 0.4f, 0.8660254f }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 } },
    };
    HVertexBuffer buffer = NewVertexBuffer(context, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration(context);
    AddVertexStream(streams, "position", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "normal", 3, TYPE_FLOAT, false);
    AddVertexStream(streams, "tangent", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "color", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "texcoord0", 2, TYPE_FLOAT, false);
    AddVertexStream(streams, "bone_weights", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "bone_indices", 4, TYPE_FLOAT, false);
    HVertexDeclaration declaration = NewVertexDeclaration(context, streams, sizeof(Vertex));
    DeleteVertexStreamDeclaration(streams);

    HVertexBuffer instance_buffer = 0;
    HVertexDeclaration instance_declaration = 0;
    if (instanced)
    {
        instance_buffer = NewVertexBuffer(context, (25 + (skinned ? 4 : 0)) * sizeof(float), 0, BUFFER_USAGE_DYNAMIC_DRAW);
        streams = NewVertexStreamDeclaration(context, VERTEX_STEP_FUNCTION_INSTANCE);
        AddVertexStream(streams, "mtx_world", 16, TYPE_FLOAT, false);
        AddVertexStream(streams, "mtx_normal", 9, TYPE_FLOAT, false);
        if (skinned)
        {
            AddVertexStream(streams, "animation_data", 4, TYPE_FLOAT, false);
        }
        instance_declaration = NewVertexDeclaration(context, streams);
        DeleteVertexStreamDeclaration(streams);
    }

    const uint32_t size = 16;
    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferParams[0].m_Width = size;
    params.m_ColorBufferCreationParams[0].m_Height = params.m_ColorBufferParams[0].m_Height = size;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    params.m_SampleCount = 1;
    HRenderTarget target = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);

    TextureCreationParams creation;
    creation.m_Width = creation.m_Height = 1;
    HTexture texture = NewTexture(context, creation);
    const uint8_t texel[] = { 64, 128, 255, 255 };
    TextureParams upload;
    upload.m_Width = upload.m_Height = 1;
    upload.m_Format = TEXTURE_FORMAT_RGBA;
    upload.m_Data = texel;
    upload.m_DataSize = sizeof(texel);
    upload.m_MinFilter = upload.m_MagFilter = TEXTURE_FILTER_NEAREST;
    SetTexture(context, texture, upload);

    HTexture pose = 0;
    if (skinned)
    {
        creation.m_Width = 6;
        pose = NewTexture(context, creation);
    }

    BeginFrame(context);
    SetColorMask(context, true, true, true, true);
    SetRenderTarget(context, target, RenderTargetBindingParams());
    SetViewport(context, 0, 0, size, size);
    DisableState(context, STATE_BLEND);
    DisableState(context, STATE_DEPTH_TEST);
    DisableState(context, STATE_CULL_FACE);
    EnableProgram(context, program);
    EnableVertexBuffer(context, buffer, 0);
    EnableVertexDeclaration(context, declaration, 0, 0, program);
    if (instanced)
    {
        EnableVertexBuffer(context, instance_buffer, 1);
        EnableVertexDeclaration(context, instance_declaration, 1, 0, program);
    }
    uint32_t textures = 0;
    for (uint32_t i = 0; i < GetUniformCount(program); ++i)
    {
        Uniform uniform;
        GetUniform(program, i, &uniform);
        if (!IsTypeTextureType(uniform.m_Type))
        {
            continue;
        }
        SetSampler(context, uniform.m_Location, textures);
        EnableTexture(context, textures++, 0, strstr(uniform.m_Name, "pose_matrix_cache") ? pose : texture);
    }

    const dmVMath::Matrix4 identity = dmVMath::Matrix4::identity();
    SetMatrix(context, program, "mtx_view", identity);
    // Keep clip depth positive on Metal and OpenGL alike.
    SetMatrix(context, program, "mtx_proj", dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, -0.1f)));
    if (skinned && !instanced)
    {
        SetVector(context, program, "animation_data", dmVMath::Vector4(0, 1, 6, 1));
    }
    SetVector(context, program, "pbrCommonTextures", dmVMath::Vector4(0));
    SetVector(context, program, "pbrEmissiveFactorAndStrength", dmVMath::Vector4(0));
    SetVector(context, program, "pbrNormalScaleAndOcclusionStrength", dmVMath::Vector4(1, 1, 0, 0));
    SetVector(context, program, "lights.color", dmVMath::Vector4(1, 0.75f, 0.5f, 1));
    SetVector(context, program, "lights.params", dmVMath::Vector4(0, 1, 0, 0));

    const char* cases[] = { "unlit_factor", "base_color_texture", "ambient_lighting", "rotated_nonuniform", "bone_nonuniform", "reflected_scale" };
    bool success = true;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
    {
        dmVMath::Matrix4 world = identity;
        dmVMath::Vector3 bone_scale(1);
        if (i >= 3)
        {
            world = dmVMath::Matrix4::rotationZ(0.35f) * dmVMath::Matrix4::scale(dmVMath::Vector3(i == 5 ? -2 : 2, 3, 4));
        }
        if (i == 4)
        {
            bone_scale = dmVMath::Vector3(1.3f, 0.6f, 2);
        }
        if (skinned)
        {
            const float matrices[] = { bone_scale.getX(), 0, 0, 0, 0, bone_scale.getY(), 0, 0, 0, 0, bone_scale.getZ(), 0,
                                       bone_scale.getX(), 0, 0, 0, 0, bone_scale.getY(), 0, 0, 0, 0, bone_scale.getZ(), 0 };
            TextureParams pose_upload;
            pose_upload.m_Width = 6;
            pose_upload.m_Height = 1;
            pose_upload.m_Format = TEXTURE_FORMAT_RGBA32F;
            pose_upload.m_Data = matrices;
            pose_upload.m_DataSize = sizeof(matrices);
            pose_upload.m_MinFilter = pose_upload.m_MagFilter = TEXTURE_FILTER_NEAREST;
            SetTexture(context, pose, pose_upload);
        }
        else
        {
            world = world * dmVMath::Matrix4::scale(bone_scale);
        }
        SetProbeTransform(context, program, instance_buffer, world, transpose(inverse(world)));
        bool unlit = i < 2;
        float factor = i == 0 ? 0.5f : i == 1 ? 1.0f : 0.5f;
        SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(factor, factor, factor, 1));
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0, 1, 0, 0));
        SetVector(context, program, "pbrMetallicRoughness.metallicRoughnessTextures", dmVMath::Vector4(i == 1 ? 1 : 0, 0, 0, 0));
        SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, unlit ? 1 : 0, 0));
        SetVector(context, program, "light_info", dmVMath::Vector4(i == 2 ? 0.5f : 0, i == 2 ? 0.5f : 0, i == 2 ? 0.5f : 0, i >= 3 ? 1 : 0));
        SetVector(context, program, "lights.direction_range", dmVMath::Vector4(0, 0, i == 5 ? 1 : -1, 0));
        Clear(context, BUFFER_TYPE_COLOR0_BIT, 255, 0, 255, 0, 1, 0);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        uint8_t pixels[size * size * 4];
        ReadPixels(context, 0, 0, size, size, pixels, sizeof(pixels));
        const uint8_t* pixel = pixels + ((size / 2) * size + size / 2) * 4;
        bool passed = pixel[3] == 255;
        if (!skinned && !instanced)
        {
            memcpy(reference[i], pixel, 4);
            const uint8_t expected[][3] = { { 188, 188, 188 }, { 64, 128, 255 }, { 139, 139, 139 } };
            if (i < 3)
            {
                for (uint32_t c = 0; c < 3; ++c)
                {
                    passed &= abs((int)pixel[2 - c] - expected[i][c]) <= 1;
                }
            }
            else
            {
                passed &= pixel[0] > 0 && pixel[1] > 0 && pixel[2] > 0;
            }
        }
        else
        {
            for (uint32_t c = 0; c < 4; ++c)
            {
                passed &= abs((int)pixel[c] - reference[i][c]) <= 1;
            }
        }
        char capture_name[128];
        dmSnPrintf(capture_name, sizeof(capture_name), "%s_%s", name, cases[i]);
        SaveCapture(capture_name, pixels, size, size);
        printf("%s: %s (%u,%u,%u,%u)\n", capture_name, passed ? "PASS" : "FAIL", pixel[2], pixel[1], pixel[0], pixel[3]);
        success &= passed;
    }

    for (uint32_t i = 0; i < textures; ++i)
    {
        DisableTexture(context, i, 0);
    }
    if (instanced)
    {
        DisableVertexDeclaration(context, instance_declaration);
        DisableVertexBuffer(context, instance_buffer);
    }
    DisableVertexDeclaration(context, declaration);
    DisableVertexBuffer(context, buffer);
    DisableProgram(context);
    SetRenderTarget(context, 0, RenderTargetBindingParams());
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    Flip(context);
    if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL)
    {
        // Drain the hidden drawable before deleting Metal's context.
        uint8_t drained[4];
        ReadPixels(context, 0, 0, 1, 1, drained, sizeof(drained));
    }
    DeleteRenderTarget(context, target);
    DeleteTexture(context, texture);
    if (pose)
    {
        DeleteTexture(context, pose);
    }
    if (instanced)
    {
        DeleteVertexDeclaration(instance_declaration);
        DeleteVertexBuffer(instance_buffer);
    }
    DeleteVertexDeclaration(declaration);
    DeleteVertexBuffer(buffer);
    return success;
}

int main(int argc, char** argv)
{
    if (argc != 1 && (argc != 3 || strcmp(argv[1], "--backend") != 0))
    {
        printf("Usage: %s [--backend metal|opengl|opengles|vulkan]\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, 0, _IONBF, 0);
    TestMainPlatformInit();
    dmExportedSymbols();
    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);
    atexit(dmLog::LogFinalize);
    dmHashEnableReverseHash(true);
    const char* backend = argc == 1 ? DM_TEST_GLTF_BACKEND : argv[2];
    AdapterFamily family;
    if (strcmp(backend, "metal") == 0)
    {
        family = ADAPTER_FAMILY_METAL;
    }
    else if (strcmp(backend, "opengl") == 0)
    {
        family = ADAPTER_FAMILY_OPENGL;
    }
    else if (strcmp(backend, "opengles") == 0)
    {
        family = ADAPTER_FAMILY_OPENGLES;
    }
    else if (strcmp(backend, "vulkan") == 0)
    {
        family = ADAPTER_FAMILY_VULKAN;
    }
    else
    {
        dmLogError("Unknown graphics backend: %s", backend);
        return 1;
    }
    if (!InstallAdapter(family) || GetInstalledAdapterFamily() != family)
    {
        return 1;
    }

    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = window_params.m_Height = 64;
    window_params.m_Hidden = 1;
    window_params.m_FocusOnShow = 0;
    switch (family)
    {
        case ADAPTER_FAMILY_METAL:
        {
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
            break;
        }
        case ADAPTER_FAMILY_OPENGL:
        {
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
            window_params.m_OpenGLUseCoreProfileHint = 1;
            window_params.m_GraphicsApiVersionHint = 33;
            break;
        }
        case ADAPTER_FAMILY_OPENGLES:
        {
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
            break;
        }
        default:
        {
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
            break;
        }
    }
    if (dmPlatform::OpenWindow(window, window_params) != WINDOW_RESULT_OK)
    {
        dmPlatform::DeleteWindow(window);
        Finalize();
        return 1;
    }
    JobSystemCreateParams job_params = {};
    job_params.m_ThreadCount = 1;
    HJobContext jobs = JobSystemCreate(&job_params);
    ContextParams params = {};
    params.m_JobContext = jobs;
    params.m_Window = window;
    params.m_Width = params.m_Height = 64;
    params.m_VerifyGraphicsCalls = 1;
    HContext context = NewContext(params);
    bool success = context != 0;
    if (context)
    {
        uint8_t reference[6][4];
        const char* shaders[] = { DM_TEST_GLTF_SHADER, DM_TEST_GLTF_SKIN_SHADER, DM_TEST_GLTF_INSTANCE_SHADER, DM_TEST_GLTF_SKIN_INSTANCE_SHADER };
        const char* names[] = { "gltf", "gltf_skinned", "gltf_instanced", "gltf_skinned_instanced" };
        uint64_t start = dmTime::GetMonotonicTime();
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(shaders); ++i)
        {
            dmArray<uint8_t> bytes;
            ShaderDesc* shader = 0;
            if (!ReadFile(shaders[i], bytes) || dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &shader) != dmDDF::RESULT_OK)
            {
                success = false;
                break;
            }
            char error[1024] = {};
            HProgram program = NewProgram(context, shader, error, sizeof(error));
            dmDDF::FreeMessage(shader);
            if (!program)
            {
                dmLogError("Cannot create glTF program %s: %s", names[i], error);
                success = false;
                break;
            }
            bool passed = RunProbes(context, program, names[i], i == 1 || i == 3, i >= 2, reference);
            DeleteProgram(context, program);
            success &= passed;
            if (!passed)
            {
                break;
            }
        }
        printf("GPU probes completed in %.3f ms\n", (dmTime::GetMonotonicTime() - start) / 1000.0);
        CloseWindow(context);
        DeleteContext(context);
    }
    Finalize();
    JobSystemDestroy(jobs);
    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
    return success ? 0 : 1;
}
