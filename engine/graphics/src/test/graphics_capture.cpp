// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (https://www.defold.com/license).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <platform/window.hpp>
#include "test_app_graphics.h"
#include "graphics_capture_shaders.h"

#define STB_IMAGE_WRITE_STATIC
#define STBIWDEF static inline
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#if defined(_WIN32)
#include <d3dcompiler.h>
#endif

using namespace dmGraphics;

static const uint32_t CAPTURE_SIZE = 256;
static const uint32_t CAPTURE_BYTES = CAPTURE_SIZE * CAPTURE_SIZE * 4;
static const uint32_t CAPTURE_BUFFERS = BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT;

static const char* CAPTURE_CASES[] = {
    "clear", "triangle", "stencil", "stencil_nested", "stencil_masks",
    "stencil_ops", "stencil_depth", "stencil_faces", "cubemap",
};

enum CaptureRectangle
{
    RECT_BASIC_MASK,
    RECT_BASIC_FILL,
    RECT_ORANGE,
    RECT_GREEN,
    RECT_BLUE,
    RECT_YELLOW,
    RECT_NESTED_OUTER,
    RECT_NESTED_CHILD,
    RECT_NESTED_INNER,
    RECT_BITS_LEFT,
    RECT_BITS_RIGHT,
    RECT_BITS_LOW_READ,
    RECT_BITS_HIGH_READ,
    RECT_DEPTH_OCCLUDER,
    RECT_DEPTH_NEAR,
    RECT_DEPTH_FAR,
    RECT_DEPTH_STENCIL_FAIL,
    RECT_FACE_FRONT,
    RECT_FACE_BACK,
    RECT_FACE_COMPARE_FRONT,
    RECT_FACE_COMPARE_BACK,
    RECT_OP_FIRST,
};

struct CaptureVertex
{
    float m_Position[3];
    float m_Color[3];
};

struct CaptureRectangleData
{
    uint32_t m_Left, m_Top, m_Right, m_Bottom;
    float    m_Depth;
    uint8_t  m_Red, m_Green, m_Blue;
    bool     m_Clockwise;
};

// Pixel coordinates are top-down. Depth is normalized to [0, 1] by every shader.
static const CaptureRectangleData CAPTURE_RECTANGLES[] = {
    {64, 48, 160, 160, 0, 255, 255, 255, false},
    {32, 32, 224, 208, 0, 223, 96, 32, false},
    {16, 16, 240, 240, 0, 223, 96, 32, false},
    {16, 16, 240, 240, 0, 32, 191, 96, false},
    {16, 16, 240, 240, 0, 32, 96, 223, false},
    {16, 16, 240, 240, 0, 223, 191, 32, false},
    {32, 32, 192, 208, 0, 255, 255, 255, false},
    {96, 64, 224, 176, 0, 255, 255, 255, false},
    {128, 96, 208, 144, 0, 255, 255, 255, false},
    {32, 32, 144, 224, 0, 255, 255, 255, false},
    {96, 64, 224, 192, 0, 255, 255, 255, false},
    {16, 112, 240, 128, 0, 223, 32, 191, false},
    {16, 144, 240, 160, 0, 32, 191, 223, false},
    {96, 32, 160, 224, .25f, 255, 255, 255, false},
    {32, 64, 224, 112, .125f, 255, 255, 255, false},
    {32, 144, 224, 192, .75f, 255, 255, 255, false},
    {32, 208, 224, 224, .75f, 255, 255, 255, false},
    {32, 32, 112, 112, 0, 223, 96, 32, false},
    {144, 32, 224, 112, 0, 32, 191, 223, true},
    {32, 144, 112, 224, 0, 223, 32, 191, false},
    {144, 144, 224, 224, 0, 32, 191, 96, true},
    {24, 24, 72, 72, 0, 32, 191, 96, false},
    {96, 24, 144, 72, 0, 32, 191, 96, false},
    {168, 24, 216, 72, 0, 32, 191, 96, false},
    {24, 96, 72, 144, 0, 32, 191, 96, false},
    {96, 96, 144, 144, 0, 32, 191, 96, false},
    {168, 96, 216, 144, 0, 32, 191, 96, false},
    {24, 168, 72, 216, 0, 32, 191, 96, false},
    {96, 168, 144, 216, 0, 32, 191, 96, false},
    {168, 168, 216, 216, 0, 32, 191, 96, false},
};

static void MakeRectangleVertices(const CaptureRectangleData& rectangle, CaptureVertex* vertices)
{
    const uint32_t x[] = {rectangle.m_Left, rectangle.m_Right, rectangle.m_Left,
                          rectangle.m_Right, rectangle.m_Right, rectangle.m_Left};
    const uint32_t y[] = {rectangle.m_Bottom, rectangle.m_Bottom, rectangle.m_Top,
                          rectangle.m_Bottom, rectangle.m_Top, rectangle.m_Top};
    for (uint32_t i = 0; i < 6; ++i)
    {
        uint32_t corner = rectangle.m_Clockwise ? (i / 3) * 3 + (2 - i % 3) : i;
        vertices[i].m_Position[0] = x[corner] * (2.0f / CAPTURE_SIZE) - 1.0f;
        vertices[i].m_Position[1] = 1.0f - y[corner] * (2.0f / CAPTURE_SIZE);
        vertices[i].m_Position[2] = rectangle.m_Depth;
        vertices[i].m_Color[0] = rectangle.m_Red / 255.0f;
        vertices[i].m_Color[1] = rectangle.m_Green / 255.0f;
        vertices[i].m_Color[2] = rectangle.m_Blue / 255.0f;
    }
}

