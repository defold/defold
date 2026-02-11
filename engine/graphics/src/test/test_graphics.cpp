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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include <platform/platform_window.h>
#include <dmsdk/dlib/dstrings.h> // dmStrCaseCmp

#include "graphics.h"
#include "graphics_private.h"
#include "graphics_native.h"

#include "test_graphics_util.h"

#include "null/graphics_null_private.h"

#define APP_TITLE "GraphicsTest"
#define WIDTH 8u
#define HEIGHT 4u

#define ASSERT_VECF(exp, act, num_values) \
    for (int i = 0; i < num_values; ++i) \
        ASSERT_NEAR(exp[i], act[i], EPSILON);

#define ASSERT_VEC(exp, act, num_values) \
    for (int i = 0; i < num_values; ++i) \
        ASSERT_EQ(exp[i], act[i]);

const float EPSILON = 0.000001f;

using namespace dmVMath;

template<bool ASYNCHRONOUS>
class dmGraphicsTestT : public jc_test_base_class
{
public:
    static const bool s_Asynchronous = ASYNCHRONOUS;

    struct ResizeData
    {
        uint32_t m_Width;
        uint32_t m_Height;
    };

    struct CloseData
    {
        bool m_ShouldClose;
    };

    HJobContext m_JobContext;
    dmPlatform::HWindow m_Window;
    dmGraphics::HContext m_Context;
    dmGraphics::NullContext* m_NullContext;
    ResizeData m_ResizeData;
    CloseData m_CloseData;

    static void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
    {
        ResizeData* data = (ResizeData*)user_data;
        data->m_Width = width;
        data->m_Height = height;
    }

    static bool OnWindowClose(void* user_data)
    {
        CloseData* data = (CloseData*)user_data;
        return data->m_ShouldClose;
    }

    void SetUp() override
    {
        dmGraphics::InstallAdapter();

        dmPlatform::WindowParams params;
        params.m_ResizeCallback = OnWindowResize;
        params.m_ResizeCallbackUserData = &m_ResizeData;
        params.m_CloseCallback = OnWindowClose;
        params.m_CloseCallbackUserData = &m_CloseData;
        params.m_Title = APP_TITLE;
        params.m_Width = WIDTH;
        params.m_Height = HEIGHT;
        params.m_Fullscreen = false;
        params.m_PrintDeviceInfo = false;
        params.m_ContextAlphabits = 8;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, params);

        JobSystemCreateParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadCount = 1;

        if (dmGraphicsTestT::s_Asynchronous)
            m_JobContext = JobSystemCreate(&job_thread_create_param);
        else
            m_JobContext = 0;

        dmGraphics::ContextParams context_params = dmGraphics::ContextParams();
        context_params.m_Window                  = m_Window;
        context_params.m_JobContext              = m_JobContext;

        m_Context = dmGraphics::NewContext(context_params);
        m_NullContext = (dmGraphics::NullContext*) m_Context;
        m_NullContext->m_UseAsyncTextureLoad = 0;

        m_ResizeData.m_Width = 0;
        m_ResizeData.m_Height = 0;
    }

    void TearDown() override
    {
        if (m_JobContext)
            JobSystemDestroy(m_JobContext);
        dmGraphics::CloseWindow(m_Context);
        dmGraphics::DeleteContext(m_Context);
        dmPlatform::DeleteWindow(m_Window);
    }
};

class dmGraphicsTest : public dmGraphicsTestT<true>
{
};

class dmGraphicsSynchronousTest : public dmGraphicsTestT<false>
{
};

TEST_F(dmGraphicsTest, NewDeleteContext)
{
    ASSERT_NE((void*)0, m_Context);
}

TEST_F(dmGraphicsTest, DoubleNewContext)
{
    ASSERT_NE((void*)0, m_Context);
    ASSERT_EQ((dmGraphics::HContext)0, dmGraphics::NewContext(dmGraphics::ContextParams()));
}

TEST_F(dmGraphicsTest, CloseWindow)
{
    dmGraphics::CloseWindow(m_Context);
    dmGraphics::CloseWindow(m_Context);
}

TEST_F(dmGraphicsTest, TestWindowState)
{
    ASSERT_TRUE(dmGraphics::GetWindowStateParam(m_Context, dmPlatform::WINDOW_STATE_OPENED) ? true : false);
    dmGraphics::CloseWindow(m_Context);
    ASSERT_FALSE(dmGraphics::GetWindowStateParam(m_Context, dmPlatform::WINDOW_STATE_OPENED));
}

TEST_F(dmGraphicsTest, TestWindowSize)
{
    ASSERT_EQ(m_NullContext->m_Width, dmGraphics::GetWidth(m_Context));
    ASSERT_EQ(m_NullContext->m_Height, dmGraphics::GetHeight(m_Context));
    uint32_t width = WIDTH * 2;
    uint32_t height = HEIGHT * 2;
    dmGraphics::SetWindowSize(m_Context, width, height);
    ASSERT_EQ(width, dmGraphics::GetWidth(m_Context));
    ASSERT_EQ(height, dmGraphics::GetHeight(m_Context));
    ASSERT_EQ(width, dmGraphics::GetWindowWidth(m_Context));
    ASSERT_EQ(height, dmGraphics::GetWindowHeight(m_Context));
    ASSERT_EQ(width, m_ResizeData.m_Width);
    ASSERT_EQ(height, m_ResizeData.m_Height);
}

TEST_F(dmGraphicsTest, TestDefaultTextureFilters)
{
    dmGraphics::TextureFilter min_filter;
    dmGraphics::TextureFilter mag_filter;
    dmGraphics::GetDefaultTextureFilters(m_Context, min_filter, mag_filter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST, min_filter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR, mag_filter);
}
TEST_F(dmGraphicsTest, Flip)
{
    dmGraphics::Flip(m_Context);
}

TEST_F(dmGraphicsTest, Clear)
{
    const uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    uint32_t width = WIDTH;
    uint32_t height = HEIGHT;
    uint32_t data_size = sizeof(uint32_t) * width * height;
    uint32_t* data = new uint32_t[width * height];
    memset(data, 1, data_size);
    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    delete [] data;
    width *= 2;
    height *= 2;
    data = new uint32_t[width * height];
    data_size = sizeof(uint32_t) * width * height;
    memset(data, 1, data_size);
    dmGraphics::SetWindowSize(m_Context, width, height);
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    delete [] data;
}

