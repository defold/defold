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
#include <dlib/time.h>
#include <platform/window.hpp>
#include <graphics/graphics.h>
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
    float m_Uv1[2];
    float m_Weights[4];
    float m_Bones[4];
};

struct Probe
{
    const char* m_Name;
    float       m_Ior;
    float       m_Transmission;
    float       m_Thickness;
    float       m_Distance;
    float       m_Scale;
    float       m_Attenuation[3];
    float       m_TransmissionTexture;
    float       m_ThicknessTexture;
    float       m_Unlit;
    float       m_Metallic;
    float       m_BaseColor;
    float       m_Ambient;
    float       m_Expected[3]; // Independent expected linear outgoing radiance.
    float       m_Roughness;
    float       m_Environment;
    bool        m_DisableCapture;
    bool        m_LinearOutput;
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
        SetConstantV4(context, &value, 1, location);
}

static void SetMatrix(HContext context, HProgram program, const char* name, const dmVMath::Matrix4& value)
{
    SetConstantM4(context, (const dmVMath::Vector4*)&value, 1, FindUniformLocation(program, name));
}

static uint8_t EncodeSrgb(float linear)
{
    float encoded = linear <= 0.0031308f ? linear * 12.92f : 1.055f * powf(linear, 1.0f / 2.4f) - 0.055f;
    return (uint8_t)(fminf(fmaxf(encoded, 0.0f), 1.0f) * 255.0f + 0.5f);
}

static float DecodeSrgb(float encoded)
{
    float value = encoded / 255.0f;
    return value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
}

// Upload constant face colors with independently authored radiance for each mip.
static HTexture NewIblTexture(HContext context, TextureType type, uint32_t size, const float* mip_colors, uint32_t mip_count)
{
    TextureCreationParams creation;
    creation.m_Type = type;
    creation.m_Width = creation.m_Height = size;
    creation.m_MipMapCount = mip_count;
    HTexture       texture = NewTexture(context, creation);
    uint32_t       faces = type == TEXTURE_TYPE_CUBE_MAP ? 6 : 1;
    dmArray<float> data;
    data.SetCapacity(faces * size * size * 4);
    for (uint32_t level = 0; level < mip_count; ++level)
    {
        data.SetSize(faces * size * size * 4);
        for (uint32_t i = 0; i < data.Size(); ++i)
            data[i] = mip_colors[level * 4 + i % 4];
        TextureParams upload;
        upload.m_Width = upload.m_Height = size;
        upload.m_Format = TEXTURE_FORMAT_RGBA32F;
        upload.m_Depth = upload.m_LayerCount = 1;
        upload.m_MipMap = level;
        upload.m_Data = data.Begin();
        upload.m_DataSize = (size_t)size * size * 4 * sizeof(float); // One face's stride for cubemaps.
        upload.m_MinFilter = mip_count > 1 ? TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR : TEXTURE_FILTER_LINEAR;
        upload.m_MagFilter = TEXTURE_FILTER_LINEAR;
        upload.m_UWrap = upload.m_VWrap = TEXTURE_WRAP_CLAMP_TO_EDGE;
        SetTexture(context, texture, upload);
        size /= 2;
    }
    return texture;
}

static void SaveCapture(const char* name, const uint8_t* pixels, uint32_t width, uint32_t height)
{
    const char* directory = getenv("DM_TEST_GLTF_CAPTURE_DIR");
    if (!directory)
        return;
    char path[2048];
    dmSnPrintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE* file = fopen(path, "wb");
    if (!file)
        return;
    fprintf(file, "P6\n%u %u\n255\n", width, height);
    for (uint32_t i = 0; i < width * height; ++i)
    {
        const uint8_t rgb[] = { pixels[i * 4 + 2], pixels[i * 4 + 1], pixels[i * 4] };
        fwrite(rgb, 1, sizeof(rgb), file);
    }
    fclose(file);
}

// Spatial capture checks guard projection/orientation, rough transmission blur,
// and the absence of microfacet refraction when IOR equals the surrounding air.
static bool RunCaptureProbes(HContext context, HProgram program, uint32_t scene_unit)
{
    const uint32_t             width = 128, height = 64;
    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferParams[0].m_Width = width;
    params.m_ColorBufferCreationParams[0].m_Height = params.m_ColorBufferParams[0].m_Height = height;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_ColorBufferParams[0].m_UWrap = params.m_ColorBufferParams[0].m_VWrap = TEXTURE_WRAP_CLAMP_TO_EDGE;
    params.m_ColorBufferParams[0].m_MinFilter = params.m_ColorBufferParams[0].m_MagFilter = TEXTURE_FILTER_LINEAR;
    params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    HRenderTarget scene = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);
    HRenderTarget target = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);
    SetRenderTarget(context, scene, RenderTargetBindingParams());
    SetViewport(context, 0, 0, width, height);
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, 1, 0));
    EnableState(context, STATE_SCISSOR_TEST);
    const uint8_t colors[][3] = { { 0, 0, 255 }, { 255, 255, 255 }, { 255, 0, 0 }, { 0, 255, 0 } };
    for (uint32_t i = 0; i < 4; ++i)
    {
        SetScissor(context, (i % 2) * width / 2, (i / 2) * height / 2, width / 2, height / 2);
        SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(colors[i][0] / 255.0f, colors[i][1] / 255.0f, colors[i][2] / 255.0f, 1));
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
    }
    DisableState(context, STATE_SCISSOR_TEST);
    uint8_t source[width * height * 4];
    ReadPixels(context, 0, 0, width, height, source, sizeof(source));
    EnableTexture(context, scene_unit, 0, GetRenderTargetTexture(context, scene, BUFFER_TYPE_COLOR0_BIT));
    SetRenderTarget(context, target, RenderTargetBindingParams());
    SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(1));
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, 0, 0));
    SetVector(context, program, "light_info", dmVMath::Vector4(0));
    SetVector(context, program, "transmission_params", dmVMath::Vector4(1, 0, 0, 0));
    SetVector(context, program, "pbrTransmission.transmissionFactor", dmVMath::Vector4(1, 0, 0, 0));
    SetVector(context, program, "pbrTransmission.transmissionTextures", dmVMath::Vector4(0));
    SetVector(context, program, "pbrVolume.thicknessFactorAndAttenuationColor", dmVMath::Vector4(0, 1, 1, 1));
    SetVector(context, program, "pbrVolume.volumeTextures", dmVMath::Vector4(0));
    SetMatrix(context, program, "mtx_worldview", dmVMath::Matrix4::identity());

    const char* names[] = { "transmission_smooth", "transmission_rough", "ior_one_smooth", "ior_one_rough" };
    uint8_t     images[4][width * height * 4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0, i % 2, 0, 0));
        SetVector(context, program, "pbrIor.ior", dmVMath::Vector4(i < 2 ? 1.5f : 1.0f, 0, 0, 0));
        Clear(context, BUFFER_TYPE_COLOR0_BIT, 255, 0, 255, 0, 1, 0);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        ReadPixels(context, 0, 0, width, height, images[i], sizeof(images[i]));
        SaveCapture(names[i], images[i], width, height);
    }

    bool orientation = true;
    for (uint32_t y = height / 4; y < height; y += height / 2)
    {
        for (uint32_t x = width / 4; x < width; x += width / 2)
        {
            uint32_t pixel = (y * width + x) * 4;
            for (uint32_t c = 0; c < 3; ++c)
                orientation &= abs((int)images[2][pixel + c] - source[pixel + c]) <= 1;
        }
    }
    uint32_t left = ((height / 4) * width + width / 2 - 2) * 4;
    uint32_t right = left + 4 * 4;
    int      sharp_contrast = 0, rough_contrast = 0;
    for (uint32_t c = 0; c < 3; ++c)
    {
        sharp_contrast += abs((int)images[0][left + c] - images[0][right + c]);
        rough_contrast += abs((int)images[1][left + c] - images[1][right + c]);
    }
    bool blur = sharp_contrast > 400 && rough_contrast < sharp_contrast / 2;
    bool air = memcmp(images[2], images[3], sizeof(images[2])) == 0;
    printf("capture_orientation: %s\n", orientation ? "PASS" : "FAIL");
    printf("roughness_blurs_background: %s (edge contrast %d -> %d)\n", blur ? "PASS" : "FAIL", sharp_contrast, rough_contrast);
    printf("ior_one_preserves_background: %s\n", air ? "PASS" : "FAIL");
    SetRenderTarget(context, 0, RenderTargetBindingParams());
    DisableTexture(context, scene_unit, 0);
    DeleteRenderTarget(context, target);
    DeleteRenderTarget(context, scene);
    return orientation && blur && air;
}

