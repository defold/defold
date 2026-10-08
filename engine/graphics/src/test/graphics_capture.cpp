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

#include <stdio.h>
#include <string.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <platform/window.hpp>
#include "test_app_graphics.h"
#include "graphics_capture.vp.h"
#include "graphics_capture.fp.h"
#include "graphics_capture.cube.fp.h"
#include "graphics_capture.vp.msl.h"
#include "graphics_capture.fp.msl.h"
#include "graphics_capture.cube.fp.msl.h"
#include "graphics_capture.vp.wgsl.h"
#include "graphics_capture.fp.wgsl.h"
#include "graphics_capture.cube.fp.wgsl.h"
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
#include "graphics_capture.vert.spv.h"
#include "graphics_capture.frag.spv.h"
#include "graphics_capture.cube.frag.spv.h"
#endif

#define STB_IMAGE_WRITE_STATIC
#define STBIWDEF static inline
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

using namespace dmGraphics;

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
    uint32_t m_Left;
    uint32_t m_Top;
    uint32_t m_Right;
    uint32_t m_Bottom;
    float    m_Depth;
    uint8_t  m_Red;
    uint8_t  m_Green;
    uint8_t  m_Blue;
    bool     m_Clockwise;
};

struct CaptureBackend
{
    const char*        m_Name;
    AdapterFamily      m_Family;
    WindowsGraphicsApi m_Api;
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

struct CaptureContext
{
    HWindow     m_Window;
    HContext    m_Context;
    HJobContext m_JobContext;
    bool        m_WindowOpened;
};

struct CaptureStencilOperation
{
    StencilOp m_Operation;
    uint8_t   m_Initial;
    uint8_t   m_Expected;
};

static const uint32_t CAPTURE_SIZE = 256;
static const uint32_t CAPTURE_BYTES = CAPTURE_SIZE * CAPTURE_SIZE * 4;
static const uint32_t CAPTURE_BUFFERS = BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT;

// Each case renders to an offscreen target. run_graphics_images.py compares its
// PNG with a reviewed reference; the comments below describe the expected image.
static const char*    CAPTURE_CASES[] = {
    "clear",
    "triangle",
    "stencil",
    "stencil_nested",
    "stencil_masks",
    "stencil_ops",
    "stencil_depth",
    "stencil_faces",
    "cubemap",
};

static const CaptureBackend CAPTURE_BACKENDS[] = {
    { "metal", ADAPTER_FAMILY_METAL, WINDOW_GRAPHICS_API_METAL },
    { "opengl", ADAPTER_FAMILY_OPENGL, WINDOW_GRAPHICS_API_OPENGL },
    { "vulkan", ADAPTER_FAMILY_VULKAN, WINDOW_GRAPHICS_API_VULKAN },
    { "webgpu", ADAPTER_FAMILY_WEBGPU, WINDOW_GRAPHICS_API_WEBGPU },
};

// Pixel coordinates are top-down. Depth is normalized to [0, 1] by every shader.
static const CaptureRectangleData CAPTURE_RECTANGLES[] = {
    { 64, 48, 160, 160, 0, 255, 255, 255, false },
    { 32, 32, 224, 208, 0, 223, 96, 32, false },
    { 16, 16, 240, 240, 0, 223, 96, 32, false },
    { 16, 16, 240, 240, 0, 32, 191, 96, false },
    { 16, 16, 240, 240, 0, 32, 96, 223, false },
    { 16, 16, 240, 240, 0, 223, 191, 32, false },
    { 32, 32, 192, 208, 0, 255, 255, 255, false },
    { 96, 64, 224, 176, 0, 255, 255, 255, false },
    { 128, 96, 208, 144, 0, 255, 255, 255, false },
    { 32, 32, 144, 224, 0, 255, 255, 255, false },
    { 96, 64, 224, 192, 0, 255, 255, 255, false },
    { 16, 112, 240, 128, 0, 223, 32, 191, false },
    { 16, 144, 240, 160, 0, 32, 191, 223, false },
    { 96, 32, 160, 224, 0.25f, 255, 255, 255, false },
    { 32, 64, 224, 112, 0.125f, 255, 255, 255, false },
    { 32, 144, 224, 192, 0.75f, 255, 255, 255, false },
    { 32, 208, 224, 224, 0.75f, 255, 255, 255, false },
    { 32, 32, 112, 112, 0, 223, 96, 32, false },
    { 144, 32, 224, 112, 0, 32, 191, 223, true },
    { 32, 144, 112, 224, 0, 223, 32, 191, false },
    { 144, 144, 224, 224, 0, 32, 191, 96, true },
    { 24, 24, 72, 72, 0, 32, 191, 96, false },
    { 96, 24, 144, 72, 0, 32, 191, 96, false },
    { 168, 24, 216, 72, 0, 32, 191, 96, false },
    { 24, 96, 72, 144, 0, 32, 191, 96, false },
    { 96, 96, 144, 144, 0, 32, 191, 96, false },
    { 168, 96, 216, 144, 0, 32, 191, 96, false },
    { 24, 168, 72, 216, 0, 32, 191, 96, false },
    { 96, 168, 144, 216, 0, 32, 191, 96, false },
    { 168, 168, 216, 216, 0, 32, 191, 96, false },
};

static void MakeRectangleVertices(const CaptureRectangleData& rectangle, CaptureVertex* vertices)
{
    const uint32_t x[] = { rectangle.m_Left, rectangle.m_Right, rectangle.m_Left, rectangle.m_Right, rectangle.m_Right, rectangle.m_Left };
    const uint32_t y[] = { rectangle.m_Bottom, rectangle.m_Bottom, rectangle.m_Top, rectangle.m_Bottom, rectangle.m_Top, rectangle.m_Top };
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

static HTexture CreateCaptureCubemap(HContext context)
{
    const uint32_t size = 16;
    // Upload order: +X, -X, +Y, -Y, +Z, -Z. Labels and unequal corner
    // markers make face swaps, rotation and mirroring visible in the cross.
    const uint8_t colors[6][3] = {
        { 208, 64, 64 },
        { 64, 176, 96 },
        { 72, 104, 208 },
        { 208, 176, 48 },
        { 48, 176, 192 },
        { 176, 64, 192 },
    };
    const uint8_t signs[2][5] = { { 2, 2, 7, 2, 2 }, { 0, 0, 7, 0, 0 } };
    const uint8_t axes[3][5] = { { 5, 5, 2, 5, 5 }, { 5, 5, 2, 2, 2 }, { 7, 1, 2, 4, 7 } };
    uint8_t       pixels[6 * size * size * 4];
    for (uint32_t face = 0; face < 6; ++face)
    {
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                bool border = x == 0 || y == 0 || x == size - 1 || y == size - 1;
                bool dark = border || (x >= 11 && x <= 13 && y >= 12 && y <= 13);
                bool white = (y == 2 && x >= 2 && x <= 5) || (x == 2 && y >= 2 && y <= 4);
                if (y >= 6 && y < 11)
                {
                    if (x >= 4 && x < 7)
                    {
                        white = (signs[face % 2][y - 6] & (1 << (6 - x))) != 0;
                    }
                    if (x >= 8 && x < 11)
                    {
                        white = (axes[face / 2][y - 6] & (1 << (10 - x))) != 0;
                    }
                }
                uint8_t* pixel = pixels + ((face * size + y) * size + x) * 4;
                for (uint32_t channel = 0; channel < 3; ++channel)
                {
                    if (white)
                    {
                        pixel[channel] = 255;
                    }
                    else if (dark)
                    {
                        pixel[channel] = 24;
                    }
                    else
                    {
                        pixel[channel] = colors[face][channel];
                    }
                }
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
    {
        return 0;
    }

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
    const uint32_t columns[] = { 2, 0, 1, 1, 1, 3 };
    const uint32_t rows[] = { 1, 1, 0, 2, 1, 1 };
    CaptureVertex  vertices[6 * 6];
    for (uint32_t face = 0; face < 6; ++face)
    {
        uint32_t             left = 32 + 48 * columns[face];
        uint32_t             top = 56 + 48 * rows[face];
        CaptureRectangleData rectangle = { left, top, left + 48, top + 48, 0, 0, 0, 0, false };
        MakeRectangleVertices(rectangle, vertices + face * 6);
        for (uint32_t corner = 0; corner < 6; ++corner)
        {
            CaptureVertex& vertex = vertices[face * 6 + corner];
            float          s = ((vertex.m_Position[0] + 1) * (CAPTURE_SIZE / 2) - left) / 24 - 1;
            float          t = ((1 - vertex.m_Position[1]) * (CAPTURE_SIZE / 2) - top) / 24 - 1;
            const float    directions[6][3] = {
                { 1, -t, -s },
                { -1, -t, s },
                { s, 1, t },
                { s, -1, -t },
                { s, -t, 1 },
                { -s, -t, -1 },
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
    {
        return false;
    }
    for (char* cursor = path + 1; *cursor; ++cursor)
    {
        if (*cursor != '/' && *cursor != '\\')
        {
            continue;
        }
        char separator = *cursor;
        *cursor = 0;
        dmSys::Result result = dmSys::Mkdir(path, 0755);
        *cursor = separator;
        if (result != dmSys::RESULT_OK && result != dmSys::RESULT_EXIST)
        {
            return false;
        }
    }
    return true;
}

static HProgram NewCaptureProgram(HContext context, AdapterFamily family, bool cubemap)
{
    ShaderDesc desc = {};
    AddShader(&desc, ShaderDesc::LANGUAGE_GLSL_SM330, ShaderDesc::SHADER_TYPE_VERTEX, capture_vp, capture_vp_SIZE);
    AddShader(&desc, ShaderDesc::LANGUAGE_MSL_22, ShaderDesc::SHADER_TYPE_VERTEX, capture_vp_msl, capture_vp_msl_SIZE);
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
    AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_VERTEX, capture_vert_spv, capture_vert_spv_SIZE);
#endif
    AddShader(&desc, ShaderDesc::LANGUAGE_WGSL, ShaderDesc::SHADER_TYPE_VERTEX, capture_vp_wgsl, capture_vp_wgsl_SIZE);
    if (cubemap)
    {
        AddShader(&desc, ShaderDesc::LANGUAGE_GLSL_SM330, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_cube_fp, capture_cube_fp_SIZE);
        AddShader(&desc, ShaderDesc::LANGUAGE_MSL_22, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_cube_fp_msl, capture_cube_fp_msl_SIZE);
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_cube_frag_spv, capture_cube_frag_spv_SIZE);
#endif
        AddShader(&desc, ShaderDesc::LANGUAGE_WGSL, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_cube_fp_wgsl, capture_cube_fp_wgsl_SIZE);
    }
    else
    {
        AddShader(&desc, ShaderDesc::LANGUAGE_GLSL_SM330, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_fp, capture_fp_SIZE);
        AddShader(&desc, ShaderDesc::LANGUAGE_MSL_22, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_fp_msl, capture_fp_msl_SIZE);
#if defined(DM_TEST_APP_GRAPHICS_HAS_VULKAN)
        AddShader(&desc, ShaderDesc::LANGUAGE_SPIRV, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_frag_spv, capture_frag_spv_SIZE);
#endif
        AddShader(&desc, ShaderDesc::LANGUAGE_WGSL, ShaderDesc::SHADER_TYPE_FRAGMENT, capture_fp_wgsl, capture_fp_wgsl_SIZE);
    }

    // NewProgram copies these borrowed mappings before this function returns.
    ShaderDesc::MSLResourceMapping metal_bindings[2] = {};
    if (cubemap)
    {
        ShaderDesc::ShaderDataType texture_type = ShaderDesc::SHADER_TYPE_TEXTURE_CUBE;
        if (family == ADAPTER_FAMILY_OPENGL)
        {
            texture_type = ShaderDesc::SHADER_TYPE_SAMPLER_CUBE;
        }
        AddShaderResource(&desc, "cubemap", texture_type, 0, 0, BINDING_TYPE_TEXTURE, SHADER_STAGE_FLAG_FRAGMENT);
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

    AddShaderResource(&desc, "position", ShaderDesc::SHADER_TYPE_VEC3, 0, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
    AddShaderResource(&desc, "color", ShaderDesc::SHADER_TYPE_VEC3, 1, 0, BINDING_TYPE_INPUT, SHADER_STAGE_FLAG_VERTEX);
    char     error[1024] = {};
    HProgram program = NewProgram(context, &desc, error, sizeof(error));
    if (!program)
    {
        dmLogError("Capture program creation failed: %s", error);
    }
    DeleteShaderDesc(&desc);

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
    {
        return false;
    }

    const uint32_t rectangle_count = DM_ARRAY_SIZE(CAPTURE_RECTANGLES);
    CaptureVertex  vertices[3 + 6 * rectangle_count] = {
        { { -0.75f, -0.625f, 0 }, { 1.0f, 0.125f, 0.25f } },
        { { 0.625f, -0.375f, 0 }, { 0.125f, 1.0f, 0.375f } },
        { { -0.25f, 0.75f, 0 }, { 0.25f, 0.375f, 1.0f } },
    };
    for (uint32_t i = 0; i < rectangle_count; ++i)
    {
        MakeRectangleVertices(CAPTURE_RECTANGLES[i], vertices + 3 + 6 * i);
    }
    if (cubemap)
    {
        resources->m_Vertices = CreateCubemapVertices(context);
        resources->m_Cubemap = CreateCaptureCubemap(context);
        resources->m_CubemapLocation = INVALID_UNIFORM_LOCATION;
        for (uint32_t i = 0; i < GetUniformCount(resources->m_Program); ++i)
        {
            Uniform uniform;
            GetUniform(resources->m_Program, i, &uniform);
            if (uniform.m_NameHash == dmHashString64("cubemap"))
            {
                resources->m_CubemapLocation = uniform.m_Location;
            }
        }
        if (!resources->m_Cubemap || resources->m_CubemapLocation == INVALID_UNIFORM_LOCATION)
        {
            dmLogError("Cannot create capture cubemap or find its shader binding");
            return false;
        }
    }
    else
    {
        resources->m_Vertices = NewVertexBuffer(context, sizeof(vertices), vertices, BUFFER_USAGE_STATIC_DRAW);
    }

    HVertexStreamDeclaration streams = NewVertexStreamDeclaration(context);
    AddVertexStream(streams, "position", 3, TYPE_FLOAT, false);
    AddVertexStream(streams, "color", 3, TYPE_FLOAT, false);
    resources->m_Declaration = NewVertexDeclaration(context, streams);
    DeleteVertexStreamDeclaration(streams);
    return resources->m_Target && resources->m_Program && resources->m_Vertices && resources->m_Declaration;
}

static void DeleteCaptureResources(HContext context, CaptureResources* resources)
{
    if (resources->m_Declaration)
    {
        DeleteVertexDeclaration(resources->m_Declaration);
    }
    if (resources->m_Vertices)
    {
        DeleteVertexBuffer(resources->m_Vertices);
    }
    if (resources->m_Program)
    {
        DeleteProgram(context, resources->m_Program);
    }
    if (resources->m_Cubemap)
    {
        DeleteTexture(context, resources->m_Cubemap);
    }
    if (resources->m_Target)
    {
        DeleteRenderTarget(context, resources->m_Target);
    }
}

// Reveal a stored stencil value with a colored rectangle without changing it.
// The stencil cases use this to turn otherwise invisible mask errors into pixels.
static void DrawStencilValue(HContext context, uint32_t reference, uint32_t mask, uint32_t rectangle)
{
    SetColorMask(context, true, true, true, true);
    SetStencilMask(context, 0);
    SetStencilFunc(context, COMPARE_FUNC_EQUAL, reference, mask);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_KEEP);
    DrawRectangle(context, rectangle);
}

// Verifies nested masks clip their children, including child geometry outside the parent.
// Write outer level 1, then increment accepted child pixels to levels 2 and 3.
// Expect nested orange, green and blue regions; color outside a parent exposes
// an incorrect comparison or an increment applied to rejected pixels.
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

// Verifies masked writes preserve other bits and comparisons mask both stored and reference values.
// Start at 0xa0, write low bits on the left and high bits on the right. Expect
// 0xa5 on the left, 0x30 on the right and 0x35 in their overlap, revealed as
// orange, green and blue on yellow. Magenta/cyan strips then check masked reads.
static void RenderStencilMasks(HContext context)
{
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xa0, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_YELLOW);

    // Only the low nibble may change: the reference's high bits must be ignored.
    SetStencilMask(context, 0x0f);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xf5, 0xff);
    DrawRectangle(context, RECT_BITS_LEFT);

    // Preserve those low bits in the overlap while replacing the high nibble.
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

// Verifies each stencil operation, including saturation and wrap at the byte limits.
// Seed each tile with its initial value, apply the operation, then draw green
// only where the result equals the expected value. All nine tiles must appear;
// a missing tile identifies the failing operation or clamp/wrap boundary.
static void RenderStencilOperations(HContext context)
{
    // Row-major tiles: zero, replace, increment, increment-clamp, decrement,
    // decrement-clamp, invert, increment-wrap, decrement-wrap.
    const CaptureStencilOperation operations[] = {
        { STENCIL_OP_ZERO,      0x55, 0    },
        { STENCIL_OP_REPLACE,   0x55, 0xa6 },
        { STENCIL_OP_INCR,      0x7e, 0x7f },
        { STENCIL_OP_INCR,      0xff, 0xff },
        { STENCIL_OP_DECR,      2,    1    },
        { STENCIL_OP_DECR,      0,    0    },
        { STENCIL_OP_INVERT,    0x55, 0xaa },
        { STENCIL_OP_INCR_WRAP, 0xff, 0    },
        { STENCIL_OP_DECR_WRAP, 0,    0xff },
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(operations); ++i)
    {
        SetColorMask(context, false, false, false, false);
        SetStencilMask(context, 0xff);
        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, operations[i].m_Initial, 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
        DrawRectangle(context, RECT_OP_FIRST + i);

        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 0xa6, 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, operations[i].m_Operation);
        DrawRectangle(context, RECT_OP_FIRST + i);

        DrawStencilValue(context, operations[i].m_Expected, 0xff, RECT_OP_FIRST + i);
    }
}

// Verifies depth-fail and stencil-fail operations with back-face culling enabled on an offscreen target.
// An invisible depth occluder crosses three horizontal bands. Expect an orange
// near band, a green far band with an orange center, and a blue stencil-fail band.
// This distinguishes pass, depth-fail and stencil-fail operations while also
// checking that the production offscreen winding survives back-face culling.
static void RenderDepthStencil(HContext context)
{
    EnableState(context, STATE_CULL_FACE);
    SetCullFace(context, FACE_TYPE_BACK);
    DisableState(context, STATE_STENCIL_TEST);
    EnableState(context, STATE_DEPTH_TEST);
    SetDepthFunc(context, COMPARE_FUNC_ALWAYS);
    DrawRectangle(context, RECT_DEPTH_OCCLUDER);

    // The near band passes everywhere and stores 1. The far band stores 2
    // outside the occluder; behind it, depth failure increments zero to 1.
    SetDepthMask(context, false);
    SetDepthFunc(context, COMPARE_FUNC_LESS);
    EnableState(context, STATE_STENCIL_TEST);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_INCR, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_DEPTH_NEAR);
    SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 2, 0xff);
    DrawRectangle(context, RECT_DEPTH_FAR);

    // Stencil failure must take precedence even where depth would also fail.
    // INVERT produces 0xff across the entire bottom band. Choosing the depth-fail
    // ZERO operation instead would leave a gap in its blue readout.
    SetStencilFunc(context, COMPARE_FUNC_NEVER, 3, 0xff);
    SetStencilOp(context, STENCIL_OP_INVERT, STENCIL_OP_ZERO, STENCIL_OP_REPLACE);
    DrawRectangle(context, RECT_DEPTH_STENCIL_FAIL);

    DisableState(context, STATE_DEPTH_TEST);
    DrawStencilValue(context, 1, 0xff, RECT_ORANGE);
    DrawStencilValue(context, 2, 0xff, RECT_GREEN);
    DrawStencilValue(context, 0xff, 0xff, RECT_BLUE);
}

// Verifies separate front/back state using production shader orientation; guards against swapped face assignments.
// Opposite windings must select different operations (top pair) and comparisons
// (bottom pair). Expect orange/cyan above magenta/green; swapped face state or
// accidentally reusing the front-face settings makes one or more tiles disappear.
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
    // Both tiles contain 1: front EQUAL passes and inverts it to 0xfe, while
    // back NOTEQUAL fails and replaces it with 1. Read both with common state.
    SetStencilFuncSeparate(context, FACE_TYPE_FRONT, COMPARE_FUNC_EQUAL, 1, 0xff);
    SetStencilFuncSeparate(context, FACE_TYPE_BACK, COMPARE_FUNC_NOTEQUAL, 1, 0xff);
    SetStencilOp(context, STENCIL_OP_REPLACE, STENCIL_OP_KEEP, STENCIL_OP_INVERT);
    DrawRectangle(context, RECT_FACE_COMPARE_FRONT);
    DrawRectangle(context, RECT_FACE_COMPARE_BACK);

