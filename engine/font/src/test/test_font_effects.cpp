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
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <testmain/testmain.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <graphics/graphics.h>
#include <graphics_private.h>
#include <test/test_graphics_util.h>
#include <platform/window.hpp>
#include <font_render.h>
#include <font.h>
#include <fontcollection.h>
#include <text_layout.h>

extern "C" void dmExportedSymbols();

class FontEffectTest : public jc_test_base_class
{
    protected:
    HWindow                        m_Window;
    dmGraphics::HContext           m_Context;
    dmGraphics::HProgram           m_Program;
    dmGraphics::HRenderTarget      m_Target;
    dmGraphics::HVertexDeclaration m_Declaration;
    FontEffectCache*               m_Cache;
    uint32_t                       m_Frame;
    dmArray<uint8_t>               m_Pixels;

    void                           SetUp() override
    {
        TestMainPlatformInit();
        dmExportedSymbols();
        ASSERT_TRUE(dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_OPENGL));
        m_Window = dmPlatform::NewWindow();
        WindowCreateParams window;
        WindowCreateParamsInitialize(&window);
        window.m_Width = window.m_Height = 16;
        window.m_Title = "Font color effect tests";
        window.m_Hidden = 1;
        window.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(m_Window, window));
        dmGraphics::ContextParams params;
        params.m_Window = m_Window;
        params.m_Width = params.m_Height = 16;
        m_Context = dmGraphics::NewContext(params);
        ASSERT_NE((dmGraphics::HContext)0, m_Context);
        m_Cache = FontCreateEffectCache(m_Context);
        m_Frame = 0;

        FILE* file = fopen("content/builtins/fonts/font-effect.glsl", "rb");
        ASSERT_NE((FILE*)0, file);
        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        rewind(file);
        dmArray<char> source;
        source.SetCapacity((uint32_t)size + 1);
        source.SetSize(source.Capacity());
        ASSERT_EQ((size_t)size, fread(source.Begin(), 1, size, file));
        source[size] = 0;
        fclose(file);
        dmArray<char> fragment;
        fragment.SetCapacity((uint32_t)size + 512);
        fragment.SetSize(fragment.Capacity());
        // A custom varying name guards against the helper reading a global var_effect.
        const char prefix[] = "#version 140\nin highp vec4 test_coordinates;\nin vec4 test_face_color;\nin float test_mode;\n";
        memcpy(fragment.Begin(), prefix, sizeof(prefix) - 1);
        memcpy(fragment.Begin() + sizeof(prefix) - 1, source.Begin(), size);
        dmStrlCpy(fragment.Begin() + sizeof(prefix) - 1 + size,
                  "\nout vec4 out_color;\nvoid main() { out_color = test_mode < 0.5 ? font_effect_color(test_coordinates) : "
                  "font_effect_face_color(test_coordinates, test_face_color, test_mode > 1.5); }\n",
                  512 - sizeof(prefix) + 1);
        const char vertex_prefix[] =
        "#version 140\nin vec2 position;\nin vec4 effect;\nin vec4 face_color;\nin float mode;\n"
        "out highp vec4 test_coordinates;\nout vec4 test_face_color;\nout float test_mode;\n";
        dmArray<char> vertex;
        vertex.SetCapacity((uint32_t)size + 1024);
        vertex.SetSize(vertex.Capacity());
        dmStrlCpy(vertex.Begin(), vertex_prefix, vertex.Size());
        dmStrlCat(vertex.Begin(), source.Begin(), vertex.Size());
        dmStrlCat(vertex.Begin(),
                  "\nvoid main() { gl_Position = vec4(position, 0.0, 1.0); test_coordinates = effect; "
                  "test_face_color = font_effect_vertex_color(test_coordinates, face_color); "
                  "if (effect.z < 0.0 && mode > 1.5) test_face_color.rgb *= test_face_color.a; test_mode = mode; }\n",
                  vertex.Size());
        dmGraphics::ShaderDescBuilder shaders;
        shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex.Begin(), strlen(vertex.Begin()));
        shaders.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment.Begin(), strlen(fragment.Begin()));
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "effect", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "face_color", 2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
        shaders.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "mode", 3, dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT);
        shaders.AddTexture("font_effects", 0, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);
        char error[1024] = {};
        m_Program = dmGraphics::NewProgram(m_Context, shaders.Get(), error, sizeof(error));
        if (!m_Program)
        {
            printf("Font effect shader: %s\n", error);
        }
        ASSERT_NE((dmGraphics::HProgram)0, m_Program);
        dmGraphics::HVertexStreamDeclaration streams = dmGraphics::NewVertexStreamDeclaration(m_Context);
        dmGraphics::AddVertexStream(streams, "position", 2, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(streams, "effect", 4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(streams, "face_color", 4, dmGraphics::TYPE_FLOAT, false);
        dmGraphics::AddVertexStream(streams, "mode", 1, dmGraphics::TYPE_FLOAT, false);
        m_Declaration = dmGraphics::NewVertexDeclaration(m_Context, streams, 11 * sizeof(float));
        dmGraphics::DeleteVertexStreamDeclaration(streams);
        dmGraphics::RenderTargetCreationParams target;
        target.m_ColorBufferCreationParams[0].m_Width = target.m_ColorBufferParams[0].m_Width = 128;
        target.m_ColorBufferCreationParams[0].m_Height = target.m_ColorBufferParams[0].m_Height = 64;
        target.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        target.m_SampleCount = 1;
        m_Target = dmGraphics::NewRenderTarget(m_Context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target);
    }

    void TearDown() override
    {
        FontDeleteEffectCache(m_Cache);
        dmGraphics::DeleteRenderTarget(m_Context, m_Target);
        dmGraphics::DeleteVertexDeclaration(m_Declaration);
        dmGraphics::DeleteProgram(m_Context, m_Program);
        dmGraphics::DeleteContext(m_Context);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
    }

    uint32_t Draw(const FontColorEffect& effect, float phase = 0.0f, const float* face_color = 0, bool premultiplied = false, bool vertex_evaluation = false)
    {
        FontEffectRecord record;
        FontPackEffect(effect, &record);
        uint32_t index = FontCacheEffect(m_Cache, record, ++m_Frame);
        FontUploadEffects(m_Cache);
        const float corners[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
        float       vertices[6][11] = {};
        for (uint32_t i = 0; i < 6; ++i)
        {
            vertices[i][0] = corners[i][0] * 2 - 1;
            vertices[i][1] = corners[i][1] * 2 - 1;
            vertices[i][2] = corners[i][0];
            vertices[i][3] = corners[i][1];
            vertices[i][4] = vertex_evaluation ? -(float)index : (float)index;
            vertices[i][5] = phase;
            if (face_color)
            {
                memcpy(vertices[i] + 6, face_color, 4 * sizeof(float));
                vertices[i][10] = premultiplied ? 2.0f : 1.0f;
            }
        }
        dmGraphics::HVertexBuffer buffer = dmGraphics::NewVertexBuffer(m_Context, sizeof(vertices), vertices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        dmGraphics::BeginFrame(m_Context);
        dmGraphics::SetRenderTarget(m_Context, m_Target, dmGraphics::RenderTargetBindingParams());
        dmGraphics::SetViewport(m_Context, 0, 0, 128, 64);
        dmGraphics::DisableState(m_Context, dmGraphics::STATE_BLEND);
        dmGraphics::EnableProgram(m_Context, m_Program);
        dmGraphics::EnableTexture(m_Context, 0, 0, FontGetEffectTexture(m_Cache));
        dmGraphics::EnableVertexBuffer(m_Context, buffer, 0);
        dmGraphics::EnableVertexDeclaration(m_Context, m_Declaration, 0, 0, m_Program);
        dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, 1);
        m_Pixels.SetCapacity(128 * 64 * 4);
        m_Pixels.SetSize(m_Pixels.Capacity());
        dmGraphics::ReadPixels(m_Context, 0, 0, 128, 64, m_Pixels.Begin(), m_Pixels.Size());
        // OpenGL readback returns BGRA with the top row first.
        for (uint32_t i = 0; i < m_Pixels.Size(); i += 4)
        {
            const uint8_t blue = m_Pixels[i];
            m_Pixels[i] = m_Pixels[i + 2];
            m_Pixels[i + 2] = blue;
        }
        dmGraphics::DisableVertexDeclaration(m_Context, m_Declaration);
        dmGraphics::DisableVertexBuffer(m_Context, buffer);
        dmGraphics::DisableTexture(m_Context, 0, FontGetEffectTexture(m_Cache));
        dmGraphics::SetRenderTarget(m_Context, 0, dmGraphics::RenderTargetBindingParams());
        dmGraphics::Flip(m_Context);
        dmGraphics::DeleteVertexBuffer(buffer);
        return index;
    }

    const uint8_t* Pixel(uint32_t x, uint32_t y)
    {
        return &m_Pixels[((63 - y) * 128 + x) * 4];
    }

    void CheckGradientStopCounts(FontColorEffectType type)
    {
        const uint8_t colors[4][4] = { { 240, 20, 40, 255 }, { 20, 220, 60, 192 }, { 40, 60, 240, 128 }, { 240, 200, 40, 64 } };
        const float   positions[] = { 0.125f, 0.375f, 0.625f, 0.875f };
        for (uint32_t count = 1; count <= 4; ++count)
        {
            for (uint32_t custom = 0; custom < 2; ++custom)
            {
                FontColorEffect effect = {};
                effect.m_Type = type;
                effect.m_StopCount = count;
                for (uint32_t i = 0; i < count; ++i)
                {
                    memcpy(effect.m_Colors[i], colors[i], 4);
                    effect.m_Positions[i] = custom ? positions[i] : count > 1 ? (float)i / (count - 1) :
                                                                                0.0f;
                }
                // At pixel row 32, both gradients run from t=0 at x=16 to t=1 at x=80.
                if (type == FONT_COLOR_EFFECT_RADIAL)
                {
                    effect.m_Parameters[0] = 16.5f / 128.0f;
                    effect.m_Parameters[1] = 32.5f / 64.0f;
                    effect.m_Parameters[2] = 2.0f;
                    effect.m_Parameters[3] = 1.0f;
                }
                else
                {
                    effect.m_Parameters[0] = 2.0f;
                    effect.m_Parameters[2] = -16.5f / 64.0f;
                }
                for (uint32_t animated = 0; animated < 2; ++animated)
                {
                    effect.m_Flags = animated ? FONT_COLOR_EFFECT_MIRROR : 0;
                    Draw(effect, (float)animated);
                    for (uint32_t sample = 0; sample <= 8; ++sample)
                    {
                        const float t = animated ? 1.0f - sample / 8.0f : sample / 8.0f;
                        uint32_t    left = 0;
                        while (left + 1 < count && t > effect.m_Positions[left + 1])
                        {
                            ++left;
                        }
                        const uint32_t right = left + 1 < count ? left + 1 : left;
                        float          weight = right == left ? 0.0f : (t - effect.m_Positions[left]) / (effect.m_Positions[right] - effect.m_Positions[left]);
                        weight = weight < 0.0f ? 0.0f : weight > 1.0f ? 1.0f :
                                                                        weight;
                        for (uint32_t channel = 0; channel < 4; ++channel)
                        {
                            const float expected = colors[left][channel] * (1.0f - weight) + colors[right][channel] * weight;
                            ASSERT_NEAR(expected, Pixel(16 + sample * 8, 32)[channel], 1.0f);
                        }
                    }
                    if (!animated)
                    {
                        for (uint32_t channel = 0; channel < 4; ++channel)
                        {
                            ASSERT_NEAR(colors[count - 1][channel], Pixel(96, 32)[channel], 1.0f);
                        }
                    }
                }
            }
        }
    }
};

// Constant colors bypass spatial evaluation and retain their stored alpha.
TEST_F(FontEffectTest, SingleColor)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_COLOR;
    effect.m_StopCount = 1;
    const uint8_t color[] = { 64, 128, 192, 128 };
    memcpy(effect.m_Colors[0], color, 4);
    Draw(effect);
    for (uint32_t c = 0; c < 4; ++c)
    {
        EXPECT_NEAR(color[c], Pixel(0, 0)[c], 1);
        EXPECT_NEAR(color[c], Pixel(127, 63)[c], 1);
    }
}