// A striped capture and perspective rays expose incorrect exit positions,
// including direction changes caused by nonuniform scale or camera rotation.
static bool RunRefractionProbes(HContext context, HProgram program, HVertexBuffer buffer, uint32_t scene_unit)
{
    using namespace dmVMath;
    const uint32_t             width = 128, height = 127;
    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferParams[0].m_Width = width;
    params.m_ColorBufferCreationParams[0].m_Height = params.m_ColorBufferParams[0].m_Height = height;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_ColorBufferParams[0].m_UWrap = params.m_ColorBufferParams[0].m_VWrap = TEXTURE_WRAP_CLAMP_TO_EDGE;
    params.m_ColorBufferParams[0].m_MinFilter = params.m_ColorBufferParams[0].m_MagFilter = TEXTURE_FILTER_LINEAR;
    params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    HRenderTarget scene = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);
    HRenderTarget target = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);
    const Vector4 positions[] = { Vector4(-2, -2, -2, 1), Vector4(6, -2, -2, 1), Vector4(-2, 6, -2, 1) };
    Vertex        vertices[3];
    memset(vertices, 0, sizeof(vertices));
    for (uint32_t i = 0; i < 3; ++i)
    {
        for (uint32_t c = 0; c < 4; ++c)
        {
            vertices[i].m_Position[c] = positions[i][c];
            vertices[i].m_Color[c] = 1;
        }
        vertices[i].m_Normal[2] = 1;
        vertices[i].m_Tangent[0] = vertices[i].m_Tangent[3] = 1;
    }
    SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
    EnableVertexBuffer(context, buffer, 0);
    const Matrix4 projection = Matrix4::perspective(1.57079632679f, 1, 0.1f, 100);
    SetMatrix(context, program, "mtx_proj", projection);
    SetMatrix(context, program, "mtx_projection", projection);
    SetMatrix(context, program, "mtx_view", Matrix4::identity());
    SetMatrix(context, program, "mtx_worldview", Matrix4::identity());
    SetMatrix(context, program, "mtx_normal", Matrix4::identity());
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", Vector4(0.5f, 1, 1, 0));
    SetVector(context, program, "light_info", Vector4(0));
    SetRenderTarget(context, scene, RenderTargetBindingParams());
    SetViewport(context, 0, 0, width, height);
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    EnableState(context, STATE_SCISSOR_TEST);
    for (uint32_t i = 0; i < width / 8; ++i)
    {
        SetScissor(context, i * 8, 0, 8, height);
        SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", Vector4(float(i & 1), float((i >> 1) & 1), float((i >> 2) & 1), 1));
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
    }
    DisableState(context, STATE_SCISSOR_TEST);
    uint8_t source[width * height * 4];
    ReadPixels(context, 0, 0, width, height, source, sizeof(source));
    SaveCapture("refraction_source", source, width, height);
    EnableTexture(context, scene_unit, 0, GetRenderTargetTexture(context, scene, BUFFER_TYPE_COLOR0_BIT));
    SetRenderTarget(context, target, RenderTargetBindingParams());
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", Vector4(0.5f, 1, 0, 0));
    SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", Vector4(1));
    SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", Vector4(0));
    SetVector(context, program, "pbrVolume.thicknessFactorAndAttenuationColor", Vector4(3, 1, 1, 1));
    SetVector(context, program, "pbrVolume.attenuationDistance", Vector4(0));

    const Matrix4 rotated_scale = Matrix4::rotationY(0.7f) * Matrix4::rotationZ(0.35f) * Matrix4::scale(Vector3(2, 0.75f, 0.5f));
    const Matrix4 worlds[] = { Matrix4::identity(), Matrix4::scale(Vector3(2)), Matrix4::scale(Vector3(1, 1, 2)), rotated_scale, rotated_scale, rotated_scale };
    const float   iors[] = { 1.5f, 1.5f, 1, 1, 1.5f, 1.5f };
    const char*   names[] = { "refraction_perspective", "refraction_uniform_scale", "air_nonuniform_scale", "air_rotated_scale", "refraction_rotated_scale", "refraction_camera_rotation" };
    bool          success = true;
    for (uint32_t i = 0; i < 6; ++i)
    {
        Matrix4 view = i == 5 ? Matrix4::rotationY(-0.4f) * Matrix4::rotationZ(0.25f) : Matrix4::identity();
        Matrix4 model_view = view * worlds[i];
        Matrix4 inverse_model_view = affineInverse(model_view);
        Vector4 normal = transpose(model_view) * Vector4(0, 0, 1, 0);
        float   normal_length = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
        for (uint32_t v = 0; v < 3; ++v)
        {
            Vector4 local_position = inverse_model_view * positions[v];
            for (uint32_t c = 0; c < 4; ++c)
                vertices[v].m_Position[c] = local_position[c];
            for (uint32_t c = 0; c < 3; ++c)
                vertices[v].m_Normal[c] = normal[c] / normal_length;
        }
        SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_DYNAMIC_DRAW);
        EnableVertexBuffer(context, buffer, 0);
        SetMatrix(context, program, "mtx_view", view);
        SetMatrix(context, program, "mtx_worldview", model_view);
        SetMatrix(context, program, "mtx_normal", transpose(inverse_model_view));
        SetVector(context, program, "pbrIor.ior", Vector4(iors[i], 0, 0, 0));
        Clear(context, BUFFER_TYPE_COLOR0_BIT, 255, 0, 255, 0, 1, 0);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        uint8_t image[width * height * 4];
        ReadPixels(context, 0, 0, width, height, image, sizeof(image));
        SaveCapture(names[i], image, width, height);

        bool           passed = true;
        const uint32_t samples[] = { 20, 44, 84, 108 };
        for (uint32_t s = 0; s < 4; ++s)
        {
            uint32_t x = samples[s], y = height / 2;
            float    px = (2 * (x + 0.5f) / width - 1) * 2;
            float    py = (2 * (y + 0.5f) / height - 1) * 2;
            float    distance = sqrtf(px * px + py * py + 4);
            float    eta = 1 / iors[i];
            float    rx = eta * px / distance, ry = eta * py / distance;
            float    rz = -sqrtf(1 - rx * rx - ry * ry);
            Vector4  local_ray = inverse_model_view * Vector4(rx, ry, rz, 0);
            float    path_length = 3 / sqrtf(local_ray[0] * local_ray[0] + local_ray[1] * local_ray[1] + local_ray[2] * local_ray[2]);
            float    u = (px + rx * path_length) / (2 - rz * path_length) * 0.5f + 0.5f;
            float    texel = u * width - 0.5f;
            int      source_x = int(floorf(texel));
            float    blend = texel - source_x;
            uint32_t x0 = uint32_t(fminf(fmaxf(float(source_x), 0), float(width - 1)));
            uint32_t x1 = uint32_t(fminf(fmaxf(float(source_x + 1), 0), float(width - 1)));
            float    f0 = (iors[i] - 1) / (iors[i] + 1);
            f0 *= f0;
            float    fresnel = f0 + (1 - f0) * powf(1 - 2 / distance, 5);
            uint32_t pixel = (y * width + x) * 4;
            uint32_t source0 = (y * width + x0) * 4;
            uint32_t source1 = (y * width + x1) * 4;
            for (uint32_t c = 0; c < 3; ++c)
            {
                // The production capture is linearly filtered in encoded RGB.
                float   encoded = source[source0 + c] * (1 - blend) + source[source1 + c] * blend;
                uint8_t expected = EncodeSrgb(DecodeSrgb(encoded) * (1 - fresnel));
                bool    matches = abs(int(image[pixel + c]) - expected) <= 1;
                if (!matches)
                    printf("%s x=%u channel=%u: expected %u, got %u (scene x=%.3f)\n", names[i], x, c, expected, image[pixel + c], texel);
                passed &= matches;
            }
            passed &= image[pixel + 3] == 255;
        }
        printf("%s: %s\n", names[i], passed ? "PASS" : "FAIL");
        success &= passed;
    }
    SetRenderTarget(context, 0, RenderTargetBindingParams());
    DisableTexture(context, scene_unit, 0);
    DeleteRenderTarget(context, target);
    DeleteRenderTarget(context, scene);
    return success;
}

