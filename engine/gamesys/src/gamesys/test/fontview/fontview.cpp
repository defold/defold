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

#include "fontview.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>

#include <hid/hid.h>

#include "../../gamesys.h"

#include "../../resources/res_font.h"
#include "../../resources/res_shader_program.h"
#include "../../resources/res_material.h"

#include <dmsdk/resource/resource.h>

#ifdef __MACH__
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

using namespace dmVMath;

int main(int argc, char *argv[])
{
    int32_t exit_code = 1;
    dmFontView::Context context;

    if (dmFontView::Init(&context, argc, argv))
    {
#ifdef __MACH__
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif

        exit_code = dmFontView::Run(&context);
    }

    dmFontView::Finalize(&context);

    return exit_code;
}

namespace dmFontView
{
    int32_t Run(Context* context)
    {
        dmHID::HKeyboard keyboard = dmHID::GetKeyboard(context->m_HidContext, 0);

        while (true)
        {
            dmHID::Update(context->m_HidContext);

            dmHID::KeyboardPacket keybdata;
            dmHID::GetKeyboardPacket(keyboard, &keybdata);
            if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC))
                break;

            dmGraphics::Clear(context->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR0_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, 1.0, 0);
            dmGraphics::SetViewport(context->m_GraphicsContext, 0, 0, context->m_ScreenWidth, context->m_ScreenHeight);

            uint16_t x = 10;
            uint16_t y = 40;

            const uint32_t min = 32;
            const uint32_t max = 126;
            char buffer[max - min];
            for (uint32_t i = min; i < max; ++i)
                buffer[i - min] = (char)i;

            dmRender::DrawTextParams params;
            params.m_Text = context->m_TestString;
            params.m_WorldTransform = Matrix4::translation(dmVMath::Vector3(x, y, 0.0f));
            params.m_FaceColor = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

            y += 60;

            params.m_Text = buffer;
            params.m_WorldTransform = Matrix4::translation(dmVMath::Vector3(x, y, 0.0f));

            dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

            dmGraphics::SetBlendFunc(context->m_GraphicsContext, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            dmGraphics::EnableState(context->m_GraphicsContext, dmGraphics::STATE_BLEND);
            dmGraphics::DisableState(context->m_GraphicsContext, dmGraphics::STATE_DEPTH_TEST);
            dmRender::Draw(context->m_RenderContext, 0x0, 0x0);
            dmRender::ClearRenderObjects(context->m_RenderContext);

            dmGraphics::Flip(context->m_GraphicsContext);
        }

        return 0;
    }

    bool Init(Context* context, int argc, char* argv[])
    {
        memset(context, 0, sizeof(Context));

        if (argc > 2)
        {
            const char* data_dir = argv[1];
            const char* font_path = argv[2];
            if (argc > 3)
                context->m_TestString = argv[3];
            else
                context->m_TestString = "The quick brown fox jumps over the lazy dog";

            context->m_Window = dmPlatform::NewWindow();

            dmPlatform::WindowParams window_params = {};
            window_params.m_Width            = 960;
            window_params.m_Height           = 540;
            window_params.m_Title            = "FontView";
            window_params.m_ContextAlphabits = 8;
            window_params.m_GraphicsApi      = dmPlatform::PLATFORM_GRAPHICS_API_VULKAN;

            dmPlatform::OpenWindow(context->m_Window, window_params);

            context->m_ScreenWidth = window_params.m_Width;
            context->m_ScreenHeight = window_params.m_Height;

            context->m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
            dmHID::Init(context->m_HidContext);

            dmGraphics::InstallAdapter();

            dmGraphics::ContextParams graphics_context_params;
            graphics_context_params.m_Window = context->m_Window;

            context->m_GraphicsContext = dmGraphics::NewContext(graphics_context_params);

            dmGraphics::EnableState(context->m_GraphicsContext, dmGraphics::STATE_DEPTH_TEST);

            dmRender::RenderContextParams render_params;
            render_params.m_MaxRenderTypes = 10;
            render_params.m_MaxInstances = 100;
            render_params.m_MaxCharacters = 1024;
            render_params.m_MaxBatches = 128;
            context->m_RenderContext = dmRender::NewRenderContext(context->m_GraphicsContext, render_params);
            dmRender::SetViewMatrix(context->m_RenderContext, Matrix4::identity());
            dmRender::SetProjectionMatrix(context->m_RenderContext, Matrix4::identity());

            dmResource::NewFactoryParams params;
            params.m_MaxResources = 16;
            params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
            context->m_Factory = dmResource::NewFactory(&params, data_dir);

            dmResource::Result factory_result;
    #define REGISTER_RESOURCE_TYPE(extension, precreate_func, create_func, post_create_func, destroy_func, recreate_func)\
            factory_result = dmResource::RegisterType(context->m_Factory, extension, 0, precreate_func, create_func, post_create_func, destroy_func, recreate_func);\
            if( factory_result != dmResource::RESULT_OK )\
            {\
                dmLogFatal("Unable to register resource type: %s", extension);\
                return false;\
            }\

            //REGISTER_RESOURCE_TYPE("fontc", 0, dmGameSystem::ResFontCreate, 0, dmGameSystem::ResFontDestroy, dmGameSystem::ResFontRecreate);
            // Link with "fontc" resource type
            REGISTER_RESOURCE_TYPE("spc", dmGameSystem::ResShaderProgramPreload, dmGameSystem::ResShaderProgramCreate, 0, dmGameSystem::ResShaderProgramDestroy, dmGameSystem::ResShaderProgramRecreate);
            REGISTER_RESOURCE_TYPE("materialc", 0, dmGameSystem::ResMaterialCreate, 0, dmGameSystem::ResMaterialDestroy, 0);

    #undef REGISTER_RESOURCE_TYPE

            dmResource::Get(context->m_Factory, font_path, (void**) &context->m_FontMap);
            if (!context->m_FontMap)
            {
                dmLogFatal("Font '%s' could not be loaded.", font_path);
                return false;
            }

            return true;
        }
        else
        {
            dmLogFatal("Usage: %s <data-dir> <font.fontc> [text]", argv[0]);
        }
        return false;
    }

    void Finalize(Context* context)
    {
        if (context->m_HidContext)
        {
            dmHID::Final(context->m_HidContext);
            dmHID::DeleteContext(context->m_HidContext);
        }

        if (context->m_Factory)
        {
            if (context->m_FontMap) dmResource::Release(context->m_Factory, context->m_FontMap);
            dmResource::DeleteFactory(context->m_Factory);
        }
        if (context->m_RenderContext)
            dmRender::DeleteRenderContext(context->m_RenderContext, 0);
        if (context->m_GraphicsContext)
            dmGraphics::DeleteContext(context->m_GraphicsContext);

        if (context->m_Window)
        {
            dmPlatform::CloseWindow(context->m_Window);
            dmPlatform::DeleteWindow(context->m_Window);
        }
    }
}