// Linear gradients with 1-4 stops retain endpoint RGBA and interpolate default/custom positions through animation.
TEST_F(FontEffectTest, LinearGradientStopCounts)
{
    CheckGradientStopCounts(FONT_COLOR_EFFECT_LINEAR);
}

// Radial gradients with 1-4 stops stay constant for one color and interpolate every authored ring through animation.
TEST_F(FontEffectTest, RadialGradientStopCounts)
{
    CheckGradientStopCounts(FONT_COLOR_EFFECT_RADIAL);
}

// Interior stops must affect pixels inside a quad without adding geometry.
TEST_F(FontEffectTest, PositionedStopsInsideQuad)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_LINEAR;
    effect.m_StopCount = 4;
    const uint8_t colors[4][4] = { { 255, 255, 0, 255 }, { 255, 255, 0, 255 }, { 255, 0, 0, 255 }, { 0, 0, 255, 255 } };
    memcpy(effect.m_Colors, colors, sizeof(colors));
    effect.m_Positions[1] = 0.5f;
    effect.m_Positions[2] = 0.75f;
    effect.m_Positions[3] = 1.0f;
    effect.m_Parameters[0] = 1.0f;
    Draw(effect);
    EXPECT_NEAR(255, Pixel(32, 32)[0], 1);
    EXPECT_NEAR(255, Pixel(32, 32)[1], 1);
    EXPECT_NEAR(123, Pixel(80, 32)[1], 2);
    EXPECT_NEAR(123, Pixel(112, 32)[0], 2);
    EXPECT_NEAR(132, Pixel(112, 32)[2], 2);
}