TEST_F(dmGraphicsTest, VertexBuffer)
{
    char data[16];
    memset(data, 1, sizeof(data));
    dmGraphics::HVertexBuffer vertex_buffer = dmGraphics::NewVertexBuffer(m_Context, 16, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::VertexBuffer* vb = (dmGraphics::VertexBuffer*)vertex_buffer;
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(data, 2, sizeof(data));
    dmGraphics::SetVertexBufferData(vertex_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(&data[4], 3, 4);
    dmGraphics::SetVertexBufferSubData(vertex_buffer, 4, 4, &data[4]);
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    // Invalid range
    memset(&data[14], 4, 1);
    dmGraphics::SetVertexBufferSubData(vertex_buffer, 14, 4, data);
    ASSERT_NE(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    memset(data, 4, 4);
    void* copy = dmGraphics::MapVertexBuffer(m_Context, vertex_buffer, dmGraphics::BUFFER_ACCESS_READ_WRITE);
    memcpy(copy, data, sizeof(data));
    ASSERT_NE(0, memcmp(data, vb->m_Buffer, sizeof(data)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(m_Context, vertex_buffer));
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, sizeof(data)));

    // Smaller size
    dmGraphics::SetVertexBufferData(vertex_buffer, 1, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(1u, vb->m_Size);

    // Bigger size
    dmGraphics::SetVertexBufferData(vertex_buffer, 4, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(4u, vb->m_Size);
    ASSERT_EQ(0, memcmp(data, vb->m_Buffer, 4));

    dmGraphics::DeleteVertexBuffer(vertex_buffer);
}

TEST_F(dmGraphicsTest, IndexBuffer)
{
    char data[16];
    memset(data, 1, sizeof(data));
    dmGraphics::HIndexBuffer index_buffer = dmGraphics::NewIndexBuffer(m_Context, 16, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::IndexBuffer* ib = (dmGraphics::IndexBuffer*)index_buffer;
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(data, 2, sizeof(data));
    dmGraphics::SetIndexBufferData(index_buffer, sizeof(data), data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(&data[4], 3, 4);
    dmGraphics::SetIndexBufferSubData(index_buffer, 4, 4, &data[4]);
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    // Invalid range
    memset(&data[14], 4, 1);
    dmGraphics::SetIndexBufferSubData(index_buffer, 14, 4, data);
    ASSERT_NE(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    memset(data, 4, 4);
    void* copy = dmGraphics::MapIndexBuffer(m_Context, index_buffer, dmGraphics::BUFFER_ACCESS_READ_WRITE);
    memcpy(copy, data, sizeof(data));
    ASSERT_NE(0, memcmp(data, ib->m_Buffer, sizeof(data)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(m_Context, index_buffer));
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, sizeof(data)));

    // Smaller size
    dmGraphics::SetIndexBufferData(index_buffer, 1, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(1u, ib->m_Size);

    // Bigger size
    dmGraphics::SetIndexBufferData(index_buffer, 4, data, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    ASSERT_EQ(4u, ib->m_Size);
    ASSERT_EQ(0, memcmp(data, ib->m_Buffer, 4));

    dmGraphics::DeleteIndexBuffer(index_buffer);
}

TEST_F(dmGraphicsTest, VertexStreamDeclaration)
{
    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_Context);
    dmGraphics::AddVertexStream(stream_declaration, "stream0", 2, dmGraphics::TYPE_BYTE, true);
    dmGraphics::AddVertexStream(stream_declaration, "stream1", 4, dmGraphics::TYPE_FLOAT, false);

    #define TEST_STREAM_DECLARATION(streams, name, ix, size, type, normalize) \
        ASSERT_TRUE(streams[ix].m_NameHash == dmHashString64(name)); \
        ASSERT_EQ(streams[ix].m_Stream, ix); \
        ASSERT_EQ(streams[ix].m_Size, size); \
        ASSERT_EQ(streams[ix].m_Type, type); \
        ASSERT_EQ(streams[ix].m_Normalize, normalize);

    ASSERT_EQ(stream_declaration->m_StreamCount, 2);
    TEST_STREAM_DECLARATION(stream_declaration->m_Streams, "stream0", 0, 2, dmGraphics::TYPE_BYTE, true);
    TEST_STREAM_DECLARATION(stream_declaration->m_Streams, "stream1", 1, 4, dmGraphics::TYPE_FLOAT, false);

    #undef TEST_STREAM_DECLARATION

    #define TEST_STREAM(streams, name, ix, location, size, type, normalize) \
        ASSERT_TRUE(streams[ix].m_NameHash == dmHashString64(name)); \
        ASSERT_EQ(streams[ix].m_Location, location); \
        ASSERT_EQ(streams[ix].m_Size, size); \
        ASSERT_EQ(streams[ix].m_Type, type); \
        ASSERT_EQ(streams[ix].m_Normalize, normalize);

    // Test that the stream declaration has been passed to the vertex declaration
    dmGraphics::HVertexDeclaration vertex_declaration = dmGraphics::NewVertexDeclaration(m_Context, stream_declaration);
    dmGraphics::VertexDeclaration* vx = (dmGraphics::VertexDeclaration*) vertex_declaration;
    ASSERT_EQ(vx->m_StreamCount, 2);
    TEST_STREAM(vx->m_Streams, "stream0", 0, -1, 2, dmGraphics::TYPE_BYTE, true);
    TEST_STREAM(vx->m_Streams, "stream1", 1, -1, 4, dmGraphics::TYPE_FLOAT, false);

    #undef TEST_STREAM

    uint32_t vx_stride = dmGraphics::GetVertexDeclarationStride(vertex_declaration);
    ASSERT_EQ(2 + 4 * sizeof(float), vx_stride);

    dmGraphics::DeleteVertexDeclaration(vertex_declaration);
    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);
}

TEST_F(dmGraphicsTest, VertexDeclaration)
{
    float v[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f };
    dmGraphics::HVertexBuffer vertex_buffer = dmGraphics::NewVertexBuffer(m_Context, sizeof(v), (void*)v, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_Context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 3, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "uv",       2, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::HVertexDeclaration vertex_declaration = dmGraphics::NewVertexDeclaration(m_Context, stream_declaration);

    dmGraphics::EnableVertexBuffer(m_Context, vertex_buffer, 0);
    dmGraphics::EnableVertexDeclaration(m_Context, vertex_declaration, 0);

    float p[] = { 0.0f, 1.0f, 2.0f, 5.0f, 6.0f, 7.0f };
    ASSERT_EQ(sizeof(p) / 2, m_NullContext->m_VertexStreams[0][0].m_Size);
    ASSERT_EQ(0, memcmp(p, m_NullContext->m_VertexStreams[0][0].m_Source, 3 * sizeof(float)));
    float uv[] = { 3.0f, 4.0f, 8.0f, 9.0f };
    ASSERT_EQ(sizeof(uv) / 2, m_NullContext->m_VertexStreams[0][1].m_Size);
    ASSERT_EQ(0, memcmp(uv, m_NullContext->m_VertexStreams[0][1].m_Source, 2 * sizeof(float)));

    dmGraphics::DisableVertexDeclaration(m_Context, vertex_declaration);

    ASSERT_EQ(0u, m_NullContext->m_VertexStreams[0][0].m_Size);
    ASSERT_EQ(0u, m_NullContext->m_VertexStreams[0][1].m_Size);

    dmGraphics::DeleteVertexDeclaration(vertex_declaration);
    dmGraphics::DeleteVertexBuffer(vertex_buffer);
    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);
}

TEST_F(dmGraphicsTest, Drawing)
{
    float v[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f };
    uint32_t i[] = { 0, 1, 2, 2, 1, 0 };

    dmGraphics::HVertexStreamDeclaration stream_declaration = dmGraphics::NewVertexStreamDeclaration(m_Context);
    dmGraphics::AddVertexStream(stream_declaration, "position", 3, dmGraphics::TYPE_FLOAT, false);
    dmGraphics::AddVertexStream(stream_declaration, "uv",       2, dmGraphics::TYPE_FLOAT, false);

    dmGraphics::HVertexDeclaration vd = dmGraphics::NewVertexDeclaration(m_Context, stream_declaration);
    dmGraphics::HVertexBuffer vb = dmGraphics::NewVertexBuffer(m_Context, sizeof(v), v, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    dmGraphics::HIndexBuffer ib = dmGraphics::NewIndexBuffer(m_Context, sizeof(i), i, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

    dmGraphics::EnableVertexBuffer(m_Context, vb, 0);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, 0);
    dmGraphics::DrawElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, dmGraphics::TYPE_UNSIGNED_INT, ib, 1);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, 0);
    dmGraphics::DrawElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 3, 6, dmGraphics::TYPE_UNSIGNED_INT, ib, 1);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, 0);
    dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, 1);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::DisableVertexBuffer(m_Context, vb);

    dmGraphics::DeleteIndexBuffer(ib);
    dmGraphics::DeleteVertexBuffer(vb);
    dmGraphics::DeleteVertexDeclaration(vd);
    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);
}

TEST_F(dmGraphicsTest, TestUniformBuffers)
{
    const char* vertex_data = ""
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(1.0);\n"
        "}\n";

    const char* fragment_data = ""
        "uniform buf\n"
        "{\n"
        "    float f;\n"
        "    vec2 v2;\n"
        "    vec3 v3;\n"
        "    vec4 v4;\n"
        "};\n"

        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    struct ubo_data_t
    {
        float f;
        float v2[2];
        float v3[3];
        float v4[4];
    } ubo_data;

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex_data, (uint32_t) strlen(vertex_data));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment_data, (uint32_t) strlen(fragment_data));

    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);

    uint32_t offset = 0;

    dmGraphics::ShaderDesc::ResourceMember buf_members[4] = {};
    buf_members[0].m_Name                     = "f";
    buf_members[0].m_NameHash                 = dmHashString64(buf_members[0].m_Name);
    buf_members[0].m_Offset                   = offset;
    buf_members[0].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
    offset += sizeof(float);

    buf_members[1].m_Name                     = "v2";
    buf_members[1].m_NameHash                 = dmHashString64(buf_members[1].m_Name);
    buf_members[1].m_Offset                   = offset;
    buf_members[1].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC2;
    offset += sizeof(float) * 2;

    buf_members[2].m_Name                     = "v3";
    buf_members[2].m_NameHash                 = dmHashString64(buf_members[2].m_Name);
    buf_members[2].m_Offset                   = offset;
    buf_members[2].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC3;
    offset += sizeof(float) * 3;

    buf_members[3].m_Name                     = "v4";
    buf_members[3].m_NameHash                 = dmHashString64(buf_members[3].m_Name);
    buf_members[3].m_Offset                   = offset;
    buf_members[3].m_Type.m_Type.m_ShaderType = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
    offset += sizeof(float) * 4;

    shader_desc_builder.AddTypeMemberWithMembers("buf", buf_members, DM_ARRAY_SIZE(buf_members));
    shader_desc_builder.AddUniformBuffer("buf", 0, 0, offset);

    dmGraphics::ShaderDesc* shader = shader_desc_builder.Get();
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Context, shader, 0, 0);
    const dmGraphics::ShaderMeta* program_meta = dmGraphics::GetShaderMeta(program);

    dmGraphics::NullProgram* null_program = (dmGraphics::NullProgram*) program;
    ASSERT_EQ(1, null_program->m_UniformBuffers.Size());

    // Create a ubo with the same layout
    {
        dmGraphics::UniformBufferLayout& pgm_ubo_0 = null_program->m_BaseProgram.m_UniformBufferLayouts[0];

        dmGraphics::HUniformBuffer ubo = dmGraphics::NewUniformBuffer(m_Context, pgm_ubo_0);
        dmGraphics::NullUniformBuffer* null_ubo = (dmGraphics::NullUniformBuffer*) ubo;

        dmGraphics::EnableProgram(m_Context, program);

        // Set the initial v4s data
        Vector4 constant(1.0f, 2.0f, 3.0f, 4.0f);
        const dmGraphics::Uniform* uniform_v4 = dmGraphics::GetUniform(program, dmHashString64("v4"));
        uint32_t uniform_v4_buffer_offset = UNIFORM_LOCATION_GET_OP2(uniform_v4->m_Location);
        dmGraphics::SetConstantV4(m_Context, &constant, 1, uniform_v4->m_Location);

        dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 0, 0);
        ASSERT_FALSE(null_ubo->m_UsedInDraw); // UBO not bound yet

        uint8_t* per_draw_uniforms = m_NullContext->m_PerDrawUniformData.Begin();
        float* written_floats = (float*) (per_draw_uniforms + uniform_v4_buffer_offset);

        ASSERT_NEAR(written_floats[0], 1.0f, EPSILON);
        ASSERT_NEAR(written_floats[1], 2.0f, EPSILON);
        ASSERT_NEAR(written_floats[2], 3.0f, EPSILON);
        ASSERT_NEAR(written_floats[3], 4.0f, EPSILON);

        // Bind a UBO and draw again, it should take values from the UBO instead.
        ubo_data.v4[0] = 5.0f;
        ubo_data.v4[1] = 6.0f;
        ubo_data.v4[2] = 7.0f;
        ubo_data.v4[3] = 8.0f;

        dmGraphics::SetUniformBuffer(m_Context, ubo, 0, sizeof(ubo_data), &ubo_data);
        dmGraphics::EnableUniformBuffer(m_Context, ubo, 0, 0);

        dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 0, 0);
        ASSERT_TRUE(null_ubo->m_UsedInDraw);

        ASSERT_NEAR(written_floats[0], 5.0f, EPSILON);
        ASSERT_NEAR(written_floats[1], 6.0f, EPSILON);
        ASSERT_NEAR(written_floats[2], 7.0f, EPSILON);
        ASSERT_NEAR(written_floats[3], 8.0f, EPSILON);

        dmGraphics::DisableUniformBuffer(m_Context, ubo);
        dmGraphics::DisableProgram(m_Context);
        dmGraphics::DeleteUniformBuffer(m_Context, ubo);
    }

    // Create a ubo that doesn't match
    {
        dmGraphics::UniformBufferLayout mismatched_layout = {};
        mismatched_layout.m_Size = 1337;
        mismatched_layout.m_Hash = -1;

        dmGraphics::HUniformBuffer ubo = dmGraphics::NewUniformBuffer(m_Context, mismatched_layout);
        dmGraphics::NullUniformBuffer* null_ubo = (dmGraphics::NullUniformBuffer*) ubo;

        dmGraphics::EnableUniformBuffer(m_Context, ubo, 0, 0);
        dmGraphics::EnableProgram(m_Context, program);

        dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 0, 0);
        ASSERT_FALSE(null_ubo->m_UsedInDraw);

        dmGraphics::DisableUniformBuffer(m_Context, ubo);
        dmGraphics::DisableProgram(m_Context);
        dmGraphics::DeleteUniformBuffer(m_Context, ubo);
    }

    dmGraphics::DeleteProgram(m_Context, program);
}