static void DrawRectangle(HContext context, uint32_t rectangle)
{
    Draw(context, PRIMITIVE_TRIANGLES, 3 + 6 * rectangle, 6, 1);
}

struct CaptureBackend
{
    const char*        m_Name;
    AdapterFamily      m_Family;
    WindowsGraphicsApi m_Api;
};

static const CaptureBackend CAPTURE_BACKENDS[] = {
    {"metal",  ADAPTER_FAMILY_METAL,   WINDOW_GRAPHICS_API_METAL},
    {"opengl", ADAPTER_FAMILY_OPENGL,  WINDOW_GRAPHICS_API_OPENGL},
    {"webgpu", ADAPTER_FAMILY_WEBGPU,  WINDOW_GRAPHICS_API_WEBGPU},
    {"vulkan", ADAPTER_FAMILY_VULKAN,  WINDOW_GRAPHICS_API_VULKAN},
    {"dx12",   ADAPTER_FAMILY_DIRECTX, WINDOW_GRAPHICS_API_DIRECTX},
};

struct CaptureResources
{
    HRenderTarget      m_Target;
    HProgram           m_Program;
    HVertexBuffer      m_Vertices;
    HVertexDeclaration m_Declaration;
    HTexture           m_Cubemap;
    HUniformLocation   m_CubemapLocation;
};

static HTexture CreateCaptureCubemap(HContext context)
{
    const uint32_t size = 16;
    // Upload order: +X, -X, +Y, -Y, +Z, -Z. Labels and unequal corner
    // markers make face swaps, rotation and mirroring visible in the cross.
    const uint8_t colors[6][3] = {
        {208, 64, 64}, {64, 176, 96}, {72, 104, 208},
        {208, 176, 48}, {48, 176, 192}, {176, 64, 192},
    };
    const uint8_t signs[2][5] = {{2, 2, 7, 2, 2}, {0, 0, 7, 0, 0}};
    const uint8_t axes[3][5] = {{5, 5, 2, 5, 5}, {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}};
    uint8_t pixels[6 * size * size * 4];
    for (uint32_t face = 0; face < 6; ++face)
    {
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                bool dark = x == 0 || y == 0 || x == size - 1 || y == size - 1 ||
                    (x >= 11 && x <= 13 && y >= 12 && y <= 13);
                bool white = (y == 2 && x >= 2 && x <= 5) || (x == 2 && y >= 2 && y <= 4);
                if (y >= 6 && y < 11)
                {
                    if (x >= 4 && x < 7)
                        white = (signs[face % 2][y - 6] & (1 << (6 - x))) != 0;
                    if (x >= 8 && x < 11)
                        white = (axes[face / 2][y - 6] & (1 << (10 - x))) != 0;
                }
                uint8_t* pixel = pixels + ((face * size + y) * size + x) * 4;
                for (uint32_t channel = 0; channel < 3; ++channel)
                    pixel[channel] = white ? 255 : dark ? 24 : colors[face][channel];
                pixel[3] = 255;
            }
        }
    }
    TextureCreationParams creation;
    creation.m_Type = TEXTURE_TYPE_CUBE_MAP;
    creation.m_Width = size;
    creation.m_Height = size;
    creation.m_LayerCount = 6;
    HTexture texture = NewTexture(context, creation);
    if (!texture)
        return 0;
    TextureParams params;
    params.m_Width = size;
    params.m_Height = size;
    params.m_Depth = 1;
    params.m_LayerCount = 6;
    params.m_Format = TEXTURE_FORMAT_RGBA;
    params.m_Data = pixels;
    params.m_DataSize = size * size * 4; // Per face; the upload contains all six faces.
    params.m_MinFilter = TEXTURE_FILTER_NEAREST;
    params.m_MagFilter = TEXTURE_FILTER_NEAREST;
    SetTexture(context, texture, params);
    return texture;
}