static HProgram LoadMaterialProgram(HContext context, const char* root, const char* name)
{
    char             path[2048];
    dmArray<uint8_t> bytes;
    dmSnPrintf(path, sizeof(path), "%s/material/%s.materialc", root, name);
    dmRenderDDF::MaterialDesc* material;
    if (!ReadFile(path, bytes) || dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &material) != dmDDF::RESULT_OK)
        return 0;
    dmSnPrintf(path, sizeof(path), "%s%s", root, material->m_Program);
    dmDDF::FreeMessage(material);
    ShaderDesc* shader;
    if (!ReadFile(path, bytes) || dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &shader) != dmDDF::RESULT_OK)
        return 0;
    char     error[1024] = {};
    HProgram program = NewProgram(context, shader, error, sizeof(error));
    dmDDF::FreeMessage(shader);
    if (!program)
        dmLogError("Cannot create %s program: %s", name, error);
    return program;
}

static HRenderTarget NewCaptureTarget(HContext context, uint32_t width, uint32_t height, TextureFormat format = TEXTURE_FORMAT_RGBA16F, bool depth = false)
{
    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferParams[0].m_Width = width;
    params.m_ColorBufferCreationParams[0].m_Height = params.m_ColorBufferParams[0].m_Height = height;
    params.m_ColorBufferParams[0].m_Format = format;
    params.m_ColorBufferParams[0].m_MinFilter = params.m_ColorBufferParams[0].m_MagFilter = TEXTURE_FILTER_LINEAR;
    params.m_ColorBufferParams[0].m_UWrap = params.m_ColorBufferParams[0].m_VWrap = TEXTURE_WRAP_CLAMP_TO_EDGE;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    uint32_t buffers = BUFFER_TYPE_COLOR0_BIT;
    if (depth)
    {
        params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
        params.m_DepthTexture = true;
        params.m_DepthBufferCreationParams.m_Width = params.m_DepthBufferParams.m_Width = width;
        params.m_DepthBufferCreationParams.m_Height = params.m_DepthBufferParams.m_Height = height;
        params.m_DepthBufferParams.m_Format = TEXTURE_FORMAT_DEPTH;
        params.m_DepthBufferParams.m_MinFilter = params.m_DepthBufferParams.m_MagFilter = TEXTURE_FILTER_NEAREST;
        buffers |= BUFFER_TYPE_DEPTH_BIT;
    }
    return NewRenderTarget(context, buffers, params);
}