TEST_F(dmGraphicsTest, TestProgram)
{
    const char* vertex_data = ""
            "uniform mediump mat4 view_proj;\n"
            "uniform mediump mat4 world;\n"

            "attribute mediump vec4 position;\n"
            "attribute mediump vec2 texcoord0;\n"

            "varying mediump vec2 var_texcoord0;\n"

            "void main()\n"
            "{\n"
            "    // NOTE: world isn't used here. Sprite positions are already transformed\n"
            "    // prior to rendering but the world-transform is set for sorting.\n"
            "   gl_Position = view_proj * vec4(position.xyz, 1.0);\n"
            "   var_texcoord0 = texcoord0;\n"
            "}\n";
    const char* fragment_data = ""
            "varying mediump vec4 position;\n"
            "varying mediump vec2 var_texcoord0;\n"

            "uniform lowp sampler2D texture_sampler;\n"
            "uniform lowp vec4 tint;\n"

            "void main()\n"
            "{\n"
            "    // Pre-multiply alpha since all runtime textures already are\n"
            "    lowp vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);\n"
            "    gl_FragColor = texture2D(texture_sampler, var_texcoord0.xy) * tint_pm;\n"
            "}\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "texcoord0", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);

    shader_desc_builder.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shader_desc_builder.AddTypeMember("world", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shader_desc_builder.AddTypeMember("tint", dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);

    shader_desc_builder.AddUniformBuffer("view_proj", 0, 0, dmGraphics::GetShaderTypeSize(dmGraphics::ShaderDesc::SHADER_TYPE_MAT4));
    shader_desc_builder.AddUniformBuffer("world", 1, 1, dmGraphics::GetShaderTypeSize(dmGraphics::ShaderDesc::SHADER_TYPE_MAT4));
    shader_desc_builder.AddUniformBuffer("tint", 2, 2, dmGraphics::GetShaderTypeSize(dmGraphics::ShaderDesc::SHADER_TYPE_VEC4));

    shader_desc_builder.AddTexture("texture_sampler", 3, dmGraphics::ShaderDesc::SHADER_TYPE_SAMPLER2D);

    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex_data, (uint32_t) strlen(vertex_data));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment_data, (uint32_t) strlen(fragment_data));

    dmGraphics::ShaderDesc* shader = shader_desc_builder.Get();

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Context, shader, 0, 0);
    ASSERT_EQ(4u, dmGraphics::GetUniformCount(program));

    const dmGraphics::Uniform* view_proj       = dmGraphics::GetUniform(program, dmHashString64("view_proj"));
    const dmGraphics::Uniform* world           = dmGraphics::GetUniform(program, dmHashString64("world"));
    const dmGraphics::Uniform* texture_sampler = dmGraphics::GetUniform(program, dmHashString64("texture_sampler"));
    const dmGraphics::Uniform* tint            = dmGraphics::GetUniform(program, dmHashString64("tint"));

    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, view_proj->m_Location);
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, world->m_Location);
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, texture_sampler->m_Location);
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, tint->m_Location);

    ASSERT_STREQ("view_proj", view_proj->m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_MAT4, view_proj->m_Type);

    ASSERT_STREQ("world", world->m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_MAT4, world->m_Type);

    ASSERT_STREQ("texture_sampler", texture_sampler->m_Name);
    ASSERT_EQ(dmGraphics::TYPE_SAMPLER_2D, texture_sampler->m_Type);

    ASSERT_STREQ("tint", tint->m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, tint->m_Type);

    uint32_t attribute_count = dmGraphics::GetAttributeCount(program);
    ASSERT_EQ(2, attribute_count);
    {
        dmhash_t         name_hash;
        dmGraphics::Type type;
        uint32_t         element_count;
        uint32_t         num_values;
        int32_t          location;
        dmGraphics::GetAttribute(program, 0, &name_hash, &type, &element_count, &num_values, &location);

        ASSERT_EQ(dmHashString64("position"), name_hash);
        ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, type);
        ASSERT_EQ(4, element_count);
        ASSERT_EQ(1, num_values);
        ASSERT_EQ(0, location);

        dmGraphics::GetAttribute(program, 1, &name_hash, &type, &element_count, &num_values, &location);

        ASSERT_EQ(dmHashString64("texcoord0"), name_hash);
        ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC2, type);
        ASSERT_EQ(2, element_count);
        ASSERT_EQ(1, num_values);
        ASSERT_EQ(1, location);
    }

    dmGraphics::EnableProgram(m_Context, program);
    Vector4 constant(1.0f, 2.0f, 3.0f, 4.0f);
    dmGraphics::SetConstantV4(m_Context, &constant, 1, tint->m_Location);
    Vector4 matrix[4] = {   Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                            Vector4(5.0f, 6.0f, 7.0f, 8.0f),
                            Vector4(9.0f, 10.0f, 11.0f, 12.0f),
                            Vector4(13.0f, 14.0f, 15.0f, 16.0f) };
    dmGraphics::SetConstantM4(m_Context, matrix, 1, view_proj->m_Location);
    char* program_data_vs = new char[1024];
    *program_data_vs = 0;

    char* program_data_fs = new char[1024];
    *program_data_fs = 0;

    dmGraphics::ShaderDescBuilder shader_desc_reload;
    shader_desc_reload.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, program_data_vs, 1024);
    shader_desc_reload.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, program_data_fs, 1024);
    dmGraphics::ReloadProgram(m_Context, program, shader_desc_reload.Get());

    delete [] program_data_vs;
    delete [] program_data_fs;

    dmGraphics::DisableProgram(m_Context);
    dmGraphics::DeleteProgram(m_Context, program);
}

TEST_F(dmGraphicsTest, TestComputeProgram)
{
    const char* compute_data = ""
        "#version 430 core\n"
        "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        "layout(location = 0) uniform vec4 my_uniform;\n"
        "void main() {\n"
        "}\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddTypeMember("my_uniform", dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shader_desc_builder.AddUniform("my_uniform", 0, 0);

    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, compute_data, (uint32_t) strlen(compute_data));

    dmGraphics::ShaderDesc* compute_desc = shader_desc_builder.Get();

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Context, compute_desc, 0, 0);

    const dmGraphics::Uniform* my_uniform = dmGraphics::GetUniform(program, dmHashString64("my_uniform"));

    ASSERT_EQ(1, dmGraphics::GetUniformCount(program));
    ASSERT_EQ(0, my_uniform->m_Location);

    ASSERT_STREQ("my_uniform", my_uniform->m_Name);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, my_uniform->m_Type);

    dmGraphics::DeleteProgram(m_Context, program);
}

template <typename T>
struct VectorTypeContainer
{
    T m_Scalar;
    T m_Vec2[2];
    T m_Vec3[3];
    T m_Vec4[4];
    T m_Mat2[4];
    T m_Mat3[9];
    T m_Mat4[16];
};

static inline void AddAttribute(dmGraphics::VertexAttributeInfos& infos,
    float* values, uint32_t num_values,
    dmGraphics::VertexAttribute::SemanticType semantic_type,
    dmGraphics::VertexAttribute::DataType data_type,
    dmGraphics::VertexAttribute::VectorType value_vector_type,
    dmGraphics::VertexAttribute::VectorType vector_type)
{
    infos.m_Infos[infos.m_NumInfos].m_SemanticType    = semantic_type;
    infos.m_Infos[infos.m_NumInfos].m_DataType        = data_type;
    infos.m_Infos[infos.m_NumInfos].m_VectorType      = vector_type;
    infos.m_Infos[infos.m_NumInfos].m_ValuePtr        = (uint8_t*) values;
    infos.m_Infos[infos.m_NumInfos].m_ValueVectorType = value_vector_type;
    infos.m_NumInfos++;
}

static void AssertVectorTypeContainerFloat(const VectorTypeContainer<float>& expected, const VectorTypeContainer<float>& actual)
{
    ASSERT_NEAR(expected.m_Scalar, actual.m_Scalar, EPSILON);
    ASSERT_VECF(expected.m_Vec2, actual.m_Vec2, 2);
    ASSERT_VECF(expected.m_Vec3, actual.m_Vec3, 3);
    ASSERT_VECF(expected.m_Vec4, actual.m_Vec4, 4);
    ASSERT_VECF(expected.m_Mat2, actual.m_Mat2, 4);
    ASSERT_VECF(expected.m_Mat3, actual.m_Mat3, 9);
    ASSERT_VECF(expected.m_Mat4, actual.m_Mat4, 16);
}

