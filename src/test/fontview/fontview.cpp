#include "fontview.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <hid/hid.h>

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

        dmRender::FontRendererDrawString(context->m_FontRenderer, buffer, x, y, 1.0f, 1.0f, 1.0f, 1.0f);

        y += 60;

        dmRender::FontRendererDrawString(context->m_FontRenderer, context->m_TestString, x, y, 1.0f, 1.0f, 1.0f, 1.0f);

        dmRender::FontRendererFlush(context->m_FontRenderer);

        dmRender::SetViewProjectionMatrix(context->m_RenderPass, &context->m_RenderContext.m_ViewProj);
        dmRender::Update(context->m_RenderWorld, 0.0f);
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

        dmDDF::Result ddf_e = dmDDF::LoadMessageFromFile(font_path, dmRenderDDF::FontDesc::m_DDFDescriptor, (void**)&context->m_FontDesc);
        if ( ddf_e != dmDDF::RESULT_OK )
        {
            dmLogFatal("Font description '%s' could not be loaded.", font_path);
            return false;
        }

        dmGraphics::HDevice device;
        dmGraphics::CreateDeviceParams graphics_params;

        graphics_params.m_DisplayWidth = 960;
        graphics_params.m_DisplayHeight = 540;
        graphics_params.m_AppTitle = "FontView";
        graphics_params.m_Fullscreen = false;
        graphics_params.m_PrintDeviceInfo = false;

        context->m_ScreenWidth = graphics_params.m_DisplayWidth;
        context->m_ScreenHeight = graphics_params.m_DisplayHeight;

        dmHID::Initialize();

        device = dmGraphics::CreateDevice(&argc, argv, &graphics_params);

        dmGraphics::HContext gfx_context = dmGraphics::GetContext();

        dmGraphics::EnableState(gfx_context, dmGraphics::DEPTH_TEST);

        context->m_RenderWorld = dmRender::NewRenderWorld(100, 100, 0x0);
        context->m_RenderContext.m_View = Matrix4::identity();
        context->m_RenderContext.m_Projection = Matrix4::identity();
        context->m_RenderContext.m_ViewProj = Matrix4::identity();
        context->m_RenderContext.m_CameraPosition = Point3(0.0f, 0.0f, 0.0f);
        context->m_RenderContext.m_GFXContext = gfx_context;

        dmRender::RenderPassDesc rp_model_desc("model", 0x0, 1, 100, 1, 0x0, 0x0);
        context->m_RenderPass = dmRender::NewRenderPass(&rp_model_desc);
        dmRender::AddRenderPass(context->m_RenderWorld, context->m_RenderPass);

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

        REGISTER_RESOURCE_TYPE("arbvp", ResVertexProgramCreate, ResVertexProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("arbfp", ResFragmentProgramCreate, ResFragmentProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("imagefontc", ResImageFontCreate, ResImageFontDestroy, 0);

#undef REGISTER_RESOURCE_TYPE

        char vertex_program_buf[128];
        DM_SNPRINTF(vertex_program_buf, sizeof(vertex_program_buf), "%s.arbvp", context->m_FontDesc->m_VertexProgram);
        char fragment_program_buf[128];
        DM_SNPRINTF(fragment_program_buf, sizeof(fragment_program_buf), "%s.arbfp", context->m_FontDesc->m_FragmentProgram);

        dmResource::Get(context->m_Factory, context->m_FontDesc->m_Font, (void**) &context->m_ImageFont);
        if (!context->m_ImageFont)
        {
            dmLogFatal("Image font '%s' could not be loaded.", context->m_FontDesc->m_Font);
            return false;
        }

        dmResource::Get(context->m_Factory, vertex_program_buf, (void**) &context->m_VertexProgram);
        if (!context->m_VertexProgram)
        {
            dmLogFatal("Vertex program '%s' could not be loaded.", vertex_program_buf);
            return false;
        }

        dmResource::Get(context->m_Factory, fragment_program_buf, (void**) &context->m_FragmentProgram);
        if (!context->m_FragmentProgram)
        {
            dmLogFatal("Fragment program '%s' could not be loaded.", fragment_program_buf);
            return false;
        }

        context->m_Font = dmRender::NewFont(context->m_ImageFont);
        if (!context->m_Font)
        {
            dmLogFatal("Font could not be created.", fragment_program_buf);
            return false;
        }

        dmRender::SetVertexProgram(context->m_Font, context->m_VertexProgram);
        dmRender::SetFragmentProgram(context->m_Font, context->m_FragmentProgram);

        context->m_FontRenderer = dmRender::NewFontRenderer(context->m_Font, context->m_RenderWorld, context->m_ScreenWidth,
            context->m_ScreenHeight, 1024);

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
    dmGraphics::DestroyDevice();
    dmHID::Finalize();

    if (context->m_Factory)
    {
        if (context->m_FontRenderer) dmRender::DeleteFontRenderer(context->m_FontRenderer);
        if (context->m_Font) dmRender::DeleteFont(context->m_Font);
        if (context->m_ImageFont) dmResource::Release(context->m_Factory, context->m_ImageFont);
        if (context->m_VertexProgram) dmResource::Release(context->m_Factory, (void*) context->m_VertexProgram);
        if (context->m_FragmentProgram) dmResource::Release(context->m_Factory, (void*) context->m_FragmentProgram);
        dmResource::DeleteFactory(context->m_Factory);
    }
    if (context->m_RenderPass)
        dmRender::DeleteRenderPass(context->m_RenderPass);
    if (context->m_RenderWorld)
        dmRender::DeleteRenderWorld(context->m_RenderWorld);
    if (context->m_FontDesc)
        dmDDF::FreeMessage(context->m_FontDesc);
}

dmResource::CreateResult ResImageFontCreate(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
{
    dmRender::HImageFont font = dmRender::NewImageFont(buffer, buffer_size);
    if (font)
    {
        resource->m_Resource = (void*)font;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

dmResource::CreateResult ResImageFontDestroy(dmResource::HFactory factory,
                                       void* context,
                                       dmResource::SResourceDescriptor* resource)
{
    dmRender::DeleteImageFont((dmRender::HImageFont) resource->m_Resource);

    return dmResource::CREATE_RESULT_OK;
}

dmResource::CreateResult ResVertexProgramCreate(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
{
    dmGraphics::HVertexProgram prog = dmGraphics::CreateVertexProgram(buffer, buffer_size);
    if (prog == 0 )
        return dmResource::CREATE_RESULT_UNKNOWN;

    resource->m_Resource = (void*) prog;
    return dmResource::CREATE_RESULT_OK;
}

dmResource::CreateResult ResVertexProgramDestroy(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
{
    dmGraphics::DestroyVertexProgram((dmGraphics::HVertexProgram) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}

dmResource::CreateResult ResFragmentProgramCreate(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename)
{
    dmGraphics::HFragmentProgram prog = dmGraphics::CreateFragmentProgram(buffer, buffer_size);
    if (prog == 0 )
        return dmResource::CREATE_RESULT_UNKNOWN;

    resource->m_Resource = (void*) prog;
    return dmResource::CREATE_RESULT_OK;
}

dmResource::CreateResult ResFragmentProgramDestroy(dmResource::HFactory factory,
                                                void* context,
                                                dmResource::SResourceDescriptor* resource)
{
    dmGraphics::DestroyFragmentProgram((dmGraphics::HFragmentProgram) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}
