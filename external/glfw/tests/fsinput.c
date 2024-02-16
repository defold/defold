//========================================================================
// Fullscreen mode input test
// Copyright (c) Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
//
// This test came about as the result of bug #2121835
//
//========================================================================

#include <GL/glfw.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static GLboolean running;
static int window_width = 640, window_height = 480;

static void GLFWCALL window_size_callback(int width, int height)
{
    window_width = width;
    window_height = height;

    glViewport(0, 0, window_width, window_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0.f, window_width, 0.f, window_height);
}

static void GLFWCALL key_callback(int key, int action)
{
    if (key == GLFW_KEY_ESC)
        running = GL_FALSE;
}

static void GLFWCALL char_callback(int character, int action)
{
}

static void GLFWCALL mouse_button_callback(int button, int action)
{
}

static void GLFWCALL mouse_position_callback(int x, int y)
{
}

static void GLFWCALL mouse_wheel_callback(int position)
{
}

int main(void)
{
    GLFWvidmode mode;

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(1);
    }

    glfwGetDesktopMode(&mode);

    if (!glfwOpenWindow(mode.Width, mode.Height, 0, 0, 0, 0, 0, 0, GLFW_FULLSCREEN))
    {
        glfwTerminate();

        fprintf(stderr, "Failed to open GLFW window\n");
        exit(1);
    }

    glfwSetWindowTitle("Fullscreen Input Detector");
    glfwSetKeyCallback(key_callback);
    glfwSetCharCallback(char_callback);
    glfwSetMousePosCallback(mouse_position_callback);
    glfwSetMouseButtonCallback(mouse_button_callback);
    glfwSetMouseWheelCallback(mouse_wheel_callback);
    glfwSetWindowSizeCallback(window_size_callback);
    glfwSwapInterval(1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glfwSetTime(0.0);

    running = GL_TRUE;

    while (running)
    {
        glClearColor((GLclampf) fabs(cos(glfwGetTime() * 4.f)), 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers();

        if (!glfwGetWindowParam(GLFW_OPENED))
            running = GL_FALSE;

        if (glfwGetTime() > 10.0)
            running = GL_FALSE;
    }

    glfwTerminate();
    exit(0);
}

