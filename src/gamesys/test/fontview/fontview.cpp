#include "fontview.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <hid/hid.h>

#include "../../gamesys.h"

#include "../../resources/res_font.h"
#include "../../resources/res_image_font.h"
#include "../../resources/res_fragment_program.h"
#include "../../resources/res_vertex_program.h"
#include "../../resources/res_material.h"

#ifdef __MACH__
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

int main(int argc, char *argv[])
{
    int32_t exit_code = 1;
    Context context;

    if (Init(&context, argc, argv))
    {
#ifdef __MACH__
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif

        exit_code = Run(&context);
    }

    Finalize(&context);

    return exit_code;
}

int32_t Run(Context* context)
{
    while (true)
    {
        dmHID::Update();

        dmHID::KeyboardPacket keybdata;
        dmHID::GetKeyboardPacket(&keybdata);
        if (dmHID::GetKey(&keybdata, dmHID::KEY_ESC))
            break;

        dmGraphics::HContext gfx_context = dmGraphics::GetContext();
        dmGraphics::Clear(gfx_context, dmGraphics::CLEAR_COLOUR_BUFFER | dmGraphics::CLEAR_DEPTH_BUFFER, 0, 0, 0, 0, 1.0, 0);
        dmGraphics::SetViewport(gfx_context, context->m_ScreenWidth, context->m_ScreenHeight);

        uint16_t x = 10;
        uint16_t y = 40;

        const uint32_t min = 32;
        const uint32_t max = 126;
        char buffer[max - min];
        for (uint32_t i = min; i < max; ++i)
            buffer[i - min] = (char)i;

        dmRender::FontRendererDrawString(context->m_FontRenderer, context->m_TestString, x, y, 1.0f, 1.0f, 1.0f, 1.0f);

        y += 60;

        dmRender::FontRendererDrawString(context->m_FontRenderer, buffer, x, y, 1.0f, 1.0f, 1.0f, 1.0f);

        dmRender::FontRendererFlush(context->m_FontRenderer);

        dmGraphics::SetBlendFunc(gfx_context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(gfx_context, dmGraphics::BLEND);
        dmGraphics::DisableState(gfx_context, dmGraphics::DEPTH_TEST);
        dmRender::Draw(context->m_RenderContext, 0x0);
        dmRender::ClearRenderObjects(context->m_RenderContext);
        dmRender::FontRendererClear(context->m_FontRenderer);

        dmGraphics::Flip();
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

        dmGraphics::CreateDeviceParams graphics_params;

        graphics_params.m_DisplayWidth = 960;
        graphics_params.m_DisplayHeight = 540;
        graphics_params.m_AppTitle = "FontView";
        graphics_params.m_Fullscreen = false;
        graphics_params.m_PrintDeviceInfo = false;

        context->m_ScreenWidth = graphics_params.m_DisplayWidth;
        context->m_ScreenHeight = graphics_params.m_DisplayHeight;

        dmHID::Initialize();

        context->m_GraphicsDevice = dmGraphics::NewDevice(&argc, argv, &graphics_params);

        dmGraphics::HContext gfx_context = dmGraphics::GetContext();

        dmGraphics::EnableState(gfx_context, dmGraphics::DEPTH_TEST);

        dmRender::RenderContextParams render_params;
        render_params.m_MaxRenderTypes = 10;
        render_params.m_MaxInstances = 100;
        render_params.m_DisplayWidth = 960;
        render_params.m_DisplayHeight = 540;
        context->m_RenderContext = dmRender::NewRenderContext(render_params);
        dmRender::SetViewMatrix(context->m_RenderContext, Matrix4::identity());
        dmRender::SetProjectionMatrix(context->m_RenderContext, Matrix4::identity());

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        context->m_Factory = dmResource::NewFactory(&params, data_dir);

        dmResource::FactoryResult factory_result;
#define REGISTER_RESOURCE_TYPE(extension, create_func, destroy_func, recreate_func)\
        factory_result = dmResource::RegisterType(context->m_Factory, extension, 0, create_func, destroy_func, recreate_func);\
        if( factory_result != dmResource::FACTORY_RESULT_OK )\
        {\
            dmLogFatal("Unable to register resource type: %s", extension);\
            return false;\
        }\

        REGISTER_RESOURCE_TYPE("fontc", dmGameSystem::ResFontCreate, dmGameSystem::ResFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("arbvp", dmGameSystem::ResVertexProgramCreate, dmGameSystem::ResVertexProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("arbfp", dmGameSystem::ResFragmentProgramCreate, dmGameSystem::ResFragmentProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("imagefontc", dmGameSystem::ResImageFontCreate, dmGameSystem::ResImageFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("materialc", dmGameSystem::ResMaterialCreate, dmGameSystem::ResMaterialDestroy, 0);

#undef REGISTER_RESOURCE_TYPE

        dmResource::Get(context->m_Factory, font_path, (void**) &context->m_Font);
        if (!context->m_Font)
        {
            dmLogFatal("Font '%s' could not be loaded.", font_path);
            return false;
        }

        context->m_FontRenderer = dmRender::NewFontRenderer(context->m_RenderContext, context->m_Font, context->m_ScreenWidth, context->m_ScreenHeight, 1024);

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
    dmGraphics::DeleteDevice(context->m_GraphicsDevice);
    dmHID::Finalize();

    if (context->m_Factory)
    {
        if (context->m_FontRenderer) dmRender::DeleteFontRenderer(context->m_FontRenderer);
        if (context->m_Font) dmResource::Release(context->m_Factory, context->m_Font);
        dmResource::DeleteFactory(context->m_Factory);
    }
    if (context->m_RenderContext)
        dmRender::DeleteRenderContext(context->m_RenderContext);
}