    DrawStencilValue(context, 0xfe, 0xff, RECT_FACE_COMPARE_FRONT);
    DrawStencilValue(context, 1, 0xff, RECT_FACE_COMPARE_BACK);
}

// Reset the target and draw one case. Stencil cases first build masks with color
// writes disabled, then reveal their values through DrawStencilValue.
static void RenderCapture(HContext context, const CaptureResources& resources, const char* name)
{
    SetRenderTarget(context, resources.m_Target, RenderTargetBindingParams());
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
    // The "clear" case issues no draw: expect opaque RGB (37, 73, 109) everywhere.
    // Unequal channels expose RGBA/BGRA swaps, and readback must flush a deferred
    // clear even when no draw has started a render pass.
    Clear(context, CAPTURE_BUFFERS, 37, 73, 109, 255, 1.0f, 0);

    EnableProgram(context, resources.m_Program);
    EnableVertexBuffer(context, resources.m_Vertices, 0);
    EnableVertexDeclaration(context, resources.m_Declaration, 0, 0, resources.m_Program);

    if (strcmp(name, "triangle") == 0)
    {
        // Asymmetric positions and interpolated colors expose flipped coordinates,
        // vertex-layout errors and channel swaps: blue above, red left, green right.
        Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
    }
    else if (strcmp(name, "cubemap") == 0)
    {
        // Sampling the uploaded cubemap by direction checks face order, binding and
        // orientation. Expect +Y above; -X, +Z, +X, -Z across; and -Y below. Labels
        // and unequal corner markers expose swapped, rotated or mirrored faces.
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
        {
            RenderNestedStencil(context);
        }
        else if (strcmp(name, "stencil_masks") == 0)
        {
            RenderStencilMasks(context);
        }
        else if (strcmp(name, "stencil_ops") == 0)
        {
            RenderStencilOperations(context);
        }
        else if (strcmp(name, "stencil_depth") == 0)
        {
            RenderDepthStencil(context);
        }
        else if (strcmp(name, "stencil_faces") == 0)
        {
            RenderStencilFaces(context);
        }
        else
        {
            // Basic stencil: write 1 into an invisible mask, then draw a larger
            // orange rectangle through EQUAL. Only their intersection may appear;
            // the surrounding pixels must retain the clear color.
            SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
            SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
            DrawRectangle(context, RECT_BASIC_MASK);
            DrawStencilValue(context, 1, 0xff, RECT_BASIC_FILL);
        }
    }
}