// Aspect-correct radial distance must be circular and evaluated per fragment.
TEST_F(FontEffectTest, CircularRadialGradient)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_RADIAL;
    effect.m_StopCount = 2;
    effect.m_Colors[0][0] = effect.m_Colors[0][3] = 255;
    effect.m_Colors[1][2] = effect.m_Colors[1][3] = 255;
    effect.m_Positions[1] = 1.0f;
    effect.m_Parameters[0] = effect.m_Parameters[1] = 0.5f;
    effect.m_Parameters[2] = 128.0f / 16.0f;
    effect.m_Parameters[3] = 64.0f / 16.0f;
    Draw(effect);
    EXPECT_GT(Pixel(64, 32)[0], 230);
    EXPECT_LT(Pixel(64, 32)[2], 20);
    EXPECT_NEAR(Pixel(75, 32)[2], Pixel(64, 43)[2], 1);
    EXPECT_EQ(255, Pixel(0, 0)[2]);
}

// Rotated ellipses sample equal colors along both axes with 1-4 stops, including animated sampling.
TEST_F(FontEffectTest, RotatedEllipticalRadialGradient)
{
    const uint8_t colors[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 } };
    for (uint32_t count = 1; count <= 4; ++count)
    {
        for (uint32_t animated = 0; animated < 2; ++animated)
        {
            FontColorEffect effect = {};
            effect.m_Type = FONT_COLOR_EFFECT_RADIAL;
            effect.m_StopCount = count;
            effect.m_Flags = animated ? FONT_COLOR_EFFECT_MIRROR : 0;
            memcpy(effect.m_Colors, colors, sizeof(colors));
            for (uint32_t i = 0; i < count; ++i)
            {
                effect.m_Positions[i] = count > 1 ? (float)i / (count - 1) : 0.0f;
            }
            // A 40x20-radius ellipse rotated by atan2(3, 4), centered on a pixel.
            effect.m_Parameters[0] = 64.5f / 128.0f;
            effect.m_Parameters[1] = 32.5f / 64.0f;
            effect.m_Parameters[2] = 0.8f * 128.0f / 40.0f;
            effect.m_Parameters[3] = 0.8f * 64.0f / 20.0f;
            effect.m_Parameters[4] = 0.6f * 64.0f / 40.0f;
            effect.m_Parameters[5] = -0.6f * 128.0f / 20.0f;
            Draw(effect, animated ? 0.25f : 0.0f);
            for (uint32_t i = 0; i <= 4; ++i)
            {
                float t = i * 0.25f + (animated ? 0.25f : 0.0f);
                t = t > 1.0f ? 2.0f - t : t;
                const float    coordinate = t * (count - 1);
                const uint32_t left = (uint32_t)coordinate;
                const uint32_t right = left + 1 < count ? left + 1 : left;
                const float    weight = coordinate - left;
                const uint8_t* samples[] = { Pixel(64 + 8 * i, 32 + 6 * i), Pixel(64 - 3 * i, 32 + 4 * i), Pixel(64 - 8 * i, 32 - 6 * i), Pixel(64 + 3 * i, 32 - 4 * i) };
                for (uint32_t sample = 0; sample < 4; ++sample)
                {
                    for (uint32_t channel = 0; channel < 4; ++channel)
                    {
                        ASSERT_NEAR(colors[left][channel] * (1.0f - weight) + colors[right][channel] * weight, samples[sample][channel], 1.0f);
                    }
                }
            }
        }
    }
}