// Production capture, reduction and atlas shaders must preserve coverage and HDR
// radiance. Uncovered pixels use the prefiltered environment, guarding dark rough tiles.
static bool RunTransmissionFilterProbes(HContext context, HProgram program, HVertexDeclaration declaration, HTexture fallback, uint32_t scene_unit, uint32_t blur_unit, uint32_t environment_unit, uint32_t brdf_unit, const char* root, float opacity, uint32_t depth_unit)
{
    HProgram filter = LoadMaterialProgram(context, root, "transmission_downsample");
    if (!filter)
        return false;
    const uint32_t width = 8, height = 8;
    HRenderTarget  scene = NewCaptureTarget(context, width, height, TEXTURE_FORMAT_RGBA16F, true);
    HRenderTarget  target = NewCaptureTarget(context, width, height, TEXTURE_FORMAT_RGBA);
    HRenderTarget  levels[3];
    for (uint32_t i = 0; i < 3; ++i)
        levels[i] = NewCaptureTarget(context, width >> (i + 1), height >> (i + 1));
    HRenderTarget atlas = NewCaptureTarget(context, 4, 7);
    const float   environment_color[] = { 4.0f, 2.0f, 0.5f, 1 };
    const float   geometry_color[] = { 8.0f, 4.0f, 2.0f };
    HTexture      cube = NewIblTexture(context, TEXTURE_TYPE_CUBE_MAP, 1, environment_color, 1);
    const float   zero_brdf[] = { 0, 0, 0, 0 };
    HTexture      no_fresnel = NewIblTexture(context, TEXTURE_TYPE_2D, 1, zero_brdf, 1);
    EnableTexture(context, environment_unit, 0, cube);
    EnableTexture(context, brdf_unit, 0, no_fresnel);
    SetRenderTarget(context, scene, RenderTargetBindingParams());
    SetViewport(context, 0, 0, width, height);
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 0, 1, 0);
    EnableProgram(context, program);
    EnableVertexDeclaration(context, declaration, 0, 0, program);
    EnableTexture(context, 0, 0, fallback);
    SetMatrix(context, program, "mtx_worldview", dmVMath::Matrix4::identity());
    SetMatrix(context, program, "mtx_normal", dmVMath::Matrix4::identity());
    SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(1, 1, 1, opacity));
    SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0));
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, 0, opacity < 1 ? 2 : 0));
    SetVector(context, program, "transmission_params", dmVMath::Vector4(0, 1, 0, 0));
    SetVector(context, program, "ibl_params", dmVMath::Vector4(0));
    SetVector(context, program, "pbrEmissiveFactorAndStrength", dmVMath::Vector4(1, 0.5f, 0.25f, 8));
    SetVector(context, program, "light_info", dmVMath::Vector4(0));
    SetViewport(context, 2, 2, 4, 4);
    EnableState(context, STATE_BLEND);
    SetBlendFuncSeparate(context, BLEND_FACTOR_SRC_ALPHA, BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, BLEND_FACTOR_ONE, BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
    DisableState(context, STATE_BLEND);
    // A quarter of the image has emissive geometry; both it and the environment
    // exceed the LDR range, so clamping either path loses measurable energy.
    float mean[3];
    for (uint32_t c = 0; c < 3; ++c)
        mean[c] = environment_color[c] * (1 - 0.25f * opacity) + geometry_color[c] * 0.25f * opacity;
    EnableProgram(context, filter);
    EnableVertexDeclaration(context, declaration, 0, 0, filter);
    SetSampler(context, FindUniformLocation(filter, "tex_source"), 0);
    HRenderTarget previous = scene;
    uint32_t      previous_width = width, previous_height = height;
    for (uint32_t i = 0; i < 3; ++i)
    {
        SetRenderTarget(context, levels[i], RenderTargetBindingParams());
        SetViewport(context, 0, 0, width >> (i + 1), height >> (i + 1));
        EnableTexture(context, 0, 0, GetRenderTargetTexture(context, previous, BUFFER_TYPE_COLOR0_BIT));
        SetVector(context, filter, "filter_params", dmVMath::Vector4(0.5f / previous_width, 0.5f / previous_height, 0, 0));
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        previous = levels[i];
        previous_width /= 2;
        previous_height /= 2;
    }
    SetRenderTarget(context, atlas, RenderTargetBindingParams());
    SetVector(context, filter, "filter_params", dmVMath::Vector4(0));
    uint32_t y = 0;
    for (uint32_t i = 0; i < 3; ++i)
    {
        SetViewport(context, 0, y, width >> (i + 1), height >> (i + 1));
        EnableTexture(context, 0, 0, GetRenderTargetTexture(context, levels[i], BUFFER_TYPE_COLOR0_BIT));
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        y += height >> (i + 1);
    }
    EnableProgram(context, program);
    EnableVertexDeclaration(context, declaration, 0, 0, program);
    EnableTexture(context, 0, 0, fallback);
    EnableTexture(context, scene_unit, 0, GetRenderTargetTexture(context, scene, BUFFER_TYPE_COLOR0_BIT));
    EnableTexture(context, blur_unit, 0, GetRenderTargetTexture(context, atlas, BUFFER_TYPE_COLOR0_BIT));
    SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, 0, 0));
    SetVector(context, program, "pbrTransmission.transmissionFactor", dmVMath::Vector4(1, 0, 0, 0));
    SetVector(context, program, "pbrVolume.thicknessFactorAndAttenuationColor", dmVMath::Vector4(0, 1, 1, 1));
    SetVector(context, program, "pbrVolume.volumeTextures", dmVMath::Vector4(0));
    SetVector(context, program, "pbrTransmission.transmissionTextures", dmVMath::Vector4(0));
    SetVector(context, program, "ibl_params", dmVMath::Vector4(1, 0, 0, 0));
    SetVector(context, program, "pbrEmissiveFactorAndStrength", dmVMath::Vector4(0));
    SetVector(context, program, "transmission_params", dmVMath::Vector4(1, 0, 0, 0));
    SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(0.125f, 0.125f, 0.125f, 1));
    SetVector(context, program, "scene_filter_params", dmVMath::Vector4(width, height, 3, 7));
    SetRenderTarget(context, target, RenderTargetBindingParams());
    SetViewport(context, 0, 0, width, height);
    bool        success = true;
    const float roughness[] = { 1, 1, 0, 0.5f, 1.0f / 3 };
    const float ior[] = { 1.5f, 1, 1.5f, 1.5f, 1.5f };
    const char* names[] = { "transmission_hdr_prefilter", "transmission_filtered_ior_one", "transmission_filtered_smooth", "transmission_level_interpolation", "transmission_atlas_edges" };
    uint8_t     pixels[width * height * 4];
    uint32_t    center = ((height / 2) * width + width / 2) * 4;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(roughness); ++i)
    {
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0, roughness[i], 0, 0));
        SetVector(context, program, "pbrIor.ior", dmVMath::Vector4(ior[i], 0, 0, 0));
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        ReadPixels(context, 0, 0, width, height, pixels, sizeof(pixels));
        char name[128];
        dmSnPrintf(name, sizeof(name), "%s%s", names[i], opacity < 1 ? "_blend" : "");
        SaveCapture(name, pixels, width, height);
        bool passed = true;
        for (uint32_t c = 0; c < 3; ++c)
        {
            float unfiltered = geometry_color[c] * opacity + environment_color[c] * (1 - opacity);
            float expected = unfiltered * 0.96f;
            if (i == 0)
                expected = mean[c] * 0.96f;
            else if (i == 1)
                expected = unfiltered;
            else if (i == 3)
                expected = (unfiltered + mean[c]) * 0.48f;
            passed &= abs((int)pixels[center + 2 - c] - EncodeSrgb(expected * 0.125f)) <= 1;
        }
        if (i == 4)
        {
            // The first reduced level has uncovered corners. Scene coverage
            // from the neighboring level must not leak through bilinear taps.
            const uint32_t corners[] = { 0, width - 1, (height - 1) * width, width * height - 1 };
            float          n_dot_v = 1.0f / sqrtf(1.0f + 2.0f * 0.875f * 0.875f);
            float          transmission = 0.96f * (1 - powf(1 - n_dot_v, 5));
            for (uint32_t p = 0; p < 4; ++p)
                for (uint32_t c = 0; c < 3; ++c)
                    passed &= abs((int)pixels[corners[p] * 4 + 2 - c] - EncodeSrgb(environment_color[c] * transmission * 0.125f)) <= 1;
        }
        printf("%s: %s (%u,%u,%u,%u)\n", name, passed ? "PASS" : "FAIL", pixels[center + 2], pixels[center + 1], pixels[center], pixels[center + 3]);
        success &= passed;
    }
    // A nearby backdrop fills the rough lobe even when most of the camera
    // capture is empty. Farther backdrops must still blur into the environment.
    struct DepthProbe
    {
        const char* m_Name;
        float       m_BackgroundZ;
        float       m_Scale;
        float       m_Roughness;
        float       m_Ior;
        bool        m_Perspective;
        bool        m_GlRange;
        bool        m_Mean;
    };
    const DepthProbe depth_probes[] = {
        { "depth_near_orthographic", -1.125f, 1, 1, 1.5f, false, false, false },
        { "depth_far_orthographic", -9, 1, 1, 1.5f, false, false, true },
        { "depth_near_perspective", -1.125f, 1, 1, 1.5f, true, false, false },
        { "depth_far_perspective", -9, 1, 1, 1.5f, true, false, true },
        { "depth_scaled_perspective", -2.25f, 2, 1, 1.5f, true, false, false },
        { "depth_near_gl_range", -1.125f, 1, 1, 1.5f, true, true, false },
        { "depth_far_gl_range", -9, 1, 1, 1.5f, true, true, true },
        { "depth_ior_one", -9, 1, 1, 1, true, false, false },
        { "depth_smooth", -9, 1, 0, 1.5f, true, false, false },
        { "depth_zero_gap", -1, 1, 1, 1.5f, true, false, false },
    };
    EnableTexture(context, depth_unit, 0, GetRenderTargetTexture(context, scene, BUFFER_TYPE_DEPTH_BIT));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(depth_probes); ++i)
    {
        const DepthProbe& p = depth_probes[i];
        dmVMath::Matrix4  projection = dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, -0.1f));
        if (p.m_Perspective)
        {
            projection = dmVMath::Matrix4::perspective(1.57079632679f, 1, 0.1f, 100);
            if (!p.m_GlRange)
            {
                // Match the renderer's zero-to-one projection for Metal/Vulkan.
                projection = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, 0.5f)) *
                dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, 0.5f)) * projection;
            }
        }
        dmVMath::Vector4 clip = projection * dmVMath::Vector4(0, 0, p.m_BackgroundZ, 1);
        float            depth = (float)clip.getZ() / (float)clip.getW();
        if (p.m_GlRange)
            depth = depth * 0.5f + 0.5f;
        SetRenderTarget(context, scene, RenderTargetBindingParams());
        SetDepthMask(context, true);
        Clear(context, BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, depth, 0);
        SetRenderTarget(context, target, RenderTargetBindingParams());
        SetVector(context, program, "transmission_params", dmVMath::Vector4(1, 0, 1, p.m_GlRange ? 2 : 1));
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0, p.m_Roughness, 0, 0));
        SetVector(context, program, "pbrIor.ior", dmVMath::Vector4(p.m_Ior, 0, 0, 0));
        SetMatrix(context, program, "mtx_worldview", dmVMath::Matrix4::scale(dmVMath::Vector3(p.m_Scale)));
        SetMatrix(context, program, "mtx_projection", projection);
        // The vertex shader always needs this backend's depth range. Only the
        // fragment reconstruction is authored in GL range for the two GL probes.
        dmVMath::Matrix4 vertex_projection = projection;
        if (p.m_GlRange)
            vertex_projection = dmVMath::Matrix4::translation(dmVMath::Vector3(0, 0, 0.5f)) *
            dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, 0.5f)) * projection;
        SetMatrix(context, program, "mtx_proj", vertex_projection);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        ReadPixels(context, 0, 0, width, height, pixels, sizeof(pixels));
        bool passed = true;
        for (uint32_t c = 0; c < 3; ++c)
        {
            float background = p.m_Mean ? mean[c] : geometry_color[c] * opacity + environment_color[c] * (1 - opacity);
            float expected = background * (p.m_Ior == 1 ? 1.0f : 0.96f) * 0.125f;
            passed &= abs((int)pixels[center + 2 - c] - EncodeSrgb(expected)) <= 1;
        }
        char name[128];
        dmSnPrintf(name, sizeof(name), "%s%s", p.m_Name, opacity < 1 ? "_blend" : "");
        SaveCapture(name, pixels, width, height);
        printf("%s: %s (%u,%u,%u,%u)\n", name, passed ? "PASS" : "FAIL", pixels[center + 2], pixels[center + 1], pixels[center], pixels[center + 3]);
        success &= passed;
    }
    SetMatrix(context, program, "mtx_worldview", dmVMath::Matrix4::identity());
    SetMatrix(context, program, "mtx_proj", dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, -0.1f)));
    SetMatrix(context, program, "mtx_projection", dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, -0.1f)));
    DisableTexture(context, depth_unit, 0);
    DisableTexture(context, scene_unit, 0);
    DisableTexture(context, blur_unit, 0);
    SetRenderTarget(context, 0, RenderTargetBindingParams());
    for (uint32_t i = 0; i < 3; ++i)
        DeleteRenderTarget(context, levels[i]);
    DeleteRenderTarget(context, atlas);
    DeleteRenderTarget(context, scene);
    DeleteRenderTarget(context, target);
    DisableTexture(context, environment_unit, 0);
    DisableTexture(context, brdf_unit, 0);
    DeleteTexture(context, cube);
    DeleteTexture(context, no_fresnel);
    DeleteProgram(context, filter);
    return success;
}

