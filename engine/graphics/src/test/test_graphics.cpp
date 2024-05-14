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

#include <stdint.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/log.h>
#include <dlib/time.h>
#include <platform/platform_window.h>
#include <dmsdk/dlib/dstrings.h> // dmStrCaseCmp

#include "graphics.h"
#include "graphics_private.h"
#include "null/graphics_null_private.h"

#define APP_TITLE "GraphicsTest"
#define WIDTH 8u
#define HEIGHT 4u

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

    dmJobThread::HContext m_JobThread;
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

    virtual void SetUp()
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

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, params);

        dmJobThread::JobThreadCreationParams job_thread_create_param;
        job_thread_create_param.m_ThreadNames[0] = "test_jobs";
        job_thread_create_param.m_ThreadCount    = 1;

        if (dmGraphicsTestT::s_Asynchronous)
            m_JobThread = dmJobThread::Create(job_thread_create_param);
        else
            m_JobThread = 0;

        dmGraphics::ContextParams context_params = dmGraphics::ContextParams();
        context_params.m_Window                  = m_Window;
        context_params.m_JobThread               = m_JobThread;

        m_Context = dmGraphics::NewContext(context_params);
        m_NullContext = (dmGraphics::NullContext*) m_Context;
        m_NullContext->m_UseAsyncTextureLoad = 0;

        m_ResizeData.m_Width = 0;
        m_ResizeData.m_Height = 0;
    }

    virtual void TearDown()
    {
        if (m_JobThread)
            dmJobThread::Destroy(m_JobThread);
        dmGraphics::CloseWindow(m_Context);
        dmGraphics::DeleteContext(m_Context);
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
    ASSERT_EQ(sizeof(p) / 2, m_NullContext->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0, memcmp(p, m_NullContext->m_VertexStreams[0].m_Source, 3 * sizeof(float)));
    float uv[] = { 3.0f, 4.0f, 8.0f, 9.0f };
    ASSERT_EQ(sizeof(uv) / 2, m_NullContext->m_VertexStreams[1].m_Size);
    ASSERT_EQ(0, memcmp(uv, m_NullContext->m_VertexStreams[1].m_Source, 2 * sizeof(float)));

    dmGraphics::DisableVertexDeclaration(m_Context, vertex_declaration);

    ASSERT_EQ(0u, m_NullContext->m_VertexStreams[0].m_Size);
    ASSERT_EQ(0u, m_NullContext->m_VertexStreams[1].m_Size);

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
    dmGraphics::DrawElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6, dmGraphics::TYPE_UNSIGNED_INT, ib);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, 0);
    dmGraphics::DrawElements(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 3, 6, dmGraphics::TYPE_UNSIGNED_INT, ib);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::EnableVertexDeclaration(m_Context, vd, 0);
    dmGraphics::Draw(m_Context, dmGraphics::PRIMITIVE_TRIANGLES, 0, 6);
    dmGraphics::DisableVertexDeclaration(m_Context, vd);

    dmGraphics::DisableVertexBuffer(m_Context, vb);

    dmGraphics::DeleteIndexBuffer(ib);
    dmGraphics::DeleteVertexBuffer(vb);
    dmGraphics::DeleteVertexDeclaration(vd);
    dmGraphics::DeleteVertexStreamDeclaration(stream_declaration);
}