static HVertexBuffer CreateCubemapVertices(HContext context)
{
    // +Y above +Z, -Y below; middle row -X, +Z, +X, -Z.
    const uint32_t columns[] = {2, 0, 1, 1, 1, 3};
    const uint32_t rows[] = {1, 1, 0, 2, 1, 1};
    CaptureVertex vertices[6 * 6];
    for (uint32_t face = 0; face < 6; ++face)
    {
        uint32_t left = 32 + 48 * columns[face];
        uint32_t top = 56 + 48 * rows[face];
        CaptureRectangleData rectangle = {left, top, left + 48, top + 48, 0, 0, 0, 0, false};
        MakeRectangleVertices(rectangle, vertices + face * 6);
        for (uint32_t corner = 0; corner < 6; ++corner)
        {
            CaptureVertex& vertex = vertices[face * 6 + corner];
            float s = ((vertex.m_Position[0] + 1) * (CAPTURE_SIZE / 2) - left) / 24 - 1;
            float t = ((1 - vertex.m_Position[1]) * (CAPTURE_SIZE / 2) - top) / 24 - 1;
            const float directions[6][3] = {
                {1, -t, -s}, {-1, -t, s}, {s, 1, t},
                {s, -1, -t}, {s, -t, 1}, {-s, -t, -1},
            };
            // The second stream carries sampling direction for this program.
            memcpy(vertex.m_Color, directions[face], sizeof(vertex.m_Color));
        }
    }
    return NewVertexBuffer(context, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
}

static bool MakeCaptureParent(const char* filename)
{
    char path[1024];
    if (dmStrlCpy(path, filename, sizeof(path)) >= sizeof(path))
        return false;
    for (char* cursor = path + 1; *cursor; ++cursor)
    {
        if (*cursor != '/' && *cursor != '\\')
            continue;
        char separator = *cursor;
        *cursor = 0;
        dmSys::Result result = dmSys::Mkdir(path, 0755);
        *cursor = separator;
        if (result != dmSys::RESULT_OK && result != dmSys::RESULT_EXIST)
            return false;
    }
    return true;
}

static HProgram NewCaptureProgram(HContext context, AdapterFamily family, bool cubemap)
{
    ShaderDesc desc = {};
#define CAPTURE_SHADER(stage, language, source) AddShaderWithType(&desc, ShaderDesc::SHADER_TYPE_##stage, ShaderDesc::LANGUAGE_##language, (uint8_t*) source, sizeof(source))
    CAPTURE_SHADER(VERTEX, GLSL_SM330, capture_vp);
    CAPTURE_SHADER(VERTEX, MSL_22, capture_vp_msl);
    CAPTURE_SHADER(VERTEX, SPIRV, capture_vert_spv);
    CAPTURE_SHADER(VERTEX, WGSL, capture_vp_wgsl);
    if (cubemap)
    {
        CAPTURE_SHADER(FRAGMENT, GLSL_SM330, capture_cube_fp);
        CAPTURE_SHADER(FRAGMENT, MSL_22, capture_cube_fp_msl);
        CAPTURE_SHADER(FRAGMENT, SPIRV, capture_cube_frag_spv);
        CAPTURE_SHADER(FRAGMENT, WGSL, capture_cube_fp_wgsl);
    }
    else
    {
        CAPTURE_SHADER(FRAGMENT, GLSL_SM330, capture_fp);
        CAPTURE_SHADER(FRAGMENT, MSL_22, capture_fp_msl);
        CAPTURE_SHADER(FRAGMENT, SPIRV, capture_frag_spv);
        CAPTURE_SHADER(FRAGMENT, WGSL, capture_fp_wgsl);
    }
#undef CAPTURE_SHADER

    // NewProgram copies these borrowed mappings before this function returns.
    ShaderDesc::MSLResourceMapping metal_bindings[2] = {};
    ShaderDesc::HLSLResourceMapping dx12_bindings[2] = {};
    if (cubemap)
    {
        AddShaderResource(&desc, "cubemap", family == ADAPTER_FAMILY_OPENGL ? ShaderDesc::SHADER_TYPE_SAMPLER_CUBE :
            ShaderDesc::SHADER_TYPE_TEXTURE_CUBE, 0, 0, BINDING_TYPE_TEXTURE, SHADER_STAGE_FLAG_FRAGMENT);
        if (family != ADAPTER_FAMILY_OPENGL)
        {
            AddShaderResource(&desc, "cube_sampler", ShaderDesc::SHADER_TYPE_SAMPLER, 1, 0, BINDING_TYPE_TEXTURE, SHADER_STAGE_FLAG_FRAGMENT);
            desc.m_Reflection.m_Textures[1].m_Bindinginfo.m_SamplerTextureIndex = 0;
        }
        for (uint32_t i = 0; i < 2; ++i)
        {
            metal_bindings[i].m_NameHash = dmHashString64(i == 0 ? "cubemap" : "cube_sampler");
            metal_bindings[i].m_Binding = i;
            metal_bindings[i].m_MslIndex = i;
            dx12_bindings[i].m_NameHash = metal_bindings[i].m_NameHash;
            dx12_bindings[i].m_Binding = i;
        }
        for (uint32_t i = 0; i < desc.m_Shaders.m_Count; ++i)
        {
            ShaderDesc::Shader& shader = desc.m_Shaders[i];
            if (shader.m_Language == ShaderDesc::LANGUAGE_MSL_22 && shader.m_ShaderType == ShaderDesc::SHADER_TYPE_FRAGMENT)
            {
                shader.m_MslResourceMapping.m_Data = metal_bindings;
                shader.m_MslResourceMapping.m_Count = 2;
            }
        }
    }

#if defined(_WIN32)
    // Compile our own test shaders to DXBC; no private vendor shader package.
    ID3DBlob* bytecode[2] = {};
    ID3DBlob* root_signature = 0;
    if (family == ADAPTER_FAMILY_DIRECTX)
    {
        const D3D_SHADER_MACRO defines[] = {{"CAPTURE_CUBEMAP", cubemap ? "1" : "0"}, {0, 0}};
        const char* entry_points[] = {"vertex_main", "fragment_main"};
        const char* profiles[] = {"vs_5_0", "ps_5_0"};
        for (uint32_t i = 0; i < 2; ++i)
        {
            ID3DBlob* errors = 0;
            HRESULT result = D3DCompile(capture_hlsl, sizeof(capture_hlsl) - 1, "graphics_capture.hlsl", defines, 0,
                entry_points[i], profiles[i], D3DCOMPILE_ENABLE_STRICTNESS, 0, &bytecode[i], &errors);
            if (FAILED(result))
                dmLogError("Capture shader compilation failed: %s", errors ? (const char*) errors->GetBufferPointer() : "unknown error");
            if (errors)
                errors->Release();
            if (bytecode[i])
            {
                AddShaderWithType(&desc, i == 0 ? ShaderDesc::SHADER_TYPE_VERTEX : ShaderDesc::SHADER_TYPE_FRAGMENT,
                    ShaderDesc::LANGUAGE_HLSL_50, (uint8_t*) bytecode[i]->GetBufferPointer(), bytecode[i]->GetBufferSize());
                if (cubemap && i == 1)
                {
                    ShaderDesc::Shader& shader = desc.m_Shaders[desc.m_Shaders.m_Count - 1];
                    shader.m_HlslResourceMapping.m_Data = dx12_bindings;
                    shader.m_HlslResourceMapping.m_Count = 2;
                }
            }
        }
        ID3DBlob* errors = 0;
        HRESULT result = D3DCompile(capture_hlsl, sizeof(capture_hlsl) - 1, "graphics_capture.hlsl", defines, 0,
            "CAPTURE_ROOT_SIGNATURE", "rootsig_1_0", 0, 0, &root_signature, &errors);
        if (FAILED(result))
            dmLogError("Capture root signature compilation failed: %s", errors ? (const char*) errors->GetBufferPointer() : "unknown error");
        if (errors)
            errors->Release();
        if (root_signature)
        {
            desc.m_HlslRootSignature.m_Data = (uint8_t*) root_signature->GetBufferPointer();
            desc.m_HlslRootSignature.m_Count = root_signature->GetBufferSize();
        }
    }
#endif
    AddShaderResource(&desc, "position", ShaderDesc::SHADER_TYPE_VEC3, 0, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
    AddShaderResource(&desc, "color", ShaderDesc::SHADER_TYPE_VEC3, 1, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
    char error[1024] = {};
    HProgram program = NewProgram(context, &desc, error, sizeof(error));
    if (!program)
        dmLogError("Capture program creation failed: %s", error);
    DeleteShaderDesc(&desc);
#if defined(_WIN32)
    if (root_signature)
        root_signature->Release();
    for (uint32_t i = 0; i < 2; ++i)
        if (bytecode[i])
            bytecode[i]->Release();
#endif
    return program;
}

static bool CreateCaptureResources(HContext context, AdapterFamily family, bool cubemap, CaptureResources* resources)
{
    RenderTargetCreationParams params = {};
    params.m_SampleCount = 1;
    params.m_ColorBufferCreationParams[0].m_Width = CAPTURE_SIZE;
    params.m_ColorBufferCreationParams[0].m_Height = CAPTURE_SIZE;
    params.m_ColorBufferParams[0].m_Width = CAPTURE_SIZE;
    params.m_ColorBufferParams[0].m_Height = CAPTURE_SIZE;
    params.m_ColorBufferParams[0].m_Format = TEXTURE_FORMAT_RGBA;
    params.m_ColorBufferLoadOps[0] = ATTACHMENT_OP_LOAD;
    params.m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
    params.m_DepthBufferCreationParams.m_Width = CAPTURE_SIZE;
    params.m_DepthBufferCreationParams.m_Height = CAPTURE_SIZE;
    params.m_DepthBufferParams.m_Width = CAPTURE_SIZE;
    params.m_DepthBufferParams.m_Height = CAPTURE_SIZE;
    params.m_DepthBufferParams.m_Format = TEXTURE_FORMAT_DEPTH;
    params.m_StencilBufferCreationParams.m_Width = CAPTURE_SIZE;
    params.m_StencilBufferCreationParams.m_Height = CAPTURE_SIZE;
    params.m_StencilBufferParams.m_Width = CAPTURE_SIZE;
    params.m_StencilBufferParams.m_Height = CAPTURE_SIZE;
    params.m_StencilBufferParams.m_Format = TEXTURE_FORMAT_STENCIL;
    resources->m_Target = NewRenderTarget(context, CAPTURE_BUFFERS, params);
    resources->m_Program = NewCaptureProgram(context, family, cubemap);
    if (!resources->m_Target || !resources->m_Program)
        return false;

    const uint32_t rectangle_count = sizeof(CAPTURE_RECTANGLES) / sizeof(CAPTURE_RECTANGLES[0]);
    CaptureVertex vertices[3 + 6 * rectangle_count] = {
        {{-0.75f, -0.625f, 0}, {1.0f, 0.125f, 0.25f}},
        {{0.625f, -0.375f, 0}, {0.125f, 1.0f, 0.375f}},
        {{-0.25f, 0.75f, 0}, {0.25f, 0.375f, 1.0f}},
    };
    for (uint32_t i = 0; i < rectangle_count; ++i)
        MakeRectangleVertices(CAPTURE_RECTANGLES[i], vertices + 3 + 6 * i);
    resources->m_Vertices = cubemap ? CreateCubemapVertices(context) : NewVertexBuffer(context, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
    if (cubemap)
    {
        resources->m_Cubemap = CreateCaptureCubemap(context);
        resources->m_CubemapLocation = INVALID_UNIFORM_LOCATION;
        for (uint32_t i = 0; i < GetUniformCount(resources->m_Program); ++i)
        {
            Uniform uniform;
            GetUniform(resources->m_Program, i, &uniform);
            if (uniform.m_NameHash == dmHashString64("cubemap"))
                resources->m_CubemapLocation = uniform.m_Location;
        }
        if (!resources->m_Cubemap || resources->m_CubemapLocation == INVALID_UNIFORM_LOCATION)
        {
            dmLogError("Cannot create capture cubemap or find its shader binding");
            return false;
        }
    }
    HVertexStreamDeclaration streams = NewVertexStreamDeclaration(context);
    AddVertexStream(streams, "position", 3, TYPE_FLOAT, false);
    AddVertexStream(streams, "color", 3, TYPE_FLOAT, false);
    resources->m_Declaration = NewVertexDeclaration(context, streams);
    DeleteVertexStreamDeclaration(streams);
    return resources->m_Target && resources->m_Program && resources->m_Vertices && resources->m_Declaration;
}

static void DrawStencilValue(HContext context, uint32_t reference, uint32_t mask, uint32_t rectangle)
{
    SetColorMask(context, true, true, true, true);
    SetStencilMask(context, 0);
    SetStencilFunc(context, COMPARE_FUNC_EQUAL, reference, mask);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_KEEP);
    DrawRectangle(context, rectangle);
}

static void RenderNestedStencil(HContext context)
{
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_NESTED_OUTER);
    // Children extend outside their parents: equality must reject those pixels.
    SetStencilFunc(context, COMPARE_FUNC_EQUAL, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_INCR);
    DrawRectangle(context, RECT_NESTED_CHILD);
    SetStencilFunc(context, COMPARE_FUNC_EQUAL, 2, 0xff);
    DrawRectangle(context, RECT_NESTED_INNER);
    DrawStencilValue(context, 1, 0xff, RECT_ORANGE);
    DrawStencilValue(context, 2, 0xff, RECT_GREEN);
    DrawStencilValue(context, 3, 0xff, RECT_BLUE);
}

static void RenderStencilMasks(HContext context)
{
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xa0, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_YELLOW);
    SetStencilMask(context, 0x0f);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xf5, 0xff);
    DrawRectangle(context, RECT_BITS_LEFT);
    SetStencilMask(context, 0xf0);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0x30, 0xff);
    DrawRectangle(context, RECT_BITS_RIGHT);
    // A zero write mask must prevent REPLACE from erasing the result.
    SetStencilMask(context, 0);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0, 0xff);
    DrawRectangle(context, RECT_ORANGE);
    DrawStencilValue(context, 0xa0, 0xff, RECT_YELLOW);
    DrawStencilValue(context, 0xa5, 0xff, RECT_ORANGE);
    DrawStencilValue(context, 0x30, 0xff, RECT_GREEN);
    DrawStencilValue(context, 0x35, 0xff, RECT_BLUE);
    // Both stored value and reference must be masked during comparison.
    DrawStencilValue(context, 0xf5, 0x0f, RECT_BITS_LOW_READ);
    DrawStencilValue(context, 0x3f, 0xf0, RECT_BITS_HIGH_READ);
}

