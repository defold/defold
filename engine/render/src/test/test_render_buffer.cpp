// Copyright 2020-2025 The Defold Foundation
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

#include <testmain/testmain.h>

#include "render/render.h"
#include "render/render_private.h"

class dmRenderBufferTest : public jc_test_base_class
{
public:
    dmPlatform::HWindow           m_Window;
    dmGraphics::HContext          m_GraphicsContext;
    dmRender::HRenderContext      m_RenderContext;
    dmRender::RenderContextParams m_Params;
    bool                          m_MultiBufferingRequired;

    void SetUp() override
    {
        dmGraphics::InstallAdapter();

        dmPlatform::WindowParams win_params = {};
        win_params.m_Width = 20;
        win_params.m_Height = 10;
        win_params.m_ContextAlphabits = 8;

        m_Window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(m_Window, win_params);

        dmGraphics::ContextParams graphics_context_params;
        graphics_context_params.m_Window = m_Window;

        m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

        dmScript::ContextParams script_context_params = {};
        script_context_params.m_GraphicsContext = m_GraphicsContext;
        m_Params.m_ScriptContext = dmScript::NewContext(script_context_params);
        m_Params.m_MaxCharacters = 256;
        m_Params.m_MaxBatches    = 128;
        m_RenderContext          = dmRender::NewRenderContext(m_GraphicsContext, m_Params);

        m_MultiBufferingRequired = m_RenderContext->m_MultiBufferingRequired;
    }
    void TearDown() override
    {
        dmRender::DeleteRenderContext(m_RenderContext, 0);
        dmGraphics::CloseWindow(m_GraphicsContext);
        dmGraphics::DeleteContext(m_GraphicsContext);
        dmPlatform::CloseWindow(m_Window);
        dmPlatform::DeleteWindow(m_Window);
        dmScript::DeleteContext(m_Params.m_ScriptContext);
    }
};

TEST_F(dmRenderBufferTest, TestBufferedRenderBufferSimple)
{
    dmRender::HBufferedRenderBuffer buffer = dmRender::NewBufferedRenderBuffer(m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
    ASSERT_NE((void*) 0x0, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());

    m_RenderContext->m_MultiBufferingRequired = 0;

    // If we don't need multiple buffers, we should still only have one buffer here
    dmRender::HRenderBuffer render_buffer = dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_NE(0, render_buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());

    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());

    m_RenderContext->m_MultiBufferingRequired = 1;

    dmRender::HRenderBuffer render_buffer_2 = dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_NE(0, render_buffer_2);
    ASSERT_EQ(2, buffer->m_Buffers.Size());

    dmRender::DeleteBufferedRenderBuffer(m_RenderContext, buffer);

    m_RenderContext->m_MultiBufferingRequired = m_MultiBufferingRequired;
}

TEST_F(dmRenderBufferTest, TestBufferedRenderBufferSetData)
{
    // We start off with 1 buffer, so size == 1
    dmRender::HBufferedRenderBuffer buffer = dmRender::NewBufferedRenderBuffer(m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
    ASSERT_NE((void*) 0x0, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());

    // Test creating multiple buffers and then setting the render buffer data
    m_RenderContext->m_MultiBufferingRequired = 1;

    dmRender::HRenderBuffer render_buffer_1 = dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(2, buffer->m_Buffers.Size());

    uint8_t buffer_1[] = { 255, 255, 0, 255 };
    dmRender::SetBufferData(m_RenderContext, buffer, sizeof(buffer_1), buffer_1, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    dmRender::HRenderBuffer render_buffer_2 = dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(3, buffer->m_Buffers.Size());

    uint8_t buffer_2[] = { 0, 127, 255, 127 };
    dmRender::SetBufferData(m_RenderContext, buffer, sizeof(buffer_2), buffer_2, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

    void* buffer_1_ptr = dmGraphics::MapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) render_buffer_1, dmGraphics::BUFFER_ACCESS_READ_ONLY);
    ASSERT_EQ(0, memcmp(buffer_1, buffer_1_ptr, sizeof(buffer_1)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) render_buffer_1));

    void* buffer_2_ptr = dmGraphics::MapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) render_buffer_2, dmGraphics::BUFFER_ACCESS_READ_ONLY);
    ASSERT_EQ(0, memcmp(buffer_2, buffer_2_ptr, sizeof(buffer_2)));
    ASSERT_TRUE(dmGraphics::UnmapVertexBuffer(m_GraphicsContext, (dmGraphics::HVertexBuffer) render_buffer_2));

    dmRender::DeleteBufferedRenderBuffer(m_RenderContext, buffer);

    m_RenderContext->m_MultiBufferingRequired = m_MultiBufferingRequired;
}

TEST_F(dmRenderBufferTest, TestBufferedRenderBufferAddAndTrim)
{
    dmRender::HBufferedRenderBuffer buffer = dmRender::NewBufferedRenderBuffer(m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
    ASSERT_NE((void*) 0x0, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());
    ASSERT_EQ(0, buffer->m_BufferIndex);

    // Test that non-multi buffering doesn't allocate more than one buffers
    m_RenderContext->m_MultiBufferingRequired = 0;

    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());
    ASSERT_EQ(0, buffer->m_BufferIndex);

    dmRender::RewindBuffer(m_RenderContext, buffer);
    ASSERT_EQ(0, buffer->m_BufferIndex);

    dmRender::TrimBuffer(m_RenderContext, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());

    // Test that multi buffering can allocate multiple buffers and then trim
    m_RenderContext->m_MultiBufferingRequired = 1;

    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(4, buffer->m_Buffers.Size());

    dmRender::TrimBuffer(m_RenderContext, buffer);
    ASSERT_EQ(4, buffer->m_Buffers.Size());
    ASSERT_EQ(3, buffer->m_BufferIndex);

    dmRender::RewindBuffer(m_RenderContext, buffer);
    ASSERT_EQ(0, buffer->m_BufferIndex);
    ASSERT_EQ(4, buffer->m_Buffers.Size());

    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(1, buffer->m_BufferIndex);

    dmRender::AddRenderBuffer(m_RenderContext, buffer);
    ASSERT_EQ(2, buffer->m_BufferIndex);

    dmRender::TrimBuffer(m_RenderContext, buffer);
    ASSERT_EQ(3, buffer->m_Buffers.Size());
    ASSERT_EQ(2, buffer->m_BufferIndex);

    dmRender::RewindBuffer(m_RenderContext, buffer);
    dmRender::TrimBuffer(m_RenderContext, buffer);
    ASSERT_EQ(1, buffer->m_Buffers.Size());
    ASSERT_EQ(0, buffer->m_BufferIndex);

    dmRender::DeleteBufferedRenderBuffer(m_RenderContext, buffer);

    m_RenderContext->m_MultiBufferingRequired = m_MultiBufferingRequired;
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    dmHashEnableReverseHash(true);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