static bool WriteCaptureImage(const char* filename, const uint8_t* pixels)
{
    dmArray<uint8_t> rgba;
    rgba.SetCapacity(CAPTURE_BYTES);
    rgba.SetSize(CAPTURE_BYTES);

    // Metal, Vulkan and WebGPU follow the production offscreen orientation.
    // Normalize rows only when exporting the image, preserving rasterizer winding.
    bool flip_y = GetInstalledAdapterFamily() != ADAPTER_FAMILY_OPENGL;
    for (uint32_t y = 0; y < CAPTURE_SIZE; ++y)
    {
        const uint8_t* source = pixels + (flip_y ? CAPTURE_SIZE - 1 - y : y) * CAPTURE_SIZE * 4;
        uint8_t*       destination = rgba.Begin() + y * CAPTURE_SIZE * 4;
        for (uint32_t x = 0; x < CAPTURE_SIZE * 4; x += 4)
        {
            // ReadPixels promises BGRA; PNG stores RGBA.
            destination[x] = source[x + 2];
            destination[x + 1] = source[x + 1];
            destination[x + 2] = source[x];
            destination[x + 3] = source[x + 3];
        }
    }

    bool written = stbi_write_png(filename, CAPTURE_SIZE, CAPTURE_SIZE, 4, rgba.Begin(), CAPTURE_SIZE * 4) != 0;
    if (!written)
    {
        dmLogError("Cannot write capture image: %s", filename);
    }
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
                       check,
                       (i / 4) % CAPTURE_SIZE,
                       (i / 4) / CAPTURE_SIZE,
                       expected[i + 2],
                       expected[i + 1],
                       expected[i],
                       expected[i + 3],
                       actual[i + 2],
                       actual[i + 1],
                       actual[i],
                       actual[i + 3]);
            return false;
        }
    }
    return true;
}