// Four-corner evaluation has a bilinear center instead of a triangle-diagonal color seam.
TEST_F(FontEffectTest, FourCornerGradient)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_FOUR_CORNER;
    effect.m_StopCount = 4;
    const uint8_t colors[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 } };
    memcpy(effect.m_Colors, colors, sizeof(colors));
    Draw(effect);
    for (uint32_t c = 0; c < 3; ++c)
    {
        EXPECT_NEAR(128, Pixel(64, 32)[c], 3);
    }
}

// Animation phase is per draw and must not create or modify the shared color record.
TEST_F(FontEffectTest, AnimationSharesRecord)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_LINEAR;
    effect.m_StopCount = 2;
    effect.m_Flags = FONT_COLOR_EFFECT_MIRROR;
    effect.m_Colors[0][0] = effect.m_Colors[0][3] = 255;
    effect.m_Colors[1][2] = effect.m_Colors[1][3] = 255;
    effect.m_Positions[1] = 1.0f;
    effect.m_Parameters[0] = 1.0f;
    uint32_t index = Draw(effect);
    EXPECT_GT(Pixel(0, 0)[0], 250);
    EXPECT_EQ(index, Draw(effect, 1.0f));
    EXPECT_GT(Pixel(0, 0)[2], 250);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

// Gradients replace inherited RGB and cap alpha, matching CPU markup precedence after the dev merge.
TEST_F(FontEffectTest, GradientOverridesInheritedColor)
{
    FontColorEffect effect = {};
    effect.m_Type = FONT_COLOR_EFFECT_COLOR;
    effect.m_StopCount = 1;
    effect.m_Colors[0][0] = 240;
    effect.m_Colors[0][1] = 120;
    effect.m_Colors[0][2] = 60;
    const float   face_color[] = { 0.0f, 0.0f, 0.0f, 128.0f / 255.0f };
    const uint8_t alpha[] = { 64, 192 };
    for (uint32_t a = 0; a < DM_ARRAY_SIZE(alpha); ++a)
    {
        for (uint32_t premultiplied = 0; premultiplied < 2; ++premultiplied)
        {
            effect.m_Colors[0][3] = alpha[a];
            Draw(effect, 0.0f, face_color, premultiplied != 0);
            const float expected_alpha = a == 0 ? 64.0f : 128.0f;
            for (uint32_t channel = 0; channel < 3; ++channel)
            {
                EXPECT_NEAR(effect.m_Colors[0][channel] * (premultiplied ? expected_alpha / 255.0f : 1.0f), Pixel(64, 32)[channel], 1);
            }
            EXPECT_NEAR(expected_alpha, Pixel(64, 32)[3], 1);
        }
    }
}

