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

#include <dlib/log.h>
#include "../platform_window.h"

#define APP_TITLE "PlatformTest"
#define WIDTH 8u
#define HEIGHT 4u

class dmPlatformTest : public jc_test_base_class
{
protected:
    struct ResizeData
    {
        uint32_t m_Width;
        uint32_t m_Height;
    };

    dmPlatform::HWindow m_Window;
    ResizeData          m_ResizeData;

    static void OnWindowResize(void* user_data, uint32_t width, uint32_t height)
    {
        ResizeData* data = (ResizeData*)user_data;
        data->m_Width = width;
        data->m_Height = height;
    }

    void SetUp() override
    {
        m_Window = dmPlatform::NewWindow();

        m_ResizeData.m_Width = 0;
        m_ResizeData.m_Height = 0;

        dmPlatform::WindowParams params = {};
        params.m_ResizeCallback = OnWindowResize;
        params.m_ResizeCallbackUserData = &m_ResizeData;
        params.m_Title = APP_TITLE;
        params.m_Width = WIDTH;
        params.m_Height = HEIGHT;
        params.m_Fullscreen = false;
        params.m_PrintDeviceInfo = false;
        params.m_ContextAlphabits = 8;

        dmPlatform::OpenWindow(m_Window, params);
    }

    void TearDown() override
    {
        dmPlatform::DeleteWindow(m_Window);
    }
};

TEST_F(dmPlatformTest, DoubleOpenWindow)
{
    dmPlatform::WindowParams params;
    params.m_Title = APP_TITLE;
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Fullscreen = false;
    params.m_PrintDeviceInfo = false;
    params.m_ContextAlphabits = 8;

    ASSERT_EQ(dmPlatform::PLATFORM_RESULT_WINDOW_ALREADY_OPENED, dmPlatform::OpenWindow(m_Window, params));
}

TEST_F(dmPlatformTest, CloseWindow)
{
    dmPlatform::CloseWindow(m_Window);
    dmPlatform::CloseWindow(m_Window);
}

TEST_F(dmPlatformTest, CloseOpenWindow)
{
    dmPlatform::CloseWindow(m_Window);
    dmPlatform::WindowParams params;
    params.m_Title = APP_TITLE;
    params.m_Width = WIDTH;
    params.m_Height = HEIGHT;
    params.m_Fullscreen = false;
    params.m_PrintDeviceInfo = true;
    params.m_ContextAlphabits = 8;
    dmLogSetLevel(LOG_SEVERITY_INFO);

    ASSERT_EQ(dmPlatform::PLATFORM_RESULT_OK, dmPlatform::OpenWindow(m_Window, params));
    dmLogSetLevel(LOG_SEVERITY_WARNING);
}

TEST_F(dmPlatformTest, TestWindowState)
{
    ASSERT_TRUE(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED) ? true : false);
    dmPlatform::CloseWindow(m_Window);
    ASSERT_FALSE(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));
}

TEST_F(dmPlatformTest, TestWindowSize)
{
    uint32_t width = WIDTH * 2;
    uint32_t height = HEIGHT * 2;
    dmPlatform::SetWindowSize(m_Window, width, height);
    ASSERT_EQ(width, dmPlatform::GetWindowWidth(m_Window));
    ASSERT_EQ(height, dmPlatform::GetWindowHeight(m_Window));
    ASSERT_EQ(width, m_ResizeData.m_Width);
    ASSERT_EQ(height, m_ResizeData.m_Height);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