static void ReadCapturePixels(HContext context, dmArray<uint8_t>& pixels)
{
    pixels.SetCapacity(CAPTURE_BYTES);
    pixels.SetSize(CAPTURE_BYTES);
    memset(pixels.Begin(), 0, pixels.Size());
    ReadPixels(context, 0, 0, CAPTURE_SIZE, CAPTURE_SIZE, pixels.Begin(), pixels.Size());
}

// Verifies readback preserves the active viewport and depth/stencil contents for subsequent draws.
// Compare exact pixels with an uninterrupted draw sequence. The viewport probe
// catches a changed triangle position/scale; the depth/stencil probe catches a
// lost mask or occluder when ReadPixels ends and resumes the render pass.
static bool CheckReadbackContinuation(HContext context, const CaptureResources& resources, const char* filename)
{
    dmArray<uint8_t> expected;
    dmArray<uint8_t> actual;
    bool             valid = true;
    const char*      checks[] = { "viewport", "depth-stencil" };
    for (uint32_t check = 0; check < DM_ARRAY_SIZE(checks); ++check)
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
                // Prepare an invisible mask and nearer occluder. After readback,
                // the far rectangle must still be rejected outside the mask and
                // behind the occluder, without rebinding or restoring any state.
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
            {
                ReadCapturePixels(context, actual);
            }
            if (check == 0)
            {
                Draw(context, PRIMITIVE_TRIANGLES, 0, 3, 1);
            }
            else
            {
                DrawRectangle(context, RECT_DEPTH_FAR);
            }
            ReadCapturePixels(context, split ? actual : expected);
        }
        if (!CapturePixelsEqual(checks[check], expected.Begin(), actual.Begin()))
        {
            char suffix[64];
            dmSnPrintf(suffix, sizeof(suffix), "%s-expected", checks[check]);
            WriteCaptureDiagnostic(filename, suffix, expected.Begin());
            dmSnPrintf(suffix, sizeof(suffix), "%s-actual", checks[check]);
            WriteCaptureDiagnostic(filename, suffix, actual.Begin());
            valid = false;
        }
    }
    return valid;
}