// Growing beyond 255 records preserves old indices and radial interiors on the GPU.
TEST_F(FontEffectTest, GrowthPreservesRadialGradient)
{
    FontColorEffect color = {};
    color.m_StopCount = 1;
    color.m_Colors[0][3] = 255;
    for (uint32_t i = 1; i <= 300; ++i)
    {
        color.m_Colors[0][0] = (uint8_t)i;
        color.m_Colors[0][1] = (uint8_t)(i >> 8);
        FontEffectRecord record;
        FontPackEffect(color, &record);
        ASSERT_EQ(i, FontCacheEffect(m_Cache, record, 1));
        if (i == 255)
        {
            FontUploadEffects(m_Cache);
            ASSERT_EQ(256u, dmGraphics::GetTextureHeight(m_Context, FontGetEffectTexture(m_Cache)));
        }
    }
    FontColorEffect radial = {};
    radial.m_Type = FONT_COLOR_EFFECT_RADIAL;
    radial.m_StopCount = 2;
    radial.m_Colors[0][0] = radial.m_Colors[0][3] = 255;
    radial.m_Colors[1][2] = radial.m_Colors[1][3] = 255;
    radial.m_Positions[1] = 1;
    radial.m_Parameters[0] = radial.m_Parameters[1] = 0.5f;
    radial.m_Parameters[2] = 4;
    radial.m_Parameters[3] = 2;
    ASSERT_EQ(301u, Draw(radial));
    ASSERT_GT(Pixel(64, 32)[0], 240);
    ASSERT_EQ(255, Pixel(0, 0)[2]);
    ASSERT_EQ(512u, dmGraphics::GetTextureHeight(m_Context, FontGetEffectTexture(m_Cache)));
    color.m_Colors[0][0] = 1;
    color.m_Colors[0][1] = 0;
    ASSERT_EQ(1u, Draw(color));
    ASSERT_EQ(1, Pixel(64, 32)[0]);
}