static void RunAllAttributeTest(float* values, uint32_t num_values, dmGraphics::VertexAttribute::SemanticType semantic_type, dmGraphics::VertexAttribute::DataType data_type, dmGraphics::VertexAttribute::VectorType value_vector_type, const VectorTypeContainer<float>& expected)
{
    dmGraphics::VertexAttributeInfos attribute_infos;
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT2);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3);
    AddAttribute(attribute_infos, values, num_values, semantic_type, data_type, value_vector_type, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4);
    attribute_infos.m_VertexStride = sizeof(expected);

    dmGraphics::WriteAttributeParams params = {};
    params.m_VertexAttributeInfos           = &attribute_infos;

    VectorTypeContainer<float> actual;

    dmGraphics::WriteAttributes((uint8_t*) &actual, 0, 1, params);
    AssertVectorTypeContainerFloat(expected, actual);
}

TEST_F(dmGraphicsTest, VertexAttributeDataTypeConversion)
{
    dmGraphics::VertexAttributeInfos attribute_infos;
    AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);

    dmGraphics::WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &attribute_infos;

    // Unsigned byte
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE;
        attribute_infos.m_VertexStride = sizeof(uint8_t) * 4;
        float position_values[] = {128.0, 255.0};
        uint8_t expected[4]     = {128,   255, 0, 1};
        uint8_t actual[4]       = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);
        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }

    // Signed byte
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_BYTE;
        attribute_infos.m_VertexStride        = sizeof(int8_t) * 4;
        float position_values[]               = {-32.0, -16.0};
        int8_t expected[4]                    = {-32,   -16, 0, 1};
        int8_t actual[4]                      = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }

    // Unsigned short
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT;
        attribute_infos.m_VertexStride        = sizeof(uint16_t) * 4;
        float position_values[]               = {32768.0, 65535.0};
        uint16_t expected[4]                  = {32768,   65535, 0, 1};
        uint16_t actual[4]                    = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }

    // Signed short
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_SHORT;
        attribute_infos.m_VertexStride        = sizeof(int16_t) * 4;
        float position_values[]               = {-16384.0, -32768.0};
        int16_t expected[4]                   = {-16384,   -32768, 0, 1};
        int16_t actual[4]                     = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }

    // Unsigned int
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT;
        attribute_infos.m_VertexStride        = sizeof(uint32_t) * 4;
        float position_values[]               = {128000.0, 13371337.0};
        uint32_t expected[4]                  = {128000,   13371337, 0, 1};
        uint32_t actual[4]                    = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }

    // Signed int
    {
        attribute_infos.m_Infos[0].m_DataType = dmGraphics::VertexAttribute::TYPE_INT;
        attribute_infos.m_VertexStride        = sizeof(int32_t) * 4;
        float position_values[]               = {-128000.0, -99999.0};
        int32_t expected[4]                   = {-128000,   -99999, 0, 1};
        int32_t actual[4]                     = {};

        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
        ASSERT_VEC(expected, actual, 4);
    }
}

TEST_F(dmGraphicsTest, VertexAttributeConversionRulesSemanticTypeNone)
{
    // Passing in no data
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            0.0,
           {0.0, 0.0},
           {0.0, 0.0, 0.0},
           {0.0, 0.0, 0.0, 0.0},
           // mat2
           {1.0, 0.0,
            0.0, 1.0},
           // mat3
           {1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0},
           // mat4
           {1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0},
        };

        RunAllAttributeTest(0, 0,
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR,
            expected);
    }

    // Passing in scalar value should populate all elements of vectors and the diagonal of matrices
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.1},
           {1.1, 1.1, 1.1},
           {1.1, 1.1, 1.1, 1.1},
           // mat2
           {1.1, 0.0,
            0.0, 1.1},
           // mat3
           {1.1, 0.0, 0.0,
            0.0, 1.1, 0.0,
            0.0, 0.0, 1.1},
           // mat4
           {1.1, 0.0, 0.0, 0.0,
            0.0, 1.1, 0.0, 0.0,
            0.0, 0.0, 1.1, 0.0,
            0.0, 0.0, 0.0, 1.1},
        };

        float values[] = { 1.1 };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR,
            expected);
    }

    // Passing in vec2 should populate at most first two elements in scalars, vectors and matrices and then fill rest with zeroes
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 0.0},
           {1.1, 1.2, 0.0, 0.0},
           // mat2
           {1.1, 1.2,
            0.0, 0.0},
           // mat3
           {1.1, 1.2, 0.0,
            0.0, 0.0, 0.0,
            0.0, 0.0, 0.0},
           // mat4
           {1.1, 1.2, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0},
        };

        float values[] = { 1.1, 1.2 };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2,
            expected);
    }

    // Passing in vec3 should populate at most first three elements in scalars, vectors and matrices and then fill rest with zeroes
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 1.3},
           {1.1, 1.2, 1.3, 0.0},
           // mat2
           {1.1, 1.2,
            1.3, 0.0},
           // mat3
           {1.1, 1.2, 1.3,
            0.0, 0.0, 0.0,
            0.0, 0.0, 0.0},
           // mat4
           {1.1, 1.2, 1.3, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0},
        };

        float values[] = { 1.1, 1.2, 1.3 };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3,
            expected);
    }

    // Passing in vec4 should populate at most first four elements in scalars, vectors and matrices and then fill rest with zeroes
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 1.3},
           {1.1, 1.2, 1.3, 4.4},
           // mat2
           {1.1, 1.2,
            1.3, 4.4},
           // mat3
           {1.1, 1.2, 1.3,
            4.4, 0.0, 0.0,
            0.0, 0.0, 0.0},
           // mat4
           {1.1, 1.2, 1.3, 4.4,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0},
        };

        float values[] = { 1.1, 1.2, 1.3, 4.4 };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4,
            expected);
    }

    // Passing in mat2 should populate at most first four elements in scalars, vectors
    // and as much of the top-level corner of matrices as possible and identity for the rest
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 1.3},
           {1.1, 1.2, 1.3, 1.4},
           // mat2
           {1.1, 1.2,
            1.3, 1.4},
           // mat3
           {1.1, 1.2, 0.0,
            1.3, 1.4, 0.0,
            0.0, 0.0, 1.0},
           // mat4
           {1.1, 1.2, 0.0, 0.0,
            1.3, 1.4, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0},
        };

        float values[] = {
            1.1, 1.2,
            1.3, 1.4
        };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_MAT2,
            expected);
    }

    // Passing in mat3 should populate at most first four elements in scalars, vectors and mat2
    // and as much of the top-level corner of mat4 matrices as possible and identity for the rest
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 1.3},
           {1.1, 1.2, 1.3, 1.4},
           // mat2
           {1.1, 1.2,
            1.4, 1.5},
           // mat3
           {1.1, 1.2, 1.3,
            1.4, 1.5, 1.6,
            1.7, 1.8, 1.9},
           // mat4
           {1.1, 1.2, 1.3, 0.0,
            1.4, 1.5, 1.6, 0.0,
            1.7, 1.8, 1.9, 0.0,
            0.0, 0.0, 0.0, 1.0},
        };

        float values[] = {
            1.1, 1.2, 1.3,
            1.4, 1.5, 1.6,
            1.7, 1.8, 1.9
        };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3,
            expected);
    }

    // Passing in mat4 should populate at most first four elements in scalars, vectors, mat2 and mat3
    {
        VectorTypeContainer<float> expected = {
            // scalar + vector types
            1.1,
           {1.1, 1.2},
           {1.1, 1.2, 1.3},
           {1.1, 1.2, 1.3, 1.4},
           // mat2
           {1.1, 1.2,
            1.5, 1.6},
           // mat3
           {1.1, 1.2, 1.3,
            1.5, 1.6, 1.7,
            1.9, 2.0, 2.1},
           // mat4
           {1.1, 1.2, 1.3, 1.4,
            1.5, 1.6, 1.7, 1.8,
            1.9, 2.0, 2.1, 2.2,
            2.3, 2.4, 2.5, 2.6},
        };

        float values[] = {
            1.1, 1.2, 1.3, 1.4,
            1.5, 1.6, 1.7, 1.8,
            1.9, 2.0, 2.1, 2.2,
            2.3, 2.4, 2.5, 2.6
        };
        RunAllAttributeTest(values, DM_ARRAY_SIZE(values),
            dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,
            dmGraphics::VertexAttribute::TYPE_FLOAT,
            dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4,
            expected);
    }
}