// Verify production shader composition on a hidden 1x1 target, including IBL
// roughness mip selection when an environment material is supplied by the project.
static bool RunProbes(HContext context, HProgram program, bool ibl, const char* project_root, bool skinned = false)
{
    Vertex vertices[] = {
        { { -1, -1, -1, 1 }, { 0, 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f } },
        { { 3, -1, -1, 1 }, { 0, 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f } },
        { { -1, 3, -1, 1 }, { 0, 0, 1 }, { 1, 0, 0, 1 }, { 1, 1, 1, 1 }, { 0.5f, 0.5f } },
    };
    if (skinned)
    {
        for (uint32_t i = 0; i < 3; ++i)
        {
            vertices[i].m_Weights[0] = 1;
            vertices[i].m_Bones[1] = 1;
        }
    }
    HVertexBuffer            buffer = NewVertexBuffer(context, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration(context);
    AddVertexStream(streams, "position", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "normal", 3, TYPE_FLOAT, false);
    AddVertexStream(streams, "tangent", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "color", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "texcoord0", 2, TYPE_FLOAT, false);
    AddVertexStream(streams, "texcoord1", 2, TYPE_FLOAT, false);
    AddVertexStream(streams, "bone_weights", 4, TYPE_FLOAT, false);
    AddVertexStream(streams, "bone_indices", 4, TYPE_FLOAT, false);
    HVertexDeclaration declaration = NewVertexDeclaration(context, streams, sizeof(Vertex));
    DeleteVertexStreamDeclaration(streams);

    RenderTargetCreationParams params;
    params.m_ColorBufferCreationParams[0].m_Width = params.m_ColorBufferParams[0].m_Width = 1;
    params.m_ColorBufferCreationParams[0].m_Height = params.m_ColorBufferParams[0].m_Height = 1;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_SampleCount = 1;
    params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    HRenderTarget         target = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);

    TextureCreationParams creation;
    creation.m_Width = creation.m_Height = 1;
    HTexture      texture = NewTexture(context, creation);
    const uint8_t texel[] = { 64, 128, 255, 255 };
    TextureParams upload;
    upload.m_Width = upload.m_Height = 1;
    upload.m_Format = TEXTURE_FORMAT_RGBA;
    upload.m_Depth = upload.m_LayerCount = 1;
    upload.m_Data = texel;
    upload.m_DataSize = sizeof(texel);
    upload.m_MinFilter = upload.m_MagFilter = TEXTURE_FILTER_NEAREST;
    SetTexture(context, texture, upload);

    HTexture pose = 0;
    if (skinned)
    {
        creation.m_Width = 6;
        creation.m_Height = 1;
        pose = NewTexture(context, creation);
    }

    HTexture diffuse = 0, specular = 0, brdf = 0;
    if (ibl)
    {
        const float diffuse_color[] = { 0.1f, 0.2f, 0.3f, 1 };
        const float specular_colors[] = { 0.25f, 0.5f, 1, 1, 0.75f, 0.25f, 0.5f, 1 };
        const float brdf_color[] = { 0.5f, 0.125f, 0, 1 };
        diffuse = NewIblTexture(context, TEXTURE_TYPE_CUBE_MAP, 1, diffuse_color, 1);
        specular = NewIblTexture(context, TEXTURE_TYPE_CUBE_MAP, 2, specular_colors, 2);
        brdf = NewIblTexture(context, TEXTURE_TYPE_2D, 1, brdf_color, 1);
    }

    // The captured scene is white; the data map has different R/G/B channels to
    // detect incorrect transmission/thickness channel selection or sRGB decoding.
    HRenderTarget scene = NewRenderTarget(context, BUFFER_TYPE_COLOR0_BIT, params);
    BeginFrame(context);
    SetColorMask(context, true, true, true, true);
    SetRenderTarget(context, scene, RenderTargetBindingParams());
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 255, 255, 255, 255, 1.0f, 0);
    SetRenderTarget(context, target, RenderTargetBindingParams());
    SetViewport(context, 0, 0, 1, 1);
    DisableState(context, STATE_BLEND);
    DisableState(context, STATE_DEPTH_TEST);
    DisableState(context, STATE_CULL_FACE);
    EnableProgram(context, program);
    EnableVertexBuffer(context, buffer, 0);
    EnableVertexDeclaration(context, declaration, 0, 0, program);
    uint32_t textures = 0;
    uint32_t scene_unit = 0;
    uint32_t blur_unit = 0;
    uint32_t depth_unit = 0;
    uint32_t environment_unit = 0;
    uint32_t brdf_unit = 0;
    for (uint32_t i = 0; i < GetUniformCount(program); ++i)
    {
        Uniform uniform;
        GetUniform(program, i, &uniform);
        if (!IsTypeTextureType(uniform.m_Type))
            continue;
        bool scene_color = strstr(uniform.m_Name, "tex_scene_color") != 0;
        if (scene_color)
            scene_unit = textures;
        if (strstr(uniform.m_Name, "tex_scene_depth"))
            depth_unit = textures;
        if (strstr(uniform.m_Name, "tex_scene_blur"))
            blur_unit = textures;
        if (strstr(uniform.m_Name, "tex_ibl_specular"))
            environment_unit = textures;
        if (strstr(uniform.m_Name, "tex_ibl_brdf"))
            brdf_unit = textures;
        HTexture binding = scene_color ? GetRenderTargetTexture(context, scene, BUFFER_TYPE_COLOR0_BIT) : texture;
        if (strstr(uniform.m_Name, "pose_matrix_cache"))
            binding = pose;
        if (strstr(uniform.m_Name, "tex_ibl_diffuse"))
            binding = diffuse;
        else if (strstr(uniform.m_Name, "tex_ibl_specular"))
            binding = specular;
        else if (strstr(uniform.m_Name, "tex_ibl_brdf"))
            binding = brdf;
        SetSampler(context, uniform.m_Location, textures);
        EnableTexture(context, textures++, 0, binding);
    }

    const dmVMath::Matrix4 identity = dmVMath::Matrix4::identity();
    // Keep clip depth positive on every backend. The engine camera normally
    // applies this backend adjustment when it supplies projection constants.
    const dmVMath::Matrix4 projection = dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, -0.1f));
    SetMatrix(context, program, "mtx_view", identity);
    if (skinned)
        SetVector(context, program, "animation_data", dmVMath::Vector4(0, 1, 6, 1));
    SetMatrix(context, program, "mtx_normal", identity);
    SetMatrix(context, program, "mtx_proj", projection);
    SetMatrix(context, program, "mtx_projection", projection);
    SetVector(context, program, "pbrCommonTextures", dmVMath::Vector4(0));
    SetVector(context, program, "pbrEmissiveFactorAndStrength", dmVMath::Vector4(0));
    SetVector(context, program, "pbrNormalScaleAndOcclusionStrength", dmVMath::Vector4(1, 1, 0, 0));
    SetVector(context, program, "pbrMetallicRoughness.metallicRoughnessTextures", dmVMath::Vector4(0));

    const Probe base_probes[] = {
        { "ior_1", 1, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 1, 1, 1 }, 1, 0 },
        { "ior_1_5", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0.96f, 0.96f, 0.96f }, 1, 0 },
        { "ior_2", 2, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 8.0f / 9, 8.0f / 9, 8.0f / 9 }, 1, 0 },
        { "opaque", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0, 0, 0 }, 1, 0 },
        { "partial_transmission", 1.5f, 0.5f, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0.48f, 0.48f, 0.48f }, 1, 0 },
        { "transmission_red", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 1, 0, 0, 0, 1, 0, { 0.96f * 64 / 255, 0.96f * 64 / 255, 0.96f * 64 / 255 }, 1, 0 },
        { "volume", 1.5f, 1, 2, 2, 1, { 0.25f, 0.5f, 1 }, 0, 0, 0, 0, 1, 0, { 0.24f, 0.48f, 0.96f }, 1, 0 },
        { "thickness_green", 1.5f, 1, 255.0f / 128, 1, 1, { 0.25f, 0.5f, 1 }, 0, 1, 0, 0, 1, 0, { 0.24f, 0.48f, 0.96f }, 1, 0 },
        { "node_scale", 1.5f, 1, 1, 1, 2, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.24f, 0.24f, 0.24f }, 1, 0 },
        { "infinite_distance", 1.5f, 1, 2, 0, 1, { 0, 0, 0 }, 0, 0, 0, 0, 1, 0, { 0.96f, 0.96f, 0.96f }, 1, 0 },
        { "unlit", 1.5f, 1, 1, 1, 1, { 0, 0, 0 }, 1, 1, 1, 0, 1, 0, { 1, 1, 1 }, 1, 0 },
        { "base_color_tint", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 0.5f, 0, { 0.48f, 0.48f, 0.48f }, 1, 0 },
        { "opaque_ior_1", 1, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 0, 1, { 0, 0, 0 }, 1, 0 },
        { "opaque_ior_1_5", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 0, 1, { 0.02f, 0.02f, 0.02f }, 1, 0 },
        { "opaque_ior_2", 2, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 0, 1, { 1.0f / 18, 1.0f / 18, 1.0f / 18 }, 1, 0 },
        { "default_ior", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0.96f, 0.96f, 0.96f }, 1, 0 },
        { "half_metal_full_transmission", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0.5f, 1, 0, { 0.48f, 0.48f, 0.48f }, 1, 0 },
        { "half_metal_partial_transmission", 1.5f, 0.5f, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0.5f, 1, 0, { 0.24f, 0.24f, 0.24f }, 1, 0 },
        { "half_metal_tint", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0.5f, 0.5f, 0, { 0.24f, 0.24f, 0.24f }, 1, 0 },
        { "half_metal_lit", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0.5f, 1, 1, { 0.74f, 0.74f, 0.74f }, 1, 0 },
        { "zero_ior_transmission", 0, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0, 0, 0 }, 1, 0 },
        { "zero_ior_opaque", 0, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 1, { 0.5f, 0.5f, 0.5f }, 1, 0 },
        { "metal", 1.5f, 1, 1, 1, 1, { 0, 0, 0 }, 0, 0, 0, 1, 1, 0, { 0, 0, 0 }, 1, 0 },
        { "capture_disabled_lit", 1.5f, 1, 1, 1, 1, { 0, 0, 0 }, 0, 0, 0, 0, 0.5f, 0.5f, { 0.26f, 0.26f, 0.26f }, 1, 0, true },
        { "linear_capture_unlit", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 1, 0, 0.5f, 0, { 0.5f, 0.5f, 0.5f }, 0, 0, true, true },
    };
    // Cubemap levels have distinct RGB radiance; the LUT is independently fixed
    // at A=0.5, B=0.125. A smooth surface uses the material's 0.04 roughness floor.
    const Probe ibl_probes[] = {
        { "ibl_metal_smooth", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 1, 0, { 0.16875f, 0.30625f, 0.6125f }, 0, 1 },
        { "ibl_metal_rough", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 1, 0, { 0.46875f, 0.15625f, 0.3125f }, 1, 1 },
        { "ibl_metal_mid", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 1, 0, { 0.3125f, 0.234375f, 0.46875f }, 0.5f, 1 },
        { "ibl_metal_tint", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 0.5f, 0, { 0.10125f, 0.18375f, 0.3675f }, 0, 1 },
        { "ibl_dielectric", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0.1685f, 0.246375f, 0.39675f }, 0.5f, 1 },
        { "ibl_zero_intensity", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 1, 0, { 0, 0, 0 }, 0.5f, 0 },
        { "ibl_half_intensity", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 1, 1, 0, { 0.15625f, 0.1171875f, 0.234375f }, 0.5f, 0.5f },
        { "ibl_unlit", 1.5f, 0, 0, 0, 1, { 1, 1, 1 }, 0, 0, 1, 1, 0.5f, 0, { 0.5f, 0.5f, 0.5f }, 0.5f, 1 },
        // Transmission removes indirect diffuse but retains reflected environment light.
        { "ibl_transmission", 1.5f, 1, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 0.5f, 0, { 0.51625f, 0.5071875f, 0.534375f }, 0.5f, 0.5f },
        // Partial transmission keeps the untransmitted fraction of indirect diffuse.
        { "ibl_partial_transmission", 1.5f, 0.5f, 0, 0, 1, { 1, 1, 1 }, 0, 0, 0, 0, 1, 0, { 0.54025f, 0.5551875f, 0.606375f }, 0.5f, 0.5f },
        // Volume attenuation tints the background without tinting reflected IBL.
        { "ibl_volume", 1.5f, 1, 2, 2, 1, { 0.25f, 0.5f, 1 }, 0, 0, 0, 0, 0.5f, 0, { 0.15625f, 0.2671875f, 0.534375f }, 0.5f, 0.5f },
    };
    // Bone Z scale changes mesh-space thickness in world space. Blend two
    // distinct bones and combine bone/model scales; small invertible scales
    // must not fall back to the unskinned approximation.
    const Probe skin_probes[] = {
        { "bone_identity", 1, 1, 1, 1, 1, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.5f, 0.5f, 0.5f }, 0, 0 },
        { "bone_nonuniform_scale", 1, 1, 1, 1, 1, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.25f, 0.25f, 0.25f }, 0, 0 },
        { "bone_blended_scale", 1, 1, 1, 1, 1, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.125f, 0.125f, 0.125f }, 0, 0 },
        { "bone_small_scale", 1, 1, 1, 1, 1, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.9930925f, 0.9930925f, 0.9930925f }, 0, 0 },
        { "bone_and_model_scale", 1, 1, 1, 1, 2, { 0.5f, 0.5f, 0.5f }, 0, 0, 0, 0, 1, 0, { 0.0625f, 0.0625f, 0.0625f }, 0, 0 },
    };
    const Probe* probes = skinned ? skin_probes : ibl ? ibl_probes :
                                                        base_probes;
    uint32_t     probe_count = skinned ? DM_ARRAY_SIZE(skin_probes) : ibl ? DM_ARRAY_SIZE(ibl_probes) :
                                                                            DM_ARRAY_SIZE(base_probes);
    bool         success = true;
    for (uint32_t i = 0; i < probe_count; ++i)
    {
        const Probe& p = probes[i];
        if (skinned)
        {
            const float   scales[] = { 1, 2, 2, 0.01f, 2 };
            const float   matrices[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, scales[i], 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 4, 0 };
            TextureParams pose_upload;
            pose_upload.m_Width = 6;
            pose_upload.m_Height = 1;
            pose_upload.m_Format = TEXTURE_FORMAT_RGBA32F;
            pose_upload.m_Data = matrices;
            pose_upload.m_DataSize = sizeof(matrices);
            pose_upload.m_MinFilter = pose_upload.m_MagFilter = TEXTURE_FILTER_NEAREST;
            SetTexture(context, pose, pose_upload);
            for (uint32_t v = 0; v < 3; ++v)
            {
                vertices[v].m_Weights[0] = i == 2 ? 0.5f : 1;
                vertices[v].m_Weights[1] = i == 2 ? 0.5f : 0;
            }
            SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
        }
        SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(p.m_BaseColor, p.m_BaseColor, p.m_BaseColor, 1));
        SetVector(context, program, "light_info", dmVMath::Vector4(p.m_Ambient, p.m_Ambient, p.m_Ambient, 0));
        SetMatrix(context, program, "mtx_worldview", dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, p.m_Scale)));
        SetMatrix(context, program, "mtx_normal", dmVMath::Matrix4::scale(dmVMath::Vector3(1, 1, 1 / p.m_Scale)));
        SetVector(context, program, "transmission_params", dmVMath::Vector4(p.m_DisableCapture ? 0 : 1, p.m_LinearOutput ? 1 : 0, 0, 0));
        SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, p.m_Unlit, 0));
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(p.m_Metallic, p.m_Roughness, 0, 0));
        if (ibl)
            SetVector(context, program, "ibl_params", dmVMath::Vector4(p.m_Environment, 1, 0, 0));
        SetVector(context, program, "pbrIor.ior", dmVMath::Vector4(p.m_Ior, 0, 0, 0));
        SetVector(context, program, "pbrTransmission.transmissionFactor", dmVMath::Vector4(p.m_Transmission, 0, 0, 0));
        SetVector(context, program, "pbrTransmission.transmissionTextures", dmVMath::Vector4(p.m_TransmissionTexture, 0, 0, 0));
        SetVector(context, program, "pbrVolume.thicknessFactorAndAttenuationColor", dmVMath::Vector4(p.m_Thickness, p.m_Attenuation[0], p.m_Attenuation[1], p.m_Attenuation[2]));
        SetVector(context, program, "pbrVolume.attenuationDistance", dmVMath::Vector4(p.m_Distance, 0, 0, 0));
        SetVector(context, program, "pbrVolume.volumeTextures", dmVMath::Vector4(p.m_ThicknessTexture, 0, 0, 0));
        Clear(context, BUFFER_TYPE_COLOR0_BIT, 255, 0, 255, 0, 1, 0);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
        uint8_t pixel[4];
        ReadPixels(context, 0, 0, 1, 1, pixel, sizeof(pixel));
        bool passed = pixel[3] == 255;
        for (uint32_t c = 0; c < 3; ++c)
        {
            uint8_t expected = p.m_LinearOutput ? (uint8_t)(p.m_Expected[c] * 255 + 0.5f) : EncodeSrgb(p.m_Expected[c]);
            passed &= abs((int)pixel[2 - c] - expected) <= 1;
        }
        printf("%s: %s (%u,%u,%u,%u)\n", p.m_Name, passed ? "PASS" : "FAIL", pixel[2], pixel[1], pixel[0], pixel[3]);
        if (skinned)
            SaveCapture(p.m_Name, pixel, 1, 1);
        success &= passed;
    }
    // UV0 and UV1 address different texels. Independent transforms must select
    // the authored transmission red and thickness green channels, including V flip.
    if (!ibl && !skinned)
    {
        Vertex uv_vertices[3];
        memcpy(uv_vertices, vertices, sizeof(uv_vertices));
        for (uint32_t i = 0; i < 3; ++i)
        {
            uv_vertices[i].m_Uv[0] = 0.25f;
            uv_vertices[i].m_Uv[1] = 0.75f;
            uv_vertices[i].m_Uv1[0] = 0.75f;
            uv_vertices[i].m_Uv1[1] = 0.25f;
        }
        SetVertexBufferData(buffer, sizeof(uv_vertices), uv_vertices, BUFFER_USAGE_STATIC_DRAW);
        const uint8_t         texels[] = { 0, 0, 0, 255, 255, 128, 0, 255, 64, 255, 0, 255, 128, 64, 0, 255 };
        TextureCreationParams uv_creation;
        uv_creation.m_Width = uv_creation.m_Height = 2;
        HTexture uv_texture = NewTexture(context, uv_creation);
        upload.m_Width = upload.m_Height = 2;
        upload.m_Data = texels;
        upload.m_DataSize = sizeof(texels);
        SetTexture(context, uv_texture, upload);
        for (uint32_t unit = 0; unit < textures; ++unit)
            if (unit != scene_unit)
                EnableTexture(context, unit, 0, uv_texture);
        SetMatrix(context, program, "mtx_worldview", identity);
        SetMatrix(context, program, "mtx_normal", identity);
        SetVector(context, program, "transmission_params", dmVMath::Vector4(1, 1, 0, 0));
        SetVector(context, program, "pbrMetallicRoughness.baseColorFactor", dmVMath::Vector4(1));
        SetVector(context, program, "pbrMetallicRoughness.metallicAndRoughnessFactor", dmVMath::Vector4(0));
        SetVector(context, program, "light_info", dmVMath::Vector4(0));
        SetVector(context, program, "pbrAlphaCutoffAndDoubleSidedAndIsUnlit", dmVMath::Vector4(0.5f, 1, 0, 0));
        SetVector(context, program, "pbrIor.ior", dmVMath::Vector4(1, 0, 0, 0));
        SetVector(context, program, "pbrTransmission.transmissionFactor", dmVMath::Vector4(1, 0, 0, 0));
        SetVector(context, program, "pbrVolume.attenuationDistance", dmVMath::Vector4(1, 0, 0, 0));
        struct UvProbe
        {
            const char* m_Name;
            float       m_U[4];
            float       m_V[4];
            bool        m_Thickness;
            float       m_Expected;
        };
        const UvProbe uv_probes[] = {
            { "transmission_uv0", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, false, 64.0f / 255 },
            { "transmission_uv1", { 0, 0, 0, 1 }, { 0, 0, 0, 0 }, false, 1 },
            { "transmission_offset", { 0, 0, 0.5f, 0 }, { 0, 0, 0, 0 }, false, 128.0f / 255 },
            { "transmission_rotated", { -1, 1, 0, 0 }, { -1, -1, 1, 0 }, false, 128.0f / 255 },
            { "thickness_uv0", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, true, 0.5f },
            { "thickness_uv1", { 0, 0, 0, 1 }, { 0, 0, 0, 0 }, true, powf(0.5f, 128.0f / 255) },
            { "thickness_offset", { 0, 0, 0.5f, 0 }, { 0, 0, 0, 0 }, true, powf(0.5f, 64.0f / 255) },
        };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(uv_probes); ++i)
        {
            const UvProbe&   p = uv_probes[i];
            dmVMath::Vector4 u(p.m_U[0], p.m_U[1], p.m_U[2], p.m_U[3]);
            dmVMath::Vector4 v(p.m_V[0], p.m_V[1], p.m_V[2], p.m_V[3]);
            SetVector(context, program, "pbrTransmission.transmissionTextures", dmVMath::Vector4(p.m_Thickness ? 0 : 1, 0, 0, 0));
            SetVector(context, program, "pbrVolume.volumeTextures", dmVMath::Vector4(p.m_Thickness ? 1 : 0, 0, 0, 0));
            SetVector(context, program, "pbrVolume.thicknessFactorAndAttenuationColor", dmVMath::Vector4(p.m_Thickness ? 1 : 0, 0.5f, 0.5f, 0.5f));
            SetVector(context, program, "pbrTransmission.transmissionTextureTransformU", u);
            SetVector(context, program, "pbrTransmission.transmissionTextureTransformV", v);
            SetVector(context, program, "pbrVolume.thicknessTextureTransformU", u);
            SetVector(context, program, "pbrVolume.thicknessTextureTransformV", v);
            Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
            uint8_t pixel[4];
            ReadPixels(context, 0, 0, 1, 1, pixel, sizeof(pixel));
            uint8_t expected = (uint8_t)(p.m_Expected * 255 + 0.5f);
            bool    passed = pixel[3] == 255;
            for (uint32_t c = 0; c < 3; ++c)
                passed &= abs((int)pixel[c] - expected) <= 1;
            printf("%s: %s (%u,%u,%u,%u)\n", p.m_Name, passed ? "PASS" : "FAIL", pixel[2], pixel[1], pixel[0], pixel[3]);
            SaveCapture(p.m_Name, pixel, 1, 1);
            success &= passed;
        }
        SetVector(context, program, "pbrTransmission.transmissionTextureTransformU", dmVMath::Vector4(0));
        SetVector(context, program, "pbrTransmission.transmissionTextureTransformV", dmVMath::Vector4(0));
        SetVector(context, program, "pbrVolume.thicknessTextureTransformU", dmVMath::Vector4(0));
        SetVector(context, program, "pbrVolume.thicknessTextureTransformV", dmVMath::Vector4(0));
        SetVertexBufferData(buffer, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
        for (uint32_t unit = 0; unit < textures; ++unit)
            if (unit != scene_unit)
                EnableTexture(context, unit, 0, texture);
        DeleteTexture(context, uv_texture);
    }
    if (!ibl && !skinned)
    {
        success &= RunCaptureProbes(context, program, scene_unit);
        success &= RunRefractionProbes(context, program, buffer, scene_unit);
    }
    else if (ibl)
    {
        success &= RunTransmissionFilterProbes(context, program, declaration, texture, scene_unit, blur_unit, environment_unit, brdf_unit, project_root, 1.0f, depth_unit);
        success &= RunTransmissionFilterProbes(context, program, declaration, texture, scene_unit, blur_unit, environment_unit, brdf_unit, project_root, 0.5f, depth_unit);
    }
    DisableVertexDeclaration(context, declaration);
    DisableVertexBuffer(context, buffer);
    DisableProgram(context);
    for (uint32_t i = 0; i < textures; ++i)
        DisableTexture(context, i, 0);
    SetRenderTarget(context, 0, RenderTargetBindingParams());
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 0, 0, 0, 255, 1, 0);
    Flip(context);
    if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL)
    {
        // Drain the hidden drawable after presentation before deleting Metal's
        // context. Vulkan readback must remain inside the active frame above.
        uint8_t drained[4];
        ReadPixels(context, 0, 0, 1, 1, drained, sizeof(drained));
    }
    DeleteRenderTarget(context, scene);
    DeleteRenderTarget(context, target);
    DeleteTexture(context, texture);
    if (pose)
        DeleteTexture(context, pose);
    if (ibl)
    {
        DeleteTexture(context, diffuse);
        DeleteTexture(context, specular);
        DeleteTexture(context, brdf);
    }
    DeleteVertexDeclaration(declaration);
    DeleteVertexBuffer(buffer);
    return success;
}