static void RenderStencilOperations(HContext context)
{
    // Row-major tiles: zero, replace, increment, increment-clamp, decrement,
    // decrement-clamp, invert, increment-wrap, decrement-wrap.
    const StencilOp operations[] = {STENCIL_OP_ZERO, STENCIL_OP_REPLACE, STENCIL_OP_INCR,
        STENCIL_OP_INCR, STENCIL_OP_DECR, STENCIL_OP_DECR, STENCIL_OP_INVERT,
        STENCIL_OP_INCR_WRAP, STENCIL_OP_DECR_WRAP};
    const uint8_t initial[] = {0x55, 0x55, 0x7e, 0xff, 2, 0, 0x55, 0xff, 0};
    const uint8_t expected[] = {0, 0xa6, 0x7f, 0xff, 1, 0, 0xaa, 0, 0xff};
    for (uint32_t i = 0; i < sizeof(operations) / sizeof(operations[0]); ++i)
    {
        SetColorMask(context, false, false, false, false);
        SetStencilMask(context, 0xff);
        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, initial[i], 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
        DrawRectangle(context, RECT_OP_FIRST + i);
        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xa6, 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, operations[i]);
        DrawRectangle(context, RECT_OP_FIRST + i);
        DrawStencilValue(context, expected[i], 0xff, RECT_OP_FIRST + i);
    }
}