// Legacy animation and alpha caps must retain the CPU's per-corner evaluation before rasterization.
TEST_F(FontEffectTest, LegacyGradientMatchesCpuInterpolation)
{
    HFont font = FontLoadFromPath("src/test/data/NotoSans-Regular.ttf");
    ASSERT_NE((HFont)0, font);
    HFontCollection collection = FontCollectionCreate();
    ASSERT_EQ(FONT_RESULT_OK, FontCollectionAddFont(collection, font));
    const char* sources[] = {
        "<gradient bottom=#FF000040 top=#0000FFFF hz=1>A</gradient>",
        "<gradient left=#FF000040 right=#0000FFFF hz=1>A</gradient>",
        "<gradient bl=#FF000040 br=#00FF0080 tl=#0000FFC0 tr=#FFFFFFFF hz=1>A</gradient>"
    };
    const uint8_t  corners[4][4] = { { 255, 0, 0, 64 }, { 0, 255, 0, 128 }, { 0, 0, 255, 192 }, { 255, 255, 255, 255 } };
    const float    times[] = { 0.0f, 0.125f, 0.25f, 0.625f };
    const float    base_color[] = { 1.0f, 1.0f, 1.0f, 128.0f / 255.0f };
    const uint32_t samples[][2] = { { 4, 4 }, { 32, 16 }, { 64, 32 }, { 96, 48 }, { 120, 60 } };
    for (uint32_t type = 0; type < DM_ARRAY_SIZE(sources); ++type)
    {
        HMarkup markup = 0;
        ASSERT_EQ(MARKUP_RESULT_OK, MarkupCreate(sources[type], strlen(sources[type]), &markup, 0));
        TextLayoutSettings settings = {};
        settings.m_Size = 32.0f;
        settings.m_Leading = 1.0f;
        HTextLayout layout = 0;
        ASSERT_EQ(TEXT_RESULT_OK, TextLayoutCreateMarkup(collection, markup, &settings, &layout));
        MarkupDestroy(markup);
        FontColorEffect effect = {};
        effect.m_Type = type == 2 ? FONT_COLOR_EFFECT_FOUR_CORNER : FONT_COLOR_EFFECT_LINEAR;
        effect.m_StopCount = type == 2 ? 4 : 2;
        effect.m_Flags = FONT_COLOR_EFFECT_MIRROR;
        memcpy(effect.m_Colors, corners, sizeof(corners));
        if (type != 2)
        {
            const uint8_t blue[] = { 0, 0, 255, 255 };
            memcpy(effect.m_Colors[1], blue, sizeof(blue));
            effect.m_Positions[1] = 1.0f;
            effect.m_Parameters[type == 0 ? 1 : 0] = 1.0f;
        }
        for (uint32_t t = 0; t < DM_ARRAY_SIZE(times); ++t)
        {
            layout->m_ElapsedTime = times[t];
            TextGlyphFaceColors cpu_colors;
            TextLayoutGetGlyphFaceColors(layout, TextLayoutGetGlyphs(layout)[0], base_color, &cpu_colors);
            const float* colors[] = { cpu_colors.m_BottomLeft, cpu_colors.m_BottomRight, cpu_colors.m_TopLeft, cpu_colors.m_TopRight };
            for (uint32_t premultiplied = 0; premultiplied < 2; ++premultiplied)
            {
                Draw(effect, times[t] * 2.0f, base_color, premultiplied != 0, true);
                for (uint32_t sample = 0; sample < DM_ARRAY_SIZE(samples); ++sample)
                {
                    const uint32_t px = samples[sample][0];
                    const uint32_t py = samples[sample][1];
                    const float    x = (px + 0.5f) / 128.0f;
                    const float    y = (py + 0.5f) / 64.0f;
                    // The quad's two triangles interpolate the old CPU corner colors.
                    float          weights[4] = { 1.0f - x - y, x, y, 0.0f };
                    if (x + y > 1.0f)
                    {
                        weights[0] = 0.0f;
                        weights[1] = 1.0f - y;
                        weights[2] = 1.0f - x;
                        weights[3] = x + y - 1.0f;
                    }
                    for (uint32_t c = 0; c < 4; ++c)
                    {
                        float expected = 0.0f;
                        for (uint32_t corner = 0; corner < 4; ++corner)
                        {
                            expected += weights[corner] * colors[corner][c] * (premultiplied && c < 3 ? colors[corner][3] : 1.0f);
                        }
                        ASSERT_NEAR(expected * 255.0f, Pixel(px, py)[c], 1.5f);
                    }
                }
            }
        }
        TextLayoutRelease(layout);
    }
    FontCollectionDestroy(collection);
    FontDestroy(font);
}