int main(int argc, char** argv)
{
    if (argc != 1 && argc != 3 && argc != 4)
    {
        printf("Usage: %s [--backend metal|opengl|opengles|vulkan | <compiled project directory> metal|opengl|opengles|vulkan [IBL material path]]\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, 0, _IONBF, 0);
    TestMainPlatformInit();
    dmExportedSymbols();
    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);
    atexit(dmLog::LogFinalize);
    dmHashEnableReverseHash(true);
    const char*   backend = argc == 1 ? DM_TEST_GLTF_BACKEND : argv[2];
    AdapterFamily family;
    if (strcmp(backend, "metal") == 0)
        family = ADAPTER_FAMILY_METAL;
    else if (strcmp(backend, "opengl") == 0)
        family = ADAPTER_FAMILY_OPENGL;
    else if (strcmp(backend, "opengles") == 0)
        family = ADAPTER_FAMILY_OPENGLES;
    else if (strcmp(backend, "vulkan") == 0)
        family = ADAPTER_FAMILY_VULKAN;
    else
    {
        dmLogError("Unknown graphics backend: %s", backend);
        return 1;
    }
    if (!InstallAdapter(family) || GetInstalledAdapterFamily() != family)
        return 1;
    char             path[2048];
    bool             ibl = argc == 4;
    dmArray<uint8_t> bytes;
    if (argc == 1 || strcmp(argv[1], "--backend") == 0)
        dmStrlCpy(path, DM_TEST_GLTF_SHADER, sizeof(path));
    else
    {
        dmSnPrintf(path, sizeof(path), "%s%s", argv[1], ibl ? argv[3] : "/builtins/materials/gltf.materialc");
        dmRenderDDF::MaterialDesc* material;
        if (!ReadFile(path, bytes) || dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &material) != dmDDF::RESULT_OK)
            return 1;
        dmSnPrintf(path, sizeof(path), "%s%s", argv[1], material->m_Program);
        dmDDF::FreeMessage(material);
    }
    ShaderDesc* shader;
    if (!ReadFile(path, bytes) || dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &shader) != dmDDF::RESULT_OK)
        return 1;

    HWindow            window = dmPlatform::NewWindow();
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = window_params.m_Height = 256;
    window_params.m_Hidden = 1;
    window_params.m_FocusOnShow = 0;
    switch (family)
    {
        case ADAPTER_FAMILY_METAL:
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
            break;
        case ADAPTER_FAMILY_OPENGL:
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
            window_params.m_OpenGLUseCoreProfileHint = 1;
            window_params.m_GraphicsApiVersionHint = 33;
            break;
        case ADAPTER_FAMILY_OPENGLES:
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGLES;
            break;
        default:
            window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_VULKAN;
            break;
    }
    if (dmPlatform::OpenWindow(window, window_params) != WINDOW_RESULT_OK)
    {
        dmDDF::FreeMessage(shader);
        dmPlatform::DeleteWindow(window);
        Finalize();
        return 1;
    }
    JobSystemCreateParams job_params = {};
    job_params.m_ThreadCount = 1;
    HJobContext   jobs = JobSystemCreate(&job_params);
    ContextParams params = {};
    params.m_JobContext = jobs;
    params.m_Window = window;
    params.m_Width = params.m_Height = 256;
    params.m_VerifyGraphicsCalls = 1;
    HContext context = NewContext(params);
    char     error[1024] = {};
    HProgram program = context ? NewProgram(context, shader, error, sizeof(error)) : 0;
    dmDDF::FreeMessage(shader);
    bool success = false;
    if (program)
    {
        uint64_t start = dmTime::GetMonotonicTime();
        success = RunProbes(context, program, ibl, argc >= 3 ? argv[1] : 0);
        if (!ibl)
        {
            ShaderDesc* skin_shader = 0;
            if (ReadFile(DM_TEST_GLTF_SKIN_SHADER, bytes) &&
                dmDDF::LoadMessage(bytes.Begin(), bytes.Size(), &skin_shader) == dmDDF::RESULT_OK)
            {
                HProgram skin_program = NewProgram(context, skin_shader, error, sizeof(error));
                dmDDF::FreeMessage(skin_shader);
                if (skin_program)
                {
                    success &= RunProbes(context, skin_program, false, 0, true);
                    DeleteProgram(context, skin_program);
                }
                else
                    success = false;
            }
            else
                success = false;
        }
        printf("GPU probes completed in %.3f ms\n", (dmTime::GetMonotonicTime() - start) / 1000.0);
        DeleteProgram(context, program);
    }
    else
        dmLogError("Cannot create glTF program: %s", error);
    if (context)
    {
        CloseWindow(context);
        DeleteContext(context);
    }
    Finalize();
    JobSystemDestroy(jobs);
    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
    return success ? 0 : 1;
}