static void RenderDepthStencil(HContext context)
{
    DisableState(context, STATE_STENCIL_TEST);
    EnableState(context, STATE_DEPTH_TEST);
    SetDepthFunc(context, COMPARE_FUNC_ALWAYS);
    DrawRectangle(context, RECT_DEPTH_OCCLUDER);
    SetDepthMask(context, false);
    SetDepthFunc(context, COMPARE_FUNC_LESS);
    EnableState(context, STATE_STENCIL_TEST);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_INCR, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_DEPTH_NEAR);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 2, 0xff);
    DrawRectangle(context, RECT_DEPTH_FAR);
    // Stencil failure must take precedence even where depth would also fail.
    SetStencilFunc(context, COMPARE_FUNC_NEVER, 3, 0xff);
    SetStencilOp(context, STENCIL_OP_INVERT, STENCIL_OP_ZERO, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_DEPTH_STENCIL_FAIL);
    DisableState(context, STATE_DEPTH_TEST);
    DrawStencilValue(context, 1, 0xff, RECT_ORANGE);
    DrawStencilValue(context, 2, 0xff, RECT_GREEN);
    DrawStencilValue(context, 0xff, 0xff, RECT_BLUE);
}

static void RenderStencilFaces(HContext context)
{
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
    SetStencilOpSeparate(context, FACE_TYPE_FRONT, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    SetStencilOpSeparate(context, FACE_TYPE_BACK, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_INVERT);
    DrawRectangle(context, RECT_FACE_FRONT);
    DrawRectangle(context, RECT_FACE_BACK);
    // Read with identical face state so a swapped front/back implementation
    // cannot cancel its own mistake between writing and reading.
    DrawStencilValue(context, 1, 0xff, RECT_FACE_FRONT);
    DrawStencilValue(context, 0xff, 0xff, RECT_FACE_BACK);
    SetColorMask(context, false, false, false, false);
    SetStencilMask(context, 0xff);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_FACE_COMPARE_FRONT);
    DrawRectangle(context, RECT_FACE_COMPARE_BACK);
    // The graphics API shares reference/read mask between faces; vary only
    // their comparison functions. Wrong face state leaves a rectangle missing.
    SetStencilFuncSeparate(context, FACE_TYPE_FRONT, COMPARE_FUNC_EQUAL, 1, 0xff);
    SetStencilFuncSeparate(context, FACE_TYPE_BACK, COMPARE_FUNC_NOTEQUAL, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_REPLACE, STENCIL_OP_KEEP, STENCIL_OP_INVERT);
    DrawRectangle(context, RECT_FACE_COMPARE_FRONT);
    DrawRectangle(context, RECT_FACE_COMPARE_BACK);
    DrawStencilValue(context, 0xfe, 0xff, RECT_FACE_COMPARE_FRONT);
    DrawStencilValue(context, 1, 0xff, RECT_FACE_COMPARE_BACK);
}