// Verifies deterministic repeated rendering and subregion readback; guards against stale clear pipeline state.
// Every case must match a second render in the same frame, and a narrow readback
// must exactly match its slice of the full image. These checks can fail even when
// the first PNG matches its reference, so they also contribute to the exit status.
static bool CaptureImage(HContext context, const CaptureResources& resources, const char* name, const char* filename)
{
    const char* diagnostics[] = { "repeated", "viewport-expected", "viewport-actual", "depth-stencil-expected", "depth-stencil-actual" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(diagnostics); ++i)
    {
        char path[1200];
        dmSnPrintf(path, sizeof(path), "%s.%s.png", filename, diagnostics[i]);
        remove(path);
    }
    dmArray<uint8_t> pixels;
    dmArray<uint8_t> repeated;
    BeginFrame(context);
    RenderCapture(context, resources, name);
    ReadCapturePixels(context, pixels);

    // An unaligned row width exercises staging-buffer padding and a nonzero
    // source offset, without changing the image used for likeness scoring.
    uint8_t narrow[13 * CAPTURE_SIZE * 4] = {};
    ReadPixels(context, 67, 0, 13, CAPTURE_SIZE, narrow, sizeof(narrow));
    bool valid = true;
    for (uint32_t row = 0; row < CAPTURE_SIZE; ++row)
    {
        const uint8_t* actual_row = narrow + row * 13 * 4;
        const uint8_t* expected_row = pixels.Begin() + (row * CAPTURE_SIZE + 67) * 4;
        valid = valid && memcmp(actual_row, expected_row, 13 * 4) == 0;
    }
    if (!valid)
    {
        dmLogError("Capture subregion readback changed pixels or row ordering");
    }

    // A complete second render also checks that clears remove stale masks.
    if (strncmp(name, "stencil", 7) == 0)
    {
        // Poison pixels outside the mask so the next stencil clear must work.
        // The following draw must also restore its pipeline after the clear's
        // internal draw; a stale Metal pipeline cache previously lost these masks.
        SetColorMask(context, false, false, false, false);
        SetStencilMask(context, 0xff);
        SetStencilFunc(context, COMPARE_FUNC_ALWAYS, 1, 0xff);
        SetStencilOp(context, STENCIL_OP_KEEP, STENCIL_OP_KEEP, STENCIL_OP_REPLACE);
        DrawRectangle(context, RECT_ORANGE);
    }
    RenderCapture(context, resources, name);
    ReadCapturePixels(context, repeated);
    if (!CapturePixelsEqual("Repeated render", pixels.Begin(), repeated.Begin()))
    {
        WriteCaptureDiagnostic(filename, "repeated", repeated.Begin());
        valid = false;
    }

    // The triangle uses the untextured program shared by these continuation probes.
    if (strcmp(name, "triangle") == 0 && !CheckReadbackContinuation(context, resources, filename))
    {
        valid = false;
    }

    SetRenderTarget(context, 0, RenderTargetBindingParams());
    SetColorMask(context, true, true, true, true);
    DisableState(context, STATE_STENCIL_TEST);
    Clear(context, BUFFER_TYPE_COLOR0_BIT, 37, 73, 109, 255, 1.0f, 0);
    Flip(context);

    // Opaque alpha also rejects a readback that leaves its zeroed buffer untouched.
    for (uint32_t i = 0; i < CAPTURE_BYTES; i += 4)
    {
        valid = valid && pixels[i + 3] == 255;
    }
    if (!valid)
    {
        dmLogError("Invalid capture or readback restoration failure");
    }
    bool written = WriteCaptureImage(filename, pixels.Begin());
    return valid && written;
}

