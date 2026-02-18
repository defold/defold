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

#include <dmsdk/graphics/graphics.h>
#include <dmsdk/platform/window.h>

#define TEST_CHECK(_EXPR) do { if (!(_EXPR)) { fprintf(stderr, "TEST_CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #_EXPR); return __LINE__; } } while (0)

int dmGraphicsCTestContextParamsInitialize(void)
{
    GraphicsCreateParams params;
    GraphicsContextParamsInitialize(&params);

    TEST_CHECK(params.m_DefaultTextureMinFilter == TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST);
    TEST_CHECK(params.m_DefaultTextureMagFilter == TEXTURE_FILTER_LINEAR);
    TEST_CHECK(params.m_SwapInterval == 1u);
    TEST_CHECK(params.m_Window == 0x0);
    TEST_CHECK(params.m_Width == 0u);
    TEST_CHECK(params.m_Height == 0u);

    /* Verify null input is accepted */
    GraphicsContextParamsInitialize(0x0);
    return 0;
}

int dmGraphicsCTestLifecycle(void)
{
    TEST_CHECK(GraphicsInstallAdapter(ADAPTER_FAMILY_NONE));
    TEST_CHECK(GraphicsNewContext(0x0) == 0x0);

    HWindow window = WindowNew();
    TEST_CHECK(window != 0x0);

    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Title = "test_graphics_c";
    window_params.m_Width = 32;
    window_params.m_Height = 32;
    window_params.m_GraphicsApi = WINDOW_GRAPHICS_API_NULL;

    WindowResult window_result = WindowOpen(window, &window_params);
    TEST_CHECK(window_result == WINDOW_RESULT_OK);

    GraphicsCreateParams params;
    GraphicsContextParamsInitialize(&params);
    params.m_Window = window;
    params.m_Width = 32;
    params.m_Height = 32;

    HGraphicsContext context = GraphicsNewContext(&params);
    TEST_CHECK(context != 0x0);

    GraphicsBeginFrame(context);
    GraphicsFlip(context);
    GraphicsCloseWindow(context);
    GraphicsDeleteContext(context);
    GraphicsFinalize();

    WindowDelete(window);
    return 0;
}