static void RenderCapture(HContext context, const CaptureResources& resources, const char* name)
{
    SetRenderTarget(context, resources.m_Target, 0);
    SetViewport(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE);
    DisableState(context, STATE_BLEND);
    DisableState(context, STATE_DEPTH_TEST);
    DisableState(context, STATE_CULL_FACE);
    DisableState(context, STATE_SCISSOR_TEST);
    DisableState(context, STATE_STENCIL_TEST);
    SetColorMask(context, true, true, true, true);
    SetDepthMask(context, true);
    SetFaceWinding(context, FACE_WINDING_CCW);
    SetStencilMask(context, 0xff);
    Clear(context, CAPTURE_BUFFERS, 37, 73, 109, 255, 1.0f, 0);
    EnableProgram(context, resources.m_Program);
    EnableVertexBuffer(context, resources.m_Vertices, 0);
    EnableVertexDeclaration(context, resources.m_Declaration, 0, 0, resources.m_Program);
    if (strcmp(name, "triangle") == 0)
    {
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
    }
    else if (strcmp(name, "cubemap") == 0)
    {
        SetSampler(context, resources.m_CubemapLocation, 0);
        EnableTexture(context, 0, 0, resources.m_Cubemap);
        Draw(context, PRIMITIVE_TRIANGLES, 0, 36, 1);
        DisableTexture(context, 0, resources.m_Cubemap);
    }
    else if (strncmp(name, "stencil", 7) == 0)
    {
        EnableState(context, STATE_STENCIL_TEST);
        SetColorMask(context, false, false, false, false);
        if (strcmp(name, "stencil_nested") == 0)
            RenderNestedStencil(context);
        else if (strcmp(name, "stencil_masks") == 0)
            RenderStencilMasks(context);
        else if (strcmp(name, "stencil_ops") == 0)
            RenderStencilOperations(context);
        else if (strcmp(name, "stencil_depth") == 0)
            RenderDepthStencil(context);
        else if (strcmp(name, "stencil_faces") == 0)
            RenderStencilFaces(context);
        else
        {
            SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
            SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
            DrawRectangle(context, RECT_BASIC_MASK);
            DrawStencilValue(context, 1, 0xff, RECT_BASIC_FILL);
        }
    }
}

static bool WriteCaptureImage(const char* filename, const uint8_t* pixels)
{
    uint8_t* rgba = (uint8_t*) malloc(CAPTURE_BYTES);
    for (uint32_t i = 0; i < CAPTURE_BYTES; i += 4)
    {
        // ReadPixels promises BGRA; PNG stores RGBA.
        rgba[i] = pixels[i + 2];
        rgba[i + 1] = pixels[i + 1];
        rgba[i + 2] = pixels[i];
        rgba[i + 3] = pixels[i + 3];
    }
    bool written = stbi_write_png(filename, CAPTURE_SIZE, CAPTURE_SIZE, 4, rgba, CAPTURE_SIZE * 4) != 0;
    free(rgba);
    if (!written)
        dmLogError("Cannot write capture image: %s", filename);
    return written;
}

static bool WriteCaptureDiagnostic(const char* filename, const char* suffix, const uint8_t* pixels)
{
    char path[1200];
    dmSnPrintf(path, sizeof(path), "%s.%s.png", filename, suffix);
    return WriteCaptureImage(path, pixels);
}

static bool CapturePixelsEqual(const char* check, const uint8_t* expected, const uint8_t* actual)
{
    for (uint32_t i = 0; i < CAPTURE_BYTES; i += 4)
    {
        if (memcmp(expected + i, actual + i, 4) != 0)
        {
            dmLogError("%s changed at (%u, %u): RGBA (%u, %u, %u, %u) -> (%u, %u, %u, %u)",
                check, (i / 4) % CAPTURE_SIZE, (i / 4) / CAPTURE_SIZE,
                expected[i + 2], expected[i + 1], expected[i], expected[i + 3],
                actual[i + 2], actual[i + 1], actual[i], actual[i + 3]);
            return false;
        }
    }
    return true;
}

