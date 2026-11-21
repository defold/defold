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

#include <jc_test/jc_test.h>
#include "../platform_window_win32.h"
#include <GL/gl.h>

struct WorkerUpload
{
    HWindow m_Window;
    GLuint  m_Texture;
};

static DWORD WINAPI Upload(void* data)
{
    WorkerUpload* upload = (WorkerUpload*)data;
    void* context = dmPlatform::AcquireAuxContext(upload->m_Window);
    if (!context)
    {
        return 1;
    }

    const GLubyte pixel[] = { 11, 147, 206, 255 };
    glGenTextures(1, &upload->m_Texture);
    glBindTexture(GL_TEXTURE_2D, upload->m_Texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glFinish();
    dmPlatform::UnacquireAuxContext(upload->m_Window, context);
    return wglGetCurrentContext() != 0;
}

// Verifies WGL shares real texture storage across threads and releases its context on close.
TEST(Win32OpenGL, SharedWorkerUpload)
{
    HWindow window = dmPlatform::NewWindow();
    ASSERT_NE((HWindow)0, window);
    WindowCreateParams params;
    WindowCreateParamsInitialize(&params);
    params.m_GraphicsApi = WINDOW_GRAPHICS_API_OPENGL;
    params.m_Hidden = true;
    params.m_Samples = 4;
    ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
    ASSERT_EQ(dmPlatform::GetWindowsHGLRC(window), wglGetCurrentContext());
    ASSERT_TRUE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_AUX_CONTEXT));
    ASSERT_TRUE(dmPlatform::GetProcAddress(window, "glTexImage2D") != 0);
    ASSERT_TRUE(dmPlatform::GetProcAddress(window, "glNonexistentDefoldProc") == 0);
    ASSERT_GE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_SAMPLE_COUNT), 1U);
    ASSERT_LE(dmPlatform::GetWindowStateParam(window, WINDOW_STATE_SAMPLE_COUNT), 4U);
    WorkerUpload upload = { window };
    HANDLE worker = CreateThread(0, 0, Upload, &upload, 0, 0);
    ASSERT_NE((HANDLE)0, worker);
    ASSERT_EQ((DWORD)WAIT_OBJECT_0, WaitForSingleObject(worker, INFINITE));
    DWORD result;
    ASSERT_TRUE(GetExitCodeThread(worker, &result));
    CloseHandle(worker);
    ASSERT_EQ(0UL, result);
    ASSERT_TRUE(glIsTexture(upload.m_Texture));
    GLubyte pixel[4] = {};
    glBindTexture(GL_TEXTURE_2D, upload.m_Texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    ASSERT_EQ(147U, pixel[1]);
    glDeleteTextures(1, &upload.m_Texture);

    bool supports_profiles = dmPlatform::GetProcAddress(window, "wglCreateContextAttribsARB") != 0;
    dmPlatform::CloseWindow(window);
    ASSERT_EQ((HGLRC)0, wglGetCurrentContext());
    if (supports_profiles)
    {
        params.m_GraphicsApiVersionHint = 33;
        params.m_OpenGLUseCoreProfileHint = true;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        GLint profile = 0;
        glGetIntegerv(0x9126, &profile); // GL_CONTEXT_PROFILE_MASK
        ASSERT_TRUE(profile & 1);        // GL_CONTEXT_CORE_PROFILE_BIT
    }

    dmPlatform::DeleteWindow(window);
}