static bool InitializeCapture(const CaptureBackend& backend, CaptureContext* capture)
{
    capture->m_Window = dmPlatform::NewWindow();

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = CAPTURE_SIZE;
    window_params.m_Height = CAPTURE_SIZE;
    window_params.m_Samples = 1;
    window_params.m_Title = "Graphics capture";
    window_params.m_Hidden = 1;
    window_params.m_FocusOnShow = 0;
    window_params.m_GraphicsApi = backend.m_Api;
    window_params.m_OpenGLUseCoreProfileHint = 1;
    window_params.m_GraphicsApiVersionHint = 33;
    if (dmPlatform::OpenWindow(capture->m_Window, window_params) != WINDOW_RESULT_OK)
    {
        dmLogError("Cannot open capture window");
        return false;
    }

    capture->m_WindowOpened = true;

    ContextParams context_params = {};
    context_params.m_Window = capture->m_Window;
    context_params.m_Width = CAPTURE_SIZE;
    context_params.m_Height = CAPTURE_SIZE;
    context_params.m_VerifyGraphicsCalls = 1;
    context_params.m_PrintDeviceInfo = 1;

    JobSystemCreateParams job_params = {};
    job_params.m_ThreadCount = 1;
    capture->m_JobContext = JobSystemCreate(&job_params);
    context_params.m_JobContext = capture->m_JobContext;
#if defined(DM_VULKAN_VALIDATION)
    context_params.m_UseValidationLayers = 1;
#endif
    capture->m_Context = NewContext(context_params);
    if (!capture->m_Context)
    {
        dmLogError("Cannot create capture context");
        return false;
    }

    return true;
}