// Test multiple channels for engine provided vertex attributes
TEST_F(dmGraphicsTest, VertexAttributeEngineProvidedData)
{
    float attribute_0_data[] = {  1.1,  1.2,  1.3,  1.4 };
    float attribute_1_data[] = { -1.1, -1.2, -1.3, -1.4 };

    dmGraphics::VertexAttributeInfos attribute_infos;
    AddAttribute(attribute_infos, attribute_0_data, 4, dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
    AddAttribute(attribute_infos, attribute_1_data, 4, dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
    AddAttribute(attribute_infos,                0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
    attribute_infos.m_VertexStride = sizeof(float) * 4 * 3;

    dmGraphics::WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &attribute_infos;

    // Provide no position channels at all!
    {
        float expected[3][4] = {
            {  1.1,  1.2,  1.3,  1.4 },
            { -1.1, -1.2, -1.3, -1.4 },
            {  0.0,  0.0,  0.0,  1.0 }, // <- the attribute has no data source, so it will be constructed with a 1.0 as W!
        };
        float actual[3][4] = {};

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);

        ASSERT_VECF(expected[0], actual[0], 4);
        ASSERT_VECF(expected[1], actual[1], 4);
        ASSERT_VECF(expected[2], actual[2], 4);
    }

    // Provide a single position data channel from the engine
    {
        float position_values[] = {2.1, 2.2, 2.3};
        float expected[3][4] = {
            {  2.1,  2.2,  2.3,  1.0 },
            { -1.1, -1.2, -1.3, -1.4 },
            {  0.0,  0.0,  0.0,  1.0 }, // <- the attribute has no data source, so it will be constructed with a 1.0 as W!
        };
        float actual[3][4] = {};

        // We provide one channel of data, which means the other two should be filled with the fallback data
        const float* position_values_channel[] = { position_values };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);

        ASSERT_VECF(expected[0], actual[0], 4);
        ASSERT_VECF(expected[1], actual[1], 4);
        ASSERT_VECF(expected[2], actual[2], 4);
    }

    // Provide two position data channel
    {
        float position_values_0[] = {2.1, 2.2, 2.3};
        float position_values_1[] = {3.1, 3.2, 3.3};

        float expected[3][4] = {
            {2.1, 2.2, 2.3, 1.0},
            {3.1, 3.2, 3.3, 1.0}, // <- this has changed
            {0.0, 0.0, 0.0, 1.0 }, // <- the attribute has no data source, so it will be constructed with a 1.0 as W!
        };
        float actual[3][4] = {};

        // We provide TWO channel of data, which means whatever channels comes after will get data from channel 0
        const float* position_values_channel[] = { position_values_0, position_values_1 };
        dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 2, false);

        dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);

        ASSERT_VECF(expected[0], actual[0], 4);
        ASSERT_VECF(expected[1], actual[1], 4);
        ASSERT_VECF(expected[2], actual[2], 4);
    }
}

// position, color and tangent should have one as W, if there is not enough source data to copy from
TEST_F(dmGraphicsTest, VertexAttributeConversionRulesSemanticTypeOneAsW)
{
    // Position semantic
    {
        dmGraphics::VertexAttributeInfos attribute_infos;
        AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
        attribute_infos.m_VertexStride = sizeof(float) * 4;

        dmGraphics::WriteAttributeParams params = {};
        params.m_VertexAttributeInfos = &attribute_infos;

        // No values available whatsoever
        {
            float expected[4] = {0.0, 0.0, 0.0, 1.0};
            float actual[4]   = {};

            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }

        // Vec2 values
        {
            float position_values[] = {1.1, 1.2};
            float expected[4]       = {1.1, 1.2, 0.0, 1.0};
            float actual[4]         = {};

            const float* position_values_channel[] = { position_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);

            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }

        // Vec3 values
        {
            float position_values[] = {1.1, 1.2, 1.3};
            float expected[4]       = {1.1, 1.2, 1.3, 1.0};
            float actual[4]         = {};

            const float* position_values_channel[] = { position_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, position_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);

            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }
    }

    // Color semantic
    {
        // No values available whatsoever
        {
            dmGraphics::VertexAttributeInfos attribute_infos;
            AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR);
            AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2);
            AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3);
            AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
            attribute_infos.m_VertexStride = sizeof(float) * (1 + 2 + 3 + 4);

            dmGraphics::WriteAttributeParams params = {};
            params.m_VertexAttributeInfos = &attribute_infos;

            VectorTypeContainer<float> actual = {};
            VectorTypeContainer<float> expected = {
                1.0,
                {1.0, 1.0},
                {1.0, 1.0, 1.0},
                {1.0, 1.0, 1.0, 1.0}
            };

            dmGraphics::WriteAttributes((uint8_t*) &actual, 0, 1, params);
            AssertVectorTypeContainerFloat(expected, actual);
        }

        dmGraphics::VertexAttributeInfos attribute_infos;
        AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
        attribute_infos.m_VertexStride = sizeof(float) * 4;

        dmGraphics::WriteAttributeParams params = {};
        params.m_VertexAttributeInfos = &attribute_infos;

        // Vec2 values
        {
            float color_values[] = {1.1, 1.2};
            float expected[4]    = {1.1, 1.2, 0.0, 1.0};
            float actual[4]      = {};

            const float* color_values_channel[] = { color_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_Colors, color_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);
            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }

        // Vec3 values
        {
            float color_values[] = {1.1, 1.2, 1.3};
            float expected[4]    = {1.1, 1.2, 1.3, 1.0};
            float actual[4]      = {};

            const float* color_values_channel[] = { color_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_Colors, color_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }
    }

    // Tangent semantic
    {
        dmGraphics::VertexAttributeInfos attribute_infos;
        AddAttribute(attribute_infos, 0, 0, dmGraphics::VertexAttribute::SEMANTIC_TYPE_TANGENT, dmGraphics::VertexAttribute::TYPE_FLOAT, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4);
        attribute_infos.m_VertexStride = sizeof(float) * 4;

        dmGraphics::WriteAttributeParams params = {};
        params.m_VertexAttributeInfos = &attribute_infos;

        // No values available whatsoever
        {
            float expected[4] = {0.0, 0.0, 0.0, 1.0};
            float actual[4]   = {};

            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }

        // Vec2 values
        {
            float tangent_values[]  = {1.1, 1.2};
            float expected[4]       = {1.1, 1.2, 0.0, 1.0};
            float actual[4]         = {};

            const float* tangent_values_channel[] = { tangent_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_Tangents, tangent_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, 1, false);
            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }

        // Vec3 values
        {
            float tangent_values[]  = {1.1, 1.2, 1.3};
            float expected[4]       = {1.1, 1.2, 1.3, 1.0};
            float actual[4]         = {};

            const float* tangent_values_channel[] = { tangent_values };
            dmGraphics::SetWriteAttributeStreamDesc(&params.m_Tangents, tangent_values_channel, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
            dmGraphics::WriteAttributes((uint8_t*) actual, 0, 1, params);
            ASSERT_VECF(expected, actual, 4);
        }
    }
}

TEST_F(dmGraphicsTest, TestVertexAttributesGL3)
{
    const char* vertex_data = ""
        "in mediump vec4 position;\n"
        "in mediump vec2 texcoord0;\n"
        "in lowp vec4 color;\n"

        "uniform highp mat4 view_proj;\n"

        "out mediump vec2 var_texcoord0;\n"
        "out lowp vec4 var_color;\n"
        "void main()\n"
        "{\n"
        "    var_texcoord0 = texcoord0;\n"
        "    var_color = vec4(color.rgb * color.a, color.a);\n"
        "    gl_Position = view_proj * vec4(position.xyz, 1.0);\n"
        "}\n";

    const char* fragment_data = ""
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1.0);\n"
        "}\n";

    dmGraphics::ShaderDescBuilder shader_desc_builder;
    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "position", 0, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);
    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "texcoord0", 1, dmGraphics::ShaderDesc::SHADER_TYPE_VEC2);
    shader_desc_builder.AddInput(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, "color", 2, dmGraphics::ShaderDesc::SHADER_TYPE_VEC4);

    shader_desc_builder.AddTypeMember("view_proj", dmGraphics::ShaderDesc::SHADER_TYPE_MAT4);
    shader_desc_builder.AddUniform("view_proj", 0, 0);

    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, vertex_data, (uint32_t) strlen(vertex_data));
    shader_desc_builder.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, fragment_data, (uint32_t) strlen(fragment_data));

    dmGraphics::ShaderDesc* shader_desc = shader_desc_builder.Get();
    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Context, shader_desc, 0, 0);

    uint32_t attribute_count = dmGraphics::GetAttributeCount(program);
    ASSERT_EQ(3, attribute_count);
    {
        dmhash_t         name_hash;
        dmGraphics::Type type;
        uint32_t         element_count;
        uint32_t         num_values;
        int32_t          location;
        dmGraphics::GetAttribute(program, 0, &name_hash, &type, &element_count, &num_values, &location);

        ASSERT_EQ(dmHashString64("position"), name_hash);
        ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, type);
        ASSERT_EQ(4, element_count);
        ASSERT_EQ(1, num_values);
        ASSERT_EQ(0, location);

        dmGraphics::GetAttribute(program, 1, &name_hash, &type, &element_count, &num_values, &location);

        ASSERT_EQ(dmHashString64("texcoord0"), name_hash);
        ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC2, type);
        ASSERT_EQ(2, element_count);
        ASSERT_EQ(1, num_values);
        ASSERT_EQ(1, location);

        dmGraphics::GetAttribute(program, 2, &name_hash, &type, &element_count, &num_values, &location);

        ASSERT_EQ(dmHashString64("color"), name_hash);
        ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, type);
        ASSERT_EQ(4, element_count);
        ASSERT_EQ(1, num_values);
        ASSERT_EQ(2, location);
    }

    dmGraphics::DeleteProgram(m_Context, program);
}

TEST_F(dmGraphicsTest, TestViewport)
{
    dmGraphics::SetViewport(m_Context, 0, 0, WIDTH, HEIGHT);
}

TEST_F(dmGraphicsTest, TestTexture)
{
    dmGraphics::TextureCreationParams creation_params;
    dmGraphics::TextureParams params;

    creation_params.m_Width = WIDTH;
    creation_params.m_Height = HEIGHT;
    creation_params.m_OriginalWidth = WIDTH;
    creation_params.m_OriginalHeight = HEIGHT;

    params.m_DataSize = WIDTH * HEIGHT;
    params.m_Data = new char[params.m_DataSize];
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);
    dmGraphics::SetTexture(m_Context, texture, params);

    delete [] (char*)params.m_Data;
    ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(m_Context, texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(m_Context, texture));
    ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(m_Context, texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(m_Context, texture));
    dmGraphics::EnableTexture(m_Context, 0, 0, texture);
    dmGraphics::DisableTexture(m_Context, 0, texture);
    dmGraphics::DeleteTexture(m_Context, texture);
}

#if defined(DM_HAS_THREADS)

static void TestTextureAsyncCallback(dmGraphics::HTexture texture, void* user_data)
{
    assert(dmGraphics::GetOpaqueHandle(texture) != INVALID_OPAQUE_HANDLE);
    int* value = (int*)user_data;
    *value = 1;
}