static bool CheckReadbackContinuation(HContext context, const CaptureResources& resources, const char* filename)
{
    uint8_t* expected = (uint8_t*) calloc(1, CAPTURE_BYTES);
    uint8_t* actual = (uint8_t*) calloc(1, CAPTURE_BYTES);
    bool valid = true;
    const char* checks[] = {"viewport", "depth-stencil"};
    for (uint32_t check = 0; check < 2; ++check)
    {
        // Compare an uninterrupted draw sequence against the same sequence
        // split by ReadPixels. No state setters or clears follow that split.
        for (uint32_t split = 0; split < 2; ++split)
        {
            RenderCapture(context, resources, "clear");
            if (check == 0)
            {
                SetViewport(context, 0, 0, 128, 192);
                // Consume the viewport's dirty flag before splitting the pass.
                // A pending SetViewport would otherwise hide failed restoration.
                SetColorMask(context, false, false, false, false);
                Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
                SetColorMask(context, true, true, true, true);
            }
            else
            {
                EnableState(context, STATE_DEPTH_TEST);
                EnableState(context, STATE_STENCIL_TEST);
                SetColorMask(context, false, false, false, false);
                SetDepthMask(context, false);
                SetDepthFunc(context, COMPARE_FUNC_ALWAYS);
                SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
                SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
                DrawRectangle(context, RECT_BASIC_MASK);
                SetStencilMask(context, 0);
                SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_KEEP);
                SetDepthMask(context, true);
                DrawRectangle(context, RECT_DEPTH_OCCLUDER);
                SetDepthMask(context, false);
                SetDepthFunc(context, COMPARE_FUNC_LESS);
                SetStencilFunc(context, COMPARE_FUNC_EQUAL, 1, 0xff);
                SetColorMask(context, true, true, true, true);
            }
            if (split)
                ReadPixels(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE, actual, CAPTURE_BYTES);
            if (check == 0)
                Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
            else
                DrawRectangle(context, RECT_DEPTH_FAR);
            ReadPixels(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE, split ? actual : expected, CAPTURE_BYTES);
        }
        if (!CapturePixelsEqual(checks[check], expected, actual))
        {
            char suffix[64];
            dmSnPrintf(suffix, sizeof(suffix), "%s-expected", checks[check]);
            WriteCaptureDiagnostic(filename, suffix, expected);
            dmSnPrintf(suffix, sizeof(suffix), "%s-actual", checks[check]);
            WriteCaptureDiagnostic(filename, suffix, actual);
            valid = false;
        }
    }
    free(actual);
    free(expected);
    return valid;
}

static bool CaptureImage(HContext context, const CaptureResources& resources, const char* name, const char* filename)
{
    const char* diagnostics[] = {"repeated", "viewport-expected", "viewport-actual", "depth-stencil-expected", "depth-stencil-actual"};
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(diagnostics); ++i)
    {
        char path[1200];
        dmSnPrintf(path, sizeof(path), "%s.%s.png", filename, diagnostics[i]);
        remove(path);
    }
    uint8_t* pixels = (uint8_t*) calloc(1, CAPTURE_BYTES);
    uint8_t* repeated = (uint8_t*) calloc(1, CAPTURE_BYTES);
    BeginFrame(context);
    RenderCapture(context, resources, name);
    ReadPixels(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE, pixels, CAPTURE_BYTES);
    // An unaligned row width exercises staging-buffer padding and a nonzero
    // source offset, without changing the image used for likeness scoring.
    uint8_t narrow[13 * CAPTURE_SIZE * 4] = {};
    ReadPixels(context, 67, 0, 13, CAPTURE_SIZE, narrow, sizeof(narrow));
    bool valid = true;
    for (uint32_t row = 0; row < CAPTURE_SIZE; ++row)
        valid = valid && memcmp(narrow + row * 13 * 4, pixels + (row * CAPTURE_SIZE + 67) * 4, 13 * 4) == 0;
    if (!valid)
        dmLogError("Capture subregion readback changed pixels or row ordering");
    // A complete second render also checks that clears remove stale masks.
    if (strncmp(name, "stencil", 7) == 0)
    {
        // Poison pixels outside the mask so the next stencil clear must work.
        SetColorMask(context, false, false, false, false);
        SetStencilMask(context, 0xff);
        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
        DrawRectangle(context, RECT_ORANGE);
    }
    RenderCapture(context, resources, name);
    ReadPixels(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE, repeated, CAPTURE_BYTES);
    if (!CapturePixelsEqual("Repeated render", pixels, repeated))
    {
        WriteCaptureDiagnostic(filename, "repeated", repeated);
        valid = false;
    }
    // The triangle uses the untextured program shared by these continuation probes.
    if (strcmp(name, "triangle") == 0 && !CheckReadbackContinuation(context, resources, filename))
        valid = false;
    SetRenderTarget(context, 0, 0);
    SetColorMask(context, true, true, true, true);
    DisableState(context, STATE_STENCIL_TEST);
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 37, 73, 109, 255, 1.0f, 0);
    Flip(context);
    for (uint32_t i = 0; i < CAPTURE_BYTES; i += 4)
    {
        valid = valid && pixels[i + 3] == 255;
    }
    if (!valid)
        dmLogError("Invalid capture or readback restoration failure");
    bool written = WriteCaptureImage(filename, pixels);
    free(repeated);
    free(pixels);
    return valid && written;
}