static void FinalizeCapture(CaptureContext* capture)
{
    if (capture->m_Context)
    {
        CloseWindow(capture->m_Context);
        DeleteContext(capture->m_Context);
        // WebGPU closes the platform window in CloseWindow(context).
        if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_WEBGPU)
        {
            capture->m_WindowOpened = false;
        }
    }
    Finalize();
    if (capture->m_JobContext)
    {
        JobSystemDestroy(capture->m_JobContext);
    }
    if (capture->m_WindowOpened)
    {
        dmPlatform::CloseWindow(capture->m_Window);
    }
    dmPlatform::DeleteWindow(capture->m_Window);
}

// Returns -1 when the original interactive test app should handle the arguments.
int RunGraphicsCapture(int argc, char** argv)
{
    bool selected = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strncmp(argv[i], "--case", 6) == 0 ||
            strcmp(argv[i], "--list-cases") == 0 ||
            strcmp(argv[i], "--backend") == 0 ||
            strcmp(argv[i], "--output") == 0 ||
            strcmp(argv[i], "--output-file") == 0)
        {
            selected = true;
            break;
        }
    }
    if (!selected)
    {
        return -1;
    }

    const char* name = 0;
    const char* backend_name = 0;
    const char* directory = 0;
    const char* output_file = 0;
    bool        list = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--list-cases") == 0)
        {
            list = true;
            continue;
        }
        const char** value = 0;
        if (strcmp(argv[i], "--case") == 0)
        {
            value = &name;
        }
        else if (strcmp(argv[i], "--backend") == 0)
        {
            value = &backend_name;
        }
        else if (strcmp(argv[i], "--output") == 0)
        {
            value = &directory;
        }
        else if (strcmp(argv[i], "--output-file") == 0)
        {
            value = &output_file;
        }
        if (!value || *value || i + 1 == argc || strncmp(argv[i + 1], "--", 2) == 0)
        {
            dmLogError("Unknown, repeated, or incomplete capture option: %s", argv[i]);
            return 1;
        }
        *value = argv[++i];
    }
    if (list && !name && !backend_name && !directory && !output_file)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(CAPTURE_CASES); ++i)
        {
            printf("%s\n", CAPTURE_CASES[i]);
        }
        return 0;
    }
    bool known_case = false;
    for (uint32_t i = 0; name && i < DM_ARRAY_SIZE(CAPTURE_CASES); ++i)
    {
        if (strcmp(name, CAPTURE_CASES[i]) == 0)
        {
            known_case = true;
            break;
        }
    }
    if (list || !known_case || !backend_name || (directory && output_file))
    {
        dmLogError("Usage: --case <name from --list-cases> --backend metal|opengl|vulkan|webgpu [--output directory | --output-file file.png]");
        return 1;
    }
    const CaptureBackend* backend = 0;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(CAPTURE_BACKENDS); ++i)
    {
        if (strcmp(backend_name, CAPTURE_BACKENDS[i].m_Name) == 0)
        {
            backend = &CAPTURE_BACKENDS[i];
            break;
        }
    }
    if (!backend || !InstallAdapter(backend->m_Family) || GetInstalledAdapterFamily() != backend->m_Family)
    {
        dmLogError("Requested graphics backend unavailable: %s", backend_name);
        return 1;
    }
    dmLogInfo("GRAPHICS_CAPTURE_BACKEND=%s", backend->m_Name);

    char filename[1024];
    int length;
    if (output_file)
    {
        length = dmSnPrintf(filename, sizeof(filename), "%s", output_file);
    }
    else
    {
        const char* output_directory = directory ? directory : "graphics-test-images";
        length = dmSnPrintf(filename, sizeof(filename), "%s/%s/%s.png", output_directory, backend_name, name);
    }
    if (length < 0 || length >= (int)sizeof(filename) || !MakeCaptureParent(filename))
    {
        dmLogError("Cannot create capture output path");
        return 1;
    }
    remove(filename);

    CaptureContext capture = {};
    if (!InitializeCapture(*backend, &capture))
    {
        FinalizeCapture(&capture);
        return 1;
    }

    CaptureResources resources = {};
    bool             success = CreateCaptureResources(capture.m_Context, backend->m_Family, strcmp(name, "cubemap") == 0, &resources);
    if (success)
    {
        success = CaptureImage(capture.m_Context, resources, name, filename);
    }
    DeleteCaptureResources(capture.m_Context, &resources);
    FinalizeCapture(&capture);
    return success ? 0 : 1;
}