TEST_F(dmGraphicsTest, TestTextureAsync)
{
    bool tmp_async_load = m_NullContext->m_UseAsyncTextureLoad;
    m_NullContext->m_UseAsyncTextureLoad = 1;

    dmGraphics::TextureCreationParams creation_params;
    dmGraphics::TextureParams params;

    creation_params.m_Width = WIDTH;
    creation_params.m_Height = HEIGHT;
    creation_params.m_OriginalWidth = WIDTH;
    creation_params.m_OriginalHeight = HEIGHT;

    params.m_DataSize = WIDTH * HEIGHT;
    params.m_Data = new char[params.m_DataSize];
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;

    const uint32_t TEXTURE_COUNT = 64;
    dmArray<dmGraphics::HTexture> textures;
    textures.SetCapacity(TEXTURE_COUNT);

    int* values = new int[TEXTURE_COUNT];
    memset(values, 0, sizeof(int)*TEXTURE_COUNT);

    bool all_complete = false;
    for (int i = 0; i < TEXTURE_COUNT; ++i)
    {
        textures.Push(dmGraphics::NewTexture(m_Context, creation_params));
        dmGraphics::SetTextureAsync(m_Context, textures[i], params, TestTextureAsyncCallback, (void*) (values + i));
    }

    uint64_t stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetMonotonicTime() < stop_time)
    {
        JobSystemUpdate(m_JobContext, 0);
        all_complete = true;
        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            if (values[i] == 0)
            {
                all_complete = false;
            }
        }
        dmTime::Sleep(20 * 1000);
    }
    ASSERT_TRUE(all_complete);

    delete [] (char*)params.m_Data;

    for (int i = 0; i < TEXTURE_COUNT; ++i)
    {
        ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(m_Context, textures[i]));
        ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(m_Context, textures[i]));
        ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(m_Context, textures[i]));
        ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(m_Context, textures[i]));
        dmGraphics::EnableTexture(m_Context, 0, 0, textures[i]);
        dmGraphics::DisableTexture(m_Context, 0, textures[i]);
        dmGraphics::DeleteTexture(m_Context, textures[i]);
    }

    all_complete = false;
    stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetMonotonicTime() < stop_time)
    {
        JobSystemUpdate(m_JobContext, 0);
        all_complete = true;
        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            if (dmGraphics::IsAssetHandleValid(m_Context, textures[i]))
                all_complete = false;
        }
        dmTime::Sleep(20 * 1000);
    }
    ASSERT_TRUE(all_complete);

    for (int i = 0; i < TEXTURE_COUNT; ++i)
    {
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, textures[i]));
    }

    m_NullContext->m_UseAsyncTextureLoad = tmp_async_load;
}

enum SyncronizedWaitCondition
{
    WAIT_CONDITION_UPLOAD,
    WAIT_CONDITION_DELETE,
};

static bool WaitUntilSyncronizedTextures(dmGraphics::HContext graphics_context, HJobContext job_thread, dmGraphics::HTexture* textures, uint32_t texture_count, SyncronizedWaitCondition cond)
{
    bool all_complete = false;
    uint64_t stop_time = dmTime::GetMonotonicTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetMonotonicTime() < stop_time)
    {
        JobSystemUpdate(job_thread, 0);
        all_complete = true;
        for (int i = 0; i < texture_count; ++i)
        {
            if (cond == WAIT_CONDITION_UPLOAD && dmGraphics::GetTextureStatusFlags(graphics_context, textures[i]) != dmGraphics::TEXTURE_STATUS_OK)
                all_complete = false;
            else if (cond == WAIT_CONDITION_DELETE && dmGraphics::IsAssetHandleValid(graphics_context, textures[i]))
                all_complete = false;
        }
        dmTime::Sleep(20 * 1000);
    }
    return all_complete;
}

TEST_F(dmGraphicsTest, TestTextureAsyncDelete)
{
    bool tmp_async_load = m_NullContext->m_UseAsyncTextureLoad;
    m_NullContext->m_UseAsyncTextureLoad = 1;

    dmGraphics::TextureCreationParams creation_params;
    dmGraphics::TextureParams params;

    creation_params.m_Width = WIDTH;
    creation_params.m_Height = HEIGHT;
    creation_params.m_OriginalWidth = WIDTH;
    creation_params.m_OriginalHeight = HEIGHT;

    params.m_DataSize = WIDTH * HEIGHT;
    params.m_Data = new char[params.m_DataSize];
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;

    const uint32_t TEXTURE_COUNT = 512;
    dmArray<dmGraphics::HTexture> textures;
    textures.SetCapacity(TEXTURE_COUNT);

    // Test 1: Deleting textures "in-flight" will not delete them immediately
    //         They will need to be force deleted by a flip
    {
        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            textures.Push(dmGraphics::NewTexture(m_Context, creation_params));
            dmGraphics::SetTextureAsync(m_Context, textures[i], params, 0, 0);

            // Immediately delete, so we simulate putting them on a post-delete-queue
            dmGraphics::DeleteTexture(m_Context, textures[i]);
        }

        // Trigger a flush of the post deletion textures by issuing a flip
        int wait_count = 10;
        while (wait_count-- > 0 && !m_NullContext->m_SetTextureAsyncState.m_PostDeleteTextures.Empty())
        {
            dmGraphics::Flip(m_Context);
            dmTime::Sleep(1000); // The sync/deletion is asynchronous, so we have to wait a little bit
        }

        ASSERT_EQ(0, m_NullContext->m_SetTextureAsyncState.m_PostDeleteTextures.Size());

        // Flush any lingering work
        JobSystemUpdate(m_JobContext, 0);

        // Make sure all are deleted
        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobContext, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_DELETE));
    }

    // Test 2: Simulate deleting textures async. This requires valid textures (i.e not pending)
    //         And that we continously update the job thread to finish the async jobs.
    {
        textures.SetSize(0);

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            textures.Push(dmGraphics::NewTexture(m_Context, creation_params));
            dmGraphics::SetTextureAsync(m_Context, textures[i], params, 0, 0);
        }

        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobContext, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_UPLOAD));

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            dmGraphics::DeleteTexture(m_Context, textures[i]);
        }

        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobContext, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_DELETE));

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, textures[i]));
        }
    }

    delete [] (char*) params.m_Data;

    m_NullContext->m_UseAsyncTextureLoad = tmp_async_load;
}
#endif // DM_HAS_THREADS

TEST_F(dmGraphicsSynchronousTest, TestSetTextureBounds)
{
    // We want to test the assert inside the graphics system.
    // However, the ASSERT_DEATH uses longjmp, which simply skips any desrtuctors,
    // and will leave mutexes locked
    ASSERT_FALSE(s_Asynchronous);
    dmGraphics::TextureCreationParams creation_params;
    dmGraphics::TextureParams params;

    creation_params.m_Width = WIDTH;
    creation_params.m_Height = HEIGHT;
    creation_params.m_OriginalWidth = WIDTH;
    creation_params.m_OriginalHeight = HEIGHT;

    params.m_DataSize  = WIDTH * HEIGHT;
    params.m_Data      = new char[params.m_DataSize];
    params.m_Width     = WIDTH;
    params.m_Height    = HEIGHT;
    params.m_Format    = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_SubUpdate = true;
    params.m_X         = WIDTH / 2;
    params.m_Y         = HEIGHT / 2;

    dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);
    ASSERT_DEATH(dmGraphics::SetTexture(m_Context, texture, params),"");

    delete [] (char*)params.m_Data;

    params.m_X         = 0;
    params.m_Y         = 0;
    params.m_Width     = WIDTH + 1;
    params.m_Height    = HEIGHT + 1;
    params.m_DataSize  = params.m_Width * params.m_Height;
    params.m_Data      = new char[params.m_DataSize];

    ASSERT_DEATH(dmGraphics::SetTexture(m_Context, texture, params),"");

    delete [] (char*)params.m_Data;

    dmGraphics::DeleteTexture(m_Context, texture);
}

TEST_F(dmGraphicsTest, TestMaxTextureSize)
{
    ASSERT_NE(dmGraphics::GetMaxTextureSize(m_Context), 0);
}

TEST_F(dmGraphicsTest, TestTextureDefautlOriginalDimension)
{
    dmGraphics::TextureCreationParams creation_params;
    dmGraphics::TextureParams params;

    creation_params.m_Width = WIDTH;
    creation_params.m_Height = HEIGHT;

    params.m_DataSize = WIDTH * HEIGHT;
    params.m_Data = new char[params.m_DataSize];
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);
    dmGraphics::SetTexture(m_Context, texture, params);

    delete [] (char*)params.m_Data;
    ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(m_Context, texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(m_Context, texture));
    ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(m_Context, texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(m_Context, texture));
    dmGraphics::EnableTexture(m_Context, 0, 0, texture);
    dmGraphics::DisableTexture(m_Context, 0, texture);
    dmGraphics::DeleteTexture(m_Context, texture);
}