// Returns -1 when the original interactive test app should handle the arguments.
int RunGraphicsCapture(int argc, char** argv)
{
    bool selected = false;
    for (int i = 1; i < argc; ++i)
        selected = selected || strncmp(argv[i], "--case", 6) == 0 || strcmp(argv[i], "--list-cases") == 0 ||
            strcmp(argv[i], "--backend") == 0 || strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "--output-file") == 0;
    if (!selected)
        return -1;

    const char* name = 0;
    const char* backend_name = 0;
    const char* directory = 0;
    const char* output_file = 0;
    bool list = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--list-cases") == 0)
        {
            list = true;
            continue;
        }
        const char** value = 0;
        if (strcmp(argv[i], "--case") == 0) value = &name;
        else if (strcmp(argv[i], "--backend") == 0) value = &backend_name;
        else if (strcmp(argv[i], "--output") == 0) value = &directory;
        else if (strcmp(argv[i], "--output-file") == 0) value = &output_file;
        if (!value || *value || i + 1 == argc || strncmp(argv[i + 1], "--", 2) == 0)
        {
            dmLogError("Unknown, repeated, or incomplete capture option: %s", argv[i]);
            return 1;
        }
        *value = argv[++i];
    }
    if (list && !name && !backend_name && !directory && !output_file)
    {
        for (uint32_t i = 0; i < sizeof(CAPTURE_CASES) / sizeof(CAPTURE_CASES[0]); ++i)
            printf("%s\n", CAPTURE_CASES[i]);
        return 0;
    }
    bool known_case = false;
    for (uint32_t i = 0; name && i < sizeof(CAPTURE_CASES) / sizeof(CAPTURE_CASES[0]); ++i)
        known_case = known_case || strcmp(name, CAPTURE_CASES[i]) == 0;
    if (list || !known_case || !backend_name || (directory && output_file))
    {
        dmLogError("Usage: --case <name from --list-cases> --backend metal|opengl|webgpu|vulkan|dx12 [--output directory | --output-file file.png]");
        return 1;
    }
    const CaptureBackend* backend = 0;
    for (uint32_t i = 0; i < sizeof(CAPTURE_BACKENDS) / sizeof(CAPTURE_BACKENDS[0]); ++i)
        if (strcmp(backend_name, CAPTURE_BACKENDS[i].m_Name) == 0)
            backend = &CAPTURE_BACKENDS[i];
    if (!backend || !InstallAdapter(backend->m_Family) || GetInstalledAdapterFamily() != backend->m_Family)
    {
        dmLogError("Requested graphics backend unavailable: %s", backend_name);
        return 1;
    }
    printf("GRAPHICS_CAPTURE_BACKEND=%s\n", backend->m_Name);
    fflush(stdout);
    char filename[1024];
    int length = output_file ? dmSnPrintf(filename, sizeof(filename), "%s", output_file) :
        dmSnPrintf(filename, sizeof(filename), "%s/%s/%s.png", directory ? directory : "graphics-test-images", backend_name, name);
    if (length < 0 || length >= (int) sizeof(filename) || !MakeCaptureParent(filename))
    {
        dmLogError("Cannot create capture output path");
        return 1;
    }
    remove(filename);
    HWindow window = dmPlatform::NewWindow();
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = CAPTURE_SIZE;
    window_params.m_Height = CAPTURE_SIZE;
    window_params.m_Samples = 1;
    window_params.m_Title = "Graphics capture";
    window_params.m_Hidden = 1;
    window_params.m_FocusOnShow = 0;
    window_params.m_GraphicsApi = backend->m_Api;
    window_params.m_OpenGLUseCoreProfileHint = 1;
    window_params.m_GraphicsApiVersionHint = 33;
    if (dmPlatform::OpenWindow(window, window_params) != WINDOW_RESULT_OK)
    {
        dmLogError("Cannot open capture window");
        dmPlatform::DeleteWindow(window);
        return 1;
    }
    ContextParams context_params = {};
    context_params.m_Window = window;
    context_params.m_Width = CAPTURE_SIZE;
    context_params.m_Height = CAPTURE_SIZE;
    context_params.m_VerifyGraphicsCalls = 1;
    context_params.m_PrintDeviceInfo = 1;
    JobSystemCreateParams job_params = {};
    job_params.m_ThreadCount = 1;
    HJobContext jobs = JobSystemCreate(&job_params);
    context_params.m_JobContext = jobs;
#if defined(DM_VULKAN_VALIDATION)
    context_params.m_UseValidationLayers = 1;
#endif
    HContext context = NewContext(context_params);
    if (!context)
    {
        dmLogError("Cannot create capture context");
        dmPlatform::CloseWindow(window);
        dmPlatform::DeleteWindow(window);
        JobSystemDestroy(jobs);
        return 1;
    }
    CaptureResources resources = {};
    bool success = CreateCaptureResources(context, backend->m_Family, strcmp(name, "cubemap") == 0, &resources);
    if (success)
        success = CaptureImage(context, resources, name, filename);
    if (resources.m_Declaration) DeleteVertexDeclaration(resources.m_Declaration);
    if (resources.m_Vertices) DeleteVertexBuffer(resources.m_Vertices);
    if (resources.m_Program) DeleteProgram(context, resources.m_Program);
    if (resources.m_Cubemap) DeleteTexture(context, resources.m_Cubemap);
    if (resources.m_Target) DeleteRenderTarget(context, resources.m_Target);
    CloseWindow(context);
    DeleteContext(context);
    Finalize();
    JobSystemDestroy(jobs);
    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
    return success ? 0 : 1;
}