static inline dmGraphics::ShaderDesc::Shader MakeDDFShader(dmGraphics::ShaderDesc::Language language, const char* data, uint32_t count)
{
    dmGraphics::ShaderDesc::Shader ddf;
    memset(&ddf,0,sizeof(ddf));
    ddf.m_Source.m_Data  = (uint8_t*)data;
    ddf.m_Source.m_Count = count;
    ddf.m_Language = language;
    return ddf;
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

    dmGraphics::ShaderDesc::Shader vs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, vertex_data, (uint32_t) strlen(vertex_data));
    dmGraphics::ShaderDesc::Shader fs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, fragment_data, (uint32_t) strlen(fragment_data));

    dmGraphics::HVertexProgram vp = dmGraphics::NewVertexProgram(m_Context, &vs_shader);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_Context, &fs_shader);

    dmGraphics::HProgram program = dmGraphics::NewProgram(m_Context, vp, fp);
    ASSERT_EQ(4u, dmGraphics::GetUniformCount(program));
    ASSERT_EQ(0, dmGraphics::GetUniformLocation(program, "view_proj"));
    ASSERT_EQ(1, dmGraphics::GetUniformLocation(program, "world"));
    ASSERT_EQ(2, dmGraphics::GetUniformLocation(program, "texture_sampler"));
    ASSERT_EQ(3, dmGraphics::GetUniformLocation(program, "tint"));
    char buffer[64];
    dmGraphics::Type type;
    int32_t size;
    dmGraphics::GetUniformName(program, 0, buffer, 64, &type, &size);
    ASSERT_STREQ("view_proj", buffer);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_MAT4, type);
    dmGraphics::GetUniformName(program, 1, buffer, 64, &type, &size);
    ASSERT_STREQ("world", buffer);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_MAT4, type);
    dmGraphics::GetUniformName(program, 2, buffer, 64, &type, &size);
    ASSERT_STREQ("texture_sampler", buffer);
    ASSERT_EQ(dmGraphics::TYPE_SAMPLER_2D, type);
    dmGraphics::GetUniformName(program, 3, buffer, 64, &type, &size);
    ASSERT_STREQ("tint", buffer);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, type);

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
    dmGraphics::SetConstantV4(m_Context, &constant, 1, 0);
    Vector4 matrix[4] = {   Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                            Vector4(5.0f, 6.0f, 7.0f, 8.0f),
                            Vector4(9.0f, 10.0f, 11.0f, 12.0f),
                            Vector4(13.0f, 14.0f, 15.0f, 16.0f) };
    dmGraphics::SetConstantM4(m_Context, matrix, 1, 4);
    char* program_data = new char[1024];
    *program_data = 0;
    vs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program_data, 1024);
    dmGraphics::ReloadVertexProgram(vp, &vs_shader);
    delete [] program_data;
    program_data = new char[1024];
    *program_data = 0;
    fs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, program_data, 1024);
    dmGraphics::ReloadFragmentProgram(fp, &fs_shader);
    delete [] program_data;
    dmGraphics::DisableProgram(m_Context);
    dmGraphics::DeleteProgram(m_Context, program);
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
}