static inline dmGraphics::RenderTargetCreationParams InitializeRenderTargetParams(uint32_t w, uint32_t h)
{
    dmGraphics::RenderTargetCreationParams p = {};

    #define SET_PARAM_DIM(p, cp) \
        p.m_Width  = w; \
        p.m_Height = h; \
        cp.m_Width  = w; \
        cp.m_Height = h;

    for (int i = 0; i < dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
    {
        SET_PARAM_DIM(p.m_ColorBufferParams[i], p.m_ColorBufferCreationParams[i]);
    }
    SET_PARAM_DIM(p.m_DepthBufferParams, p.m_DepthBufferCreationParams);
    SET_PARAM_DIM(p.m_StencilBufferParams, p.m_StencilBufferCreationParams);
    #undef SET_PARAM_DIM
    return p;
}

TEST_F(dmGraphicsTest, TestRenderTarget)
{
    dmGraphics::RenderTargetCreationParams params = InitializeRenderTargetParams(WIDTH, HEIGHT);

    // 4 color buffers + depth + stencil buffers == 8
    assert(dmGraphics::MAX_BUFFER_TYPE_COUNT == 6);

    params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_DepthBufferParams.m_Format    = dmGraphics::TEXTURE_FORMAT_DEPTH;
    params.m_StencilBufferParams.m_Format  = dmGraphics::TEXTURE_FORMAT_STENCIL;

    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_Context, flags, params);
    dmGraphics::SetRenderTarget(m_Context, target, 0);
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);

    uint32_t width = WIDTH;
    uint32_t height = HEIGHT;
    uint32_t data_size = sizeof(uint32_t) * width * height;
    char* data = new char[data_size];
    memset(data, 1, data_size);
    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    delete [] data;
    width *= 2;
    height *= 2;
    data_size = sizeof(uint32_t) * width * height;
    data = new char[data_size];
    memset(data, 1, data_size);
    dmGraphics::SetRenderTargetSize(m_Context, target, width, height);

    uint32_t target_width, target_height;
    GetRenderTargetSize(m_Context, target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);
    GetRenderTargetSize(m_Context, target, dmGraphics::BUFFER_TYPE_DEPTH_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);
    GetRenderTargetSize(m_Context, target, dmGraphics::BUFFER_TYPE_STENCIL_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);

    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    delete [] data;

    dmGraphics::SetRenderTarget(m_Context, 0x0, 0);
    dmGraphics::DeleteRenderTarget(m_Context, target);

    // Test multiple color attachments
    params.m_ColorBufferParams[1].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_ColorBufferParams[2].m_Format = dmGraphics::TEXTURE_FORMAT_RGB;

    flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT |
            dmGraphics::BUFFER_TYPE_COLOR1_BIT |
            dmGraphics::BUFFER_TYPE_COLOR2_BIT;

    target = dmGraphics::NewRenderTarget(m_Context, flags, params);
    dmGraphics::SetRenderTarget(m_Context, target, 0);
    dmGraphics::Clear(m_Context, dmGraphics::BUFFER_TYPE_COLOR0_BIT, 1, 1, 1, 1, 1.0f, 1);
    dmGraphics::Clear(m_Context, dmGraphics::BUFFER_TYPE_COLOR1_BIT, 2, 2, 2, 2, 1.0f, 1);
    dmGraphics::Clear(m_Context, dmGraphics::BUFFER_TYPE_COLOR2_BIT, 3, 3, 3, 3, 1.0f, 1);

    width = WIDTH;
    height = HEIGHT;

    GetRenderTargetSize(m_Context, target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);

    data_size              = sizeof(uint32_t) * width * height;
    uint32_t data_size_rgb = sizeof(uint32_t) * width * height * 3;

    data = new char[data_size];
    char* data_color1 = new char[data_size];
    char* data_color2 = new char[data_size_rgb];
    memset(data, 1, data_size);
    memset(data_color1, 2, data_size);
    memset(data_color2, 3, data_size_rgb);

    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    ASSERT_EQ(0, memcmp(data_color1, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[1], data_size));
    ASSERT_EQ(0, memcmp(data_color2, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[2], data_size_rgb));

    delete [] data;
    delete [] data_color1;
    delete [] data_color2;

    dmGraphics::SetRenderTarget(m_Context, 0x0, 0);
    dmGraphics::DeleteRenderTarget(m_Context, target);
}

TEST_F(dmGraphicsTest, TestGetRTAttachment)
{
    dmGraphics::RenderTargetCreationParams params = InitializeRenderTargetParams(WIDTH, HEIGHT);

    params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
    params.m_DepthBufferParams.m_Format    = dmGraphics::TEXTURE_FORMAT_DEPTH;
    params.m_StencilBufferParams.m_Format  = dmGraphics::TEXTURE_FORMAT_STENCIL;

    uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_Context, flags, params);
    dmGraphics::SetRenderTarget(m_Context, target, 0);
    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);

    dmGraphics::HTexture texture = dmGraphics::GetRenderTargetAttachment(m_Context, target, dmGraphics::ATTACHMENT_DEPTH);
    ASSERT_EQ((dmGraphics::HTexture)0x0, texture);

    texture = dmGraphics::GetRenderTargetAttachment(m_Context, target, dmGraphics::ATTACHMENT_STENCIL);
    ASSERT_EQ((dmGraphics::HTexture)0x0, texture);

    texture = dmGraphics::GetRenderTargetAttachment(m_Context, target, dmGraphics::ATTACHMENT_COLOR);
    ASSERT_NE((dmGraphics::HTexture)0x0, texture);

    char* texture_data = 0x0;
    dmGraphics::HandleResult res = dmGraphics::GetTextureHandle(0x0, (void**)&texture_data);
    ASSERT_EQ(dmGraphics::HANDLE_RESULT_ERROR, res);

    res = dmGraphics::GetTextureHandle(texture, (void**)&texture_data);
    ASSERT_EQ(dmGraphics::HANDLE_RESULT_OK, res);
    ASSERT_NE((char*)0x0, texture_data);

    uint32_t data_size = sizeof(uint32_t) * WIDTH * HEIGHT;
    char* data = new char[data_size];
    memset(data, 1, data_size);
    ASSERT_EQ(0, memcmp(data, texture_data, data_size));
    delete [] data;

    dmGraphics::SetRenderTarget(m_Context, 0x0, 0);
    dmGraphics::DeleteRenderTarget(m_Context, target);
}

TEST_F(dmGraphicsTest, TestRTDepthStencilTexture)
{
    dmGraphics::RenderTargetCreationParams params = InitializeRenderTargetParams(WIDTH, HEIGHT);
    params.m_DepthBufferParams.m_Format    = dmGraphics::TEXTURE_FORMAT_DEPTH;
    params.m_StencilBufferParams.m_Format  = dmGraphics::TEXTURE_FORMAT_STENCIL;
    params.m_DepthTexture                  = 1;
    params.m_StencilTexture                = 1;

    uint32_t flags = dmGraphics::BUFFER_TYPE_DEPTH_BIT | dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_Context, flags, params);
    dmGraphics::SetRenderTarget(m_Context, target, 0);

    float depth_value = 0.5f;
    uint32_t stencil_value = 127;

    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, depth_value, stencil_value);

    dmGraphics::HTexture depth_texture = dmGraphics::GetRenderTargetTexture(m_Context, target, dmGraphics::BUFFER_TYPE_DEPTH_BIT);
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, depth_texture));
    {
        float* texture_data = 0x0;
        dmGraphics::HandleResult res = dmGraphics::GetTextureHandle(depth_texture, (void**) &texture_data);
        ASSERT_EQ(dmGraphics::HANDLE_RESULT_OK, res);
        ASSERT_NE((float*)0x0, texture_data);

        for (int i = 0; i < WIDTH * HEIGHT; ++i)
        {
            ASSERT_NEAR(depth_value, texture_data[i], EPSILON);
        }
    }

    dmGraphics::HTexture stencil_texture = dmGraphics::GetRenderTargetTexture(m_Context, target, dmGraphics::BUFFER_TYPE_STENCIL_BIT);
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, stencil_texture));
    {
        uint32_t* texture_data = 0x0;
        dmGraphics::HandleResult res = dmGraphics::GetTextureHandle(stencil_texture, (void**)&texture_data);
        ASSERT_EQ(dmGraphics::HANDLE_RESULT_OK, res);
        ASSERT_NE((uint32_t*)0x0, texture_data);
        for (int i = 0; i < WIDTH * HEIGHT; ++i)
        {
            ASSERT_EQ(stencil_value, texture_data[i]);
        }
    }

    dmGraphics::SetRenderTarget(m_Context, 0x0, 0);
    dmGraphics::DeleteRenderTarget(m_Context, target);
}

