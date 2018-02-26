#include "fontview.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <hid/hid.h>

#include "../../gamesys.h"

#include "../../resources/res_font_map.h"
#include "../../resources/res_fragment_program.h"
#include "../../resources/res_vertex_program.h"
#include "../../resources/res_material.h"

#ifdef __MACH__
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

using namespace Vectormath::Aos;

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
        while (true)
        {
            dmHID::Update(context->m_HidContext);

            dmHID::KeyboardPacket keybdata;
            dmHID::GetKeyboardPacket(context->m_HidContext, &keybdata);
            if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC))
                break;

            dmGraphics::Clear(context->m_GraphicsContext, dmGraphics::BUFFER_TYPE_COLOR_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT, 0, 0, 0, 0, 1.0, 0);
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
            params.m_WorldTransform = Matrix4::translation(Vectormath::Aos::Vector3(x, y, 0.0f));
            params.m_FaceColor = Vectormath::Aos::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            dmRender::DrawText(context->m_RenderContext, context->m_FontMap, 0, 0, params);

            y += 60;

            params.m_Text = buffer;
            params.m_WorldTransform = Matrix4::translation(Vectormath::Aos::Vector3(x, y, 0.0f));

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

            dmGraphics::WindowParams window_params;

            window_params.m_Width = 960;
            window_params.m_Height = 540;
            window_params.m_Title = "FontView";
            window_params.m_Fullscreen = false;
            window_params.m_PrintDeviceInfo = false;

            context->m_ScreenWidth = window_params.m_Width;
            context->m_ScreenHeight = window_params.m_Height;

            context->m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
            dmHID::Init(context->m_HidContext);

            context->m_GraphicsContext = dmGraphics::NewContext(dmGraphics::ContextParams());

            dmGraphics::OpenWindow(context->m_GraphicsContext, &window_params);

            dmGraphics::EnableState(context->m_GraphicsContext, dmGraphics::STATE_DEPTH_TEST);

            dmRender::RenderContextParams render_params;
            render_params.m_MaxRenderTypes = 10;
            render_params.m_MaxInstances = 100;
            render_params.m_MaxCharacters = 1024;
            context->m_RenderContext = dmRender::NewRenderContext(context->m_GraphicsContext, render_params);
            dmRender::SetViewMatrix(context->m_RenderContext, Matrix4::identity());
            dmRender::SetProjectionMatrix(context->m_RenderContext, Matrix4::identity());

            dmResource::NewFactoryParams params;
            params.m_MaxResources = 16;
            params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
            context->m_Factory = dmResource::NewFactory(&params, data_dir);

            dmResource::Result factory_result;
    #define REGISTER_RESOURCE_TYPE(extension, precreate_func, create_func, post_create_func, destroy_func, recreate_func, duplicate_func, getdesc_func)\
            factory_result = dmResource::RegisterType(context->m_Factory, extension, 0, precreate_func, create_func, post_create_func, destroy_func, recreate_func, duplicate_func, getdesc_func);\
            if( factory_result != dmResource::RESULT_OK )\
            {\
                dmLogFatal("Unable to register resource type: %s", extension);\
                return false;\
            }\

            REGISTER_RESOURCE_TYPE("fontc", 0, dmGameSystem::ResFontMapCreate, 0, dmGameSystem::ResFontMapDestroy, dmGameSystem::ResFontMapRecreate, 0, 0);
            REGISTER_RESOURCE_TYPE("vpc", 0, dmGameSystem::ResVertexProgramCreate, 0, dmGameSystem::ResVertexProgramDestroy, dmGameSystem::ResVertexProgramRecreate, 0, 0);
            REGISTER_RESOURCE_TYPE("fpc", 0, dmGameSystem::ResFragmentProgramCreate, 0, dmGameSystem::ResFragmentProgramDestroy, dmGameSystem::ResFragmentProgramRecreate, 0, 0);
            REGISTER_RESOURCE_TYPE("materialc", 0, dmGameSystem::ResMaterialCreate, 0, dmGameSystem::ResMaterialDestroy, 0, 0, 0);

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
        dmHID::Final(context->m_HidContext);
        dmHID::DeleteContext(context->m_HidContext);

        if (context->m_Factory)
        {
            if (context->m_FontMap) dmResource::Release(context->m_Factory, context->m_FontMap);
            dmResource::DeleteFactory(context->m_Factory);
        }
        if (context->m_RenderContext)
            dmRender::DeleteRenderContext(context->m_RenderContext, 0);
        dmGraphics::DeleteContext(context->m_GraphicsContext);
    }
}