TEST_F(dmGraphicsTest, TestComputeProgram)
{
    const char* compute_data = ""
        "#version 430 core\n"
        "layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n"
        "layout(location = 0) uniform vec4 my_uniform;\n"
        "void main() {\n"
        "}\n";

    dmGraphics::ShaderDesc::Shader compute_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, compute_data, (uint32_t) strlen(compute_data));
    dmGraphics::HComputeProgram cp                = dmGraphics::NewComputeProgram(m_Context, &compute_shader);
    dmGraphics::HProgram program                  = dmGraphics::NewProgram(m_Context, cp);
    ASSERT_EQ(1, dmGraphics::GetUniformCount(program));
    ASSERT_EQ(0, dmGraphics::GetUniformLocation(program, "my_uniform"));

    char buffer[64];
    dmGraphics::Type type;
    int32_t size;
    dmGraphics::GetUniformName(program, 0, buffer, 64, &type, &size);
    ASSERT_STREQ("my_uniform", buffer);
    ASSERT_EQ(dmGraphics::TYPE_FLOAT_VEC4, type);

    dmGraphics::DeleteComputeProgram(cp);
    dmGraphics::DeleteProgram(m_Context, program);
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

    dmGraphics::ShaderDesc::Shader vs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM140, vertex_data, (uint32_t) strlen(vertex_data));
    dmGraphics::ShaderDesc::Shader fs_shader = MakeDDFShader(dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM140, fragment_data, (uint32_t) strlen(fragment_data));

    dmGraphics::HVertexProgram vp   = dmGraphics::NewVertexProgram(m_Context, &vs_shader);
    dmGraphics::HFragmentProgram fp = dmGraphics::NewFragmentProgram(m_Context, &fs_shader);
    dmGraphics::HProgram program    = dmGraphics::NewProgram(m_Context, vp, fp);

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
    dmGraphics::DeleteVertexProgram(vp);
    dmGraphics::DeleteFragmentProgram(fp);
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
    dmGraphics::SetTexture(texture, params);

    delete [] (char*)params.m_Data;
    ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(texture));
    ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(texture));
    dmGraphics::EnableTexture(m_Context, 0, 0, texture);
    dmGraphics::DisableTexture(m_Context, 0, texture);
    dmGraphics::DeleteTexture(texture);
}

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
        dmGraphics::SetTextureAsync(textures[i], params, TestTextureAsyncCallback, (void*) (values + i));
    }

    uint64_t stop_time = dmTime::GetTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetTime() < stop_time)
    {
        dmJobThread::Update(m_JobThread);
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
        ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(textures[i]));
        ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(textures[i]));
        ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(textures[i]));
        ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(textures[i]));
        dmGraphics::EnableTexture(m_Context, 0, 0, textures[i]);
        dmGraphics::DisableTexture(m_Context, 0, textures[i]);
        dmGraphics::DeleteTexture(textures[i]);
    }

    all_complete = false;
    stop_time = dmTime::GetTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetTime() < stop_time)
    {
        dmJobThread::Update(m_JobThread);
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

static bool WaitUntilSyncronizedTextures(dmGraphics::HContext graphics_context, dmJobThread::HContext job_thread, dmGraphics::HTexture* textures, uint32_t texture_count, SyncronizedWaitCondition cond)
{
    bool all_complete = false;
    uint64_t stop_time = dmTime::GetTime() + 1*1e6; // 1 second
    while(!all_complete && dmTime::GetTime() < stop_time)
    {
        dmJobThread::Update(job_thread);
        all_complete = true;
        for (int i = 0; i < texture_count; ++i)
        {
            if (cond == WAIT_CONDITION_UPLOAD && dmGraphics::GetTextureStatusFlags(textures[i]) != dmGraphics::TEXTURE_STATUS_OK)
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
            dmGraphics::SetTextureAsync(textures[i], params, 0, 0);

            // Immediately delete, so we simulate putting them on a post-delete-queue
            dmGraphics::DeleteTexture(textures[i]);
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
        dmJobThread::Update(m_JobThread);

        // Make sure all are deleted
        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobThread, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_DELETE));
    }

    // Test 2: Simulate deleting textures async. This requires valid textures (i.e not pending)
    //         And that we continously update the job thread to finish the async jobs.
    {
        textures.SetSize(0);

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            textures.Push(dmGraphics::NewTexture(m_Context, creation_params));
            dmGraphics::SetTextureAsync(textures[i], params, 0, 0);
        }

        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobThread, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_UPLOAD));

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            dmGraphics::DeleteTexture(textures[i]);
        }

        ASSERT_TRUE(WaitUntilSyncronizedTextures(m_Context, m_JobThread, textures.Begin(), TEXTURE_COUNT, WAIT_CONDITION_DELETE));

        for (int i = 0; i < TEXTURE_COUNT; ++i)
        {
            ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, textures[i]));
        }
    }

    delete [] (char*) params.m_Data;

    m_NullContext->m_UseAsyncTextureLoad = tmp_async_load;
}

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
    ASSERT_DEATH(dmGraphics::SetTexture(texture, params),"");

    delete [] (char*)params.m_Data;

    params.m_X         = 0;
    params.m_Y         = 0;
    params.m_Width     = WIDTH + 1;
    params.m_Height    = HEIGHT + 1;
    params.m_DataSize  = params.m_Width * params.m_Height;
    params.m_Data      = new char[params.m_DataSize];

    ASSERT_DEATH(dmGraphics::SetTexture(texture, params),"");

    delete [] (char*)params.m_Data;

    dmGraphics::DeleteTexture(texture);
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
    dmGraphics::SetTexture(texture, params);

    delete [] (char*)params.m_Data;
    ASSERT_EQ(WIDTH, dmGraphics::GetTextureWidth(texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetTextureHeight(texture));
    ASSERT_EQ(WIDTH, dmGraphics::GetOriginalTextureWidth(texture));
    ASSERT_EQ(HEIGHT, dmGraphics::GetOriginalTextureHeight(texture));
    dmGraphics::EnableTexture(m_Context, 0, 0, texture);
    dmGraphics::DisableTexture(m_Context, 0, texture);
    dmGraphics::DeleteTexture(texture);
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
    dmGraphics::SetRenderTargetSize(target, width, height);

    uint32_t target_width, target_height;
    GetRenderTargetSize(target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);
    GetRenderTargetSize(target, dmGraphics::BUFFER_TYPE_DEPTH_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);
    GetRenderTargetSize(target, dmGraphics::BUFFER_TYPE_STENCIL_BIT, target_width, target_height);
    ASSERT_EQ(width, target_width);
    ASSERT_EQ(height, target_height);

    dmGraphics::Clear(m_Context, flags, 1, 1, 1, 1, 1.0f, 1);
    ASSERT_EQ(0, memcmp(data, m_NullContext->m_CurrentFrameBuffer->m_ColorBuffer[0], data_size));
    delete [] data;

    dmGraphics::SetRenderTarget(m_Context, 0x0, 0);
    dmGraphics::DeleteRenderTarget(target);

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

    GetRenderTargetSize(target, dmGraphics::BUFFER_TYPE_COLOR0_BIT, target_width, target_height);
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
    dmGraphics::DeleteRenderTarget(target);
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

    dmGraphics::HTexture texture = dmGraphics::GetRenderTargetAttachment(target, dmGraphics::ATTACHMENT_DEPTH);
    ASSERT_EQ((dmGraphics::HTexture)0x0, texture);

    texture = dmGraphics::GetRenderTargetAttachment(target, dmGraphics::ATTACHMENT_STENCIL);
    ASSERT_EQ((dmGraphics::HTexture)0x0, texture);

    texture = dmGraphics::GetRenderTargetAttachment(target, dmGraphics::ATTACHMENT_COLOR);
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
    dmGraphics::DeleteRenderTarget(target);
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

    dmGraphics::HTexture depth_texture = dmGraphics::GetRenderTargetTexture(target, dmGraphics::BUFFER_TYPE_DEPTH_BIT);
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, depth_texture));
    {
        float* texture_data = 0x0;
        dmGraphics::HandleResult res = dmGraphics::GetTextureHandle(depth_texture, (void**) &texture_data);
        ASSERT_EQ(dmGraphics::HANDLE_RESULT_OK, res);
        ASSERT_NE((float*)0x0, texture_data);

        const float EPSILON = 0.000001f;
        for (int i = 0; i < WIDTH * HEIGHT; ++i)
        {
            ASSERT_NEAR(depth_value, texture_data[i], EPSILON);
        }
    }

    dmGraphics::HTexture stencil_texture = dmGraphics::GetRenderTargetTexture(target, dmGraphics::BUFFER_TYPE_STENCIL_BIT);
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
    dmGraphics::DeleteRenderTarget(target);
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
        ASSERT_NE(0, dmGraphics::GetTextureFormatBitsPerPixel((dmGraphics::TextureFormat) i));
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
        dmGraphics::SetTexture(texture, params);

        ASSERT_EQ(1,                                         dmGraphics::GetTextureDepth(texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D,               dmGraphics::GetTextureType(texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(texture));

        dmGraphics::DeleteTexture(texture);
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
        dmGraphics::SetTexture(texture, params);

        ASSERT_EQ(params.m_Depth,                            dmGraphics::GetTextureDepth(texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_CUBE_MAP,         dmGraphics::GetTextureType(texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(texture));

        dmGraphics::DeleteTexture(texture);
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
        dmGraphics::SetTexture(texture, params);

        ASSERT_EQ(params.m_Depth,                            dmGraphics::GetTextureDepth(texture));
        ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D_ARRAY,         dmGraphics::GetTextureType(texture));
        ASSERT_EQ(dmGraphics::GetMipmapCount(texture_width), dmGraphics::GetTextureMipmapCount(texture));

        dmGraphics::DeleteTexture(texture);
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
        dmGraphics::DeleteTexture(texture);

        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, texture));

        dmGraphics::HTexture texture_2 = dmGraphics::NewTexture(m_Context, creation_params);
        ASSERT_NE(texture, texture_2);
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

        dmGraphics::HTexture color0 = dmGraphics::GetRenderTargetTexture(target, dmGraphics::BUFFER_TYPE_COLOR0_BIT);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, color0));

        dmGraphics::HTexture color1 = dmGraphics::GetRenderTargetTexture(target, dmGraphics::BUFFER_TYPE_COLOR1_BIT);
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_Context, color1));

        dmGraphics::HTexture color2_not_exist = dmGraphics::GetRenderTargetTexture(target, dmGraphics::BUFFER_TYPE_COLOR2_BIT);
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color2_not_exist));

        dmGraphics::DeleteRenderTarget(target);
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, target));
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color0));
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_Context, color1));
    }
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