TEST_F(dmGraphicsTest, TestMasks)
{
    dmGraphics::SetColorMask(m_Context, false, false, false, false);
    ASSERT_FALSE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_R);
    ASSERT_FALSE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_G);
    ASSERT_FALSE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_B);
    ASSERT_FALSE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_A);
    dmGraphics::SetColorMask(m_Context, true, true, true, true);
    ASSERT_TRUE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_R);
    ASSERT_TRUE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_G);
    ASSERT_TRUE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_B);
    ASSERT_TRUE(m_NullContext->m_PipelineState.m_WriteColorMask & dmGraphics::DM_GRAPHICS_STATE_WRITE_A);

    dmGraphics::SetDepthMask(m_Context, false);
    ASSERT_FALSE(m_NullContext->m_PipelineState.m_WriteDepth);
    dmGraphics::SetDepthMask(m_Context, true);
    ASSERT_TRUE(m_NullContext->m_PipelineState.m_WriteDepth);

    dmGraphics::SetStencilMask(m_Context, 0xff);
    ASSERT_EQ(0xff, m_NullContext->m_PipelineState.m_StencilWriteMask);
    dmGraphics::SetStencilMask(m_Context, ~0xff);
    ASSERT_EQ(0, m_NullContext->m_PipelineState.m_StencilWriteMask);
    dmGraphics::SetDepthFunc(m_Context, dmGraphics::COMPARE_FUNC_ALWAYS);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_ALWAYS, (dmGraphics::CompareFunc) m_NullContext->m_PipelineState.m_DepthTestFunc);
    dmGraphics::SetDepthFunc(m_Context, dmGraphics::COMPARE_FUNC_NEVER);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_NEVER, (dmGraphics::CompareFunc) m_NullContext->m_PipelineState.m_DepthTestFunc);

    dmGraphics::SetStencilFunc(m_Context, dmGraphics::COMPARE_FUNC_ALWAYS, 0xffffffff, 0x0);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_ALWAYS, (dmGraphics::CompareFunc) m_NullContext->m_PipelineState.m_StencilFrontTestFunc);
    ASSERT_EQ(0xff, m_NullContext->m_PipelineState.m_StencilReference);
    ASSERT_EQ(0x0, m_NullContext->m_PipelineState.m_StencilCompareMask);

    dmGraphics::SetStencilFunc(m_Context, dmGraphics::COMPARE_FUNC_NEVER, 0x0, 0xffffffff);
    ASSERT_EQ(dmGraphics::COMPARE_FUNC_NEVER, (dmGraphics::CompareFunc) m_NullContext->m_PipelineState.m_StencilFrontTestFunc);
    ASSERT_EQ(0x0, m_NullContext->m_PipelineState.m_StencilReference);
    ASSERT_EQ(0xff, m_NullContext->m_PipelineState.m_StencilCompareMask);

    dmGraphics::SetStencilOp(m_Context, dmGraphics::STENCIL_OP_KEEP, dmGraphics::STENCIL_OP_REPLACE, dmGraphics::STENCIL_OP_INVERT);
    ASSERT_EQ(dmGraphics::STENCIL_OP_KEEP,    (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpFail);
    ASSERT_EQ(dmGraphics::STENCIL_OP_REPLACE, (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpDepthFail);
    ASSERT_EQ(dmGraphics::STENCIL_OP_INVERT,  (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpPass);

    dmGraphics::SetStencilOp(m_Context, dmGraphics::STENCIL_OP_INVERT, dmGraphics::STENCIL_OP_KEEP, dmGraphics::STENCIL_OP_REPLACE);
    ASSERT_EQ(dmGraphics::STENCIL_OP_INVERT,  (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpFail);
    ASSERT_EQ(dmGraphics::STENCIL_OP_KEEP,    (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpDepthFail);
    ASSERT_EQ(dmGraphics::STENCIL_OP_REPLACE, (dmGraphics::StencilOp) m_NullContext->m_PipelineState.m_StencilFrontOpPass);
}

TEST_F(dmGraphicsTest, TestCloseCallback)
{
    // Stay open
    m_CloseData.m_ShouldClose = 0;
    // Request close
    m_NullContext->m_RequestWindowClose = 1;
    dmGraphics::Flip(m_Context);
    ASSERT_TRUE(dmGraphics::GetWindowStateParam(m_Context, dmPlatform::WINDOW_STATE_OPENED) ? true : false);
    // Accept close
    m_CloseData.m_ShouldClose = 1;
    dmGraphics::Flip(m_Context);
    ASSERT_FALSE(dmGraphics::GetWindowStateParam(m_Context, dmPlatform::WINDOW_STATE_OPENED));
}

TEST_F(dmGraphicsTest, TestTextureSupport)
{
    ASSERT_TRUE(dmGraphics::IsTextureFormatSupported(m_Context, dmGraphics::TEXTURE_FORMAT_LUMINANCE));
    ASSERT_FALSE(dmGraphics::IsTextureFormatSupported(m_Context, dmGraphics::TEXTURE_FORMAT_RGBA_BC7));
}

TEST_F(dmGraphicsTest, TestTextureFormatBPP)
{
    for(uint32_t i = 0; i < dmGraphics::TEXTURE_FORMAT_COUNT; ++i)
    {
        dmGraphics::TextureFormat format = (dmGraphics::TextureFormat) i;
        // ASTC doesn't have a "bits per pixel" value.
        if (dmGraphics::IsTextureFormatASTC(format))
        {
            continue;
        }
        ASSERT_NE(0, dmGraphics::GetTextureFormatBitsPerPixel(format));
    }
}

TEST_F(dmGraphicsTest, TestGetTextureParams)
{
    const uint32_t texture_width  = 16;
    const uint32_t texture_height = 16;

    // Texture 2D
    {
        dmGraphics::TextureCreationParams creation_params;
        dmGraphics::TextureParams params;

        creation_params.m_Width          = texture_width;
        creation_params.m_Height         = texture_height;
        creation_params.m_OriginalWidth  = texture_width;
        creation_params.m_OriginalHeight = texture_height;

        // Note: the m_MipMapCount value is ignored on null and opengl, it's only updated in SetTexture
        // creation_params.m_MipMapCount = 255;

        dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);

        // JG: I don't think we deal with mipmap data correctly in graphics_null,
        //     we only allocate data for _this_ SetTexture call and not reallocate the buffer
        //     depending on the actual data size..
        params.m_MipMap = 127;
        dmGraphics::SetTexture(m_Context, texture, params);

        ASSERT_EQ(1,                                         dmGraphics::GetTextureDepth(m_Context, texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D,               dmGraphics::GetTextureType(m_Context, texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(m_Context, texture));

        dmGraphics::DeleteTexture(m_Context, texture);
    }
    // Texture cube
    {
        dmGraphics::TextureCreationParams creation_params;
        dmGraphics::TextureParams params;

        creation_params.m_Type           = dmGraphics::TEXTURE_TYPE_CUBE_MAP;
        creation_params.m_Width          = texture_width;
        creation_params.m_Height         = texture_height;
        creation_params.m_OriginalWidth  = texture_width;
        creation_params.m_OriginalHeight = texture_height;

        dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);

        // JG: We don't really do bounds check for the depth either in graphics_null
        params.m_MipMap = 127;
        params.m_Depth  = 6;
        dmGraphics::SetTexture(m_Context, texture, params);

        ASSERT_EQ(params.m_Depth,                            dmGraphics::GetTextureDepth(m_Context, texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_CUBE_MAP,         dmGraphics::GetTextureType(m_Context, texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(m_Context, texture));

        dmGraphics::DeleteTexture(m_Context, texture);
    }

    // Texture 2D array
    {
        dmGraphics::TextureCreationParams creation_params;
        dmGraphics::TextureParams params;

        creation_params.m_Type           = dmGraphics::TEXTURE_TYPE_2D_ARRAY;
        creation_params.m_Width          = texture_width;
        creation_params.m_Height         = texture_height;
        creation_params.m_OriginalWidth  = texture_width;
        creation_params.m_OriginalHeight = texture_height;

        dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);

        // JG: We don't really do bounds check for the depth either in graphics_null
        params.m_MipMap = 127;
        params.m_Depth  = 1337;
        dmGraphics::SetTexture(m_Context, texture, params);

        ASSERT_EQ(params.m_Depth,                            dmGraphics::GetTextureDepth(m_Context, texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D_ARRAY,         dmGraphics::GetTextureType(m_Context, texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(m_Context, texture));

        dmGraphics::DeleteTexture(m_Context, texture);
    }
}

TEST_F(dmGraphicsTest, TestGraphicsHandles)
{
    const uint32_t texture_width  = 16;
    const uint32_t texture_height = 16;

    ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, 0));
    ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, 0xFFFF));

    // Test textures
    {
        dmGraphics::TextureCreationParams creation_params;
        dmGraphics::TextureParams params;

        creation_params.m_Width          = texture_width;
        creation_params.m_Height         = texture_height;
        creation_params.m_OriginalWidth  = texture_width;
        creation_params.m_OriginalHeight = texture_height;

        dmGraphics::HTexture texture = dmGraphics::NewTexture(m_Context, creation_params);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, texture));
        dmGraphics::DeleteTexture(m_Context, texture);

        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, texture));

        dmGraphics::HTexture texture_2 = dmGraphics::NewTexture(m_Context, creation_params);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, texture_2));
        ASSERT_NE(texture, texture_2);
        dmGraphics::DeleteTexture(m_Context, texture_2);
    }

    // Test render targets
    {
        dmGraphics::RenderTargetCreationParams params = InitializeRenderTargetParams(texture_width, texture_height);

        params.m_ColorBufferParams[0].m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
        params.m_DepthBufferParams.m_Format    = dmGraphics::TEXTURE_FORMAT_DEPTH;
        params.m_StencilBufferParams.m_Format  = dmGraphics::TEXTURE_FORMAT_STENCIL;

        uint32_t flags = dmGraphics::BUFFER_TYPE_COLOR0_BIT |
                         dmGraphics::BUFFER_TYPE_COLOR1_BIT |
                         dmGraphics::BUFFER_TYPE_DEPTH_BIT  |
                         dmGraphics::BUFFER_TYPE_STENCIL_BIT;

        dmGraphics::HRenderTarget target = dmGraphics::NewRenderTarget(m_Context, flags, params);

        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, target));

        dmGraphics::HTexture color0 = dmGraphics::GetRenderTargetTexture(m_Context, target, dmGraphics::BUFFER_TYPE_COLOR0_BIT);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, color0));

        dmGraphics::HTexture color1 = dmGraphics::GetRenderTargetTexture(m_Context, target, dmGraphics::BUFFER_TYPE_COLOR1_BIT);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, color1));

        dmGraphics::HTexture color2_not_exist = dmGraphics::GetRenderTargetTexture(m_Context, target, dmGraphics::BUFFER_TYPE_COLOR2_BIT);
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color2_not_exist));

        dmGraphics::DeleteRenderTarget(m_Context, target);
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, target));
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color0));
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color1));
    }
}

TEST_F(dmGraphicsTest, TestGraphicsNativeSymbols)
{
    dmGraphics::GetNativeiOSUIWindow();
    dmGraphics::GetNativeiOSUIView();
    dmGraphics::GetNativeiOSEAGLContext();
    dmGraphics::GetNativeOSXNSWindow();
    dmGraphics::GetNativeOSXNSView();
    dmGraphics::GetNativeOSXNSOpenGLContext();
    dmGraphics::GetNativeWindowsHWND();
    dmGraphics::GetNativeWindowsHGLRC();
    dmGraphics::GetNativeAndroidEGLContext();
    dmGraphics::GetNativeAndroidEGLSurface();
    dmGraphics::GetNativeAndroidJavaVM();
    dmGraphics::GetNativeAndroidActivity();
    dmGraphics::GetNativeAndroidApp();
    dmGraphics::GetNativeX11Window();
    dmGraphics::GetNativeX11GLXContext();
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
