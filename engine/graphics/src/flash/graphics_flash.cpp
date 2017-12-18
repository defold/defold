#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/log.h>

#include "../graphics.h"
#include "graphics_flash.h"

using namespace Vectormath::Aos;
flash::display::Stage stage;
flash::display::Stage3D s3d;


/*
 * TODO:
 * How to free vertex and index buffers?
 * How to free texures?
 */

namespace dmGraphics
{
    uint16_t TYPE_SIZE[] =
    {
        sizeof(char), // TYPE_BYTE
        sizeof(unsigned char), // TYPE_UNSIGNED_BYTE
        sizeof(short), // TYPE_SHORT
        sizeof(unsigned short), // TYPE_UNSIGNED_SHORT
        sizeof(int), // TYPE_INT
        sizeof(unsigned int), // TYPE_UNSIGNED_INT
        sizeof(float) // TYPE_FLOAT
    };

    bool g_ContextCreated = false;

    Context::Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_DEPTH;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        m_Ctx3d = s3d->context3D;

        m_IndexBuffer = m_Ctx3d->createIndexBuffer(DM_GRAPHICS_FLASH_MAX_INDICES);
        int* tmp = new int[DM_GRAPHICS_FLASH_MAX_INDICES];
        for (uint32_t i = 0; i < DM_GRAPHICS_FLASH_MAX_INDICES; ++i) {
            tmp[i] = i;
        }
        m_IndexBuffer->uploadFromByteArray(internal::get_ram(), (int)&tmp[0], 0, DM_GRAPHICS_FLASH_MAX_INDICES, (void*)&tmp[0]);

        m_SourceFactor = BLEND_FACTOR_ONE;
        m_DestinationFactor = BLEND_FACTOR_ZERO;
        m_CullFace = FACE_TYPE_BACK;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (!g_ContextCreated)
        {
            g_ContextCreated = true;
            return new Context(params);
        }
        else
        {
            return 0x0;
        }
    }

    void DeleteContext(HContext context)
    {
        assert(context);
        if (g_ContextCreated)
        {
            delete context;
            g_ContextCreated = false;
        }
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);
        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;
        context->m_WindowResizeCallback = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData = params->m_CloseCallbackUserData;
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        context->m_WindowWidth = params->m_Width;
        context->m_WindowHeight = params->m_Height;
        context->m_WindowOpened = 1;
        uint32_t buffer_size = 4 * context->m_WindowWidth * context->m_WindowHeight;
        context->m_MainFrameBuffer.m_ColorBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_DepthBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_StencilBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_ColorBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_DepthBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_StencilBufferSize = buffer_size;
        context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
        context->m_Program = 0x0;
        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: flash");
        }
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer;
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_WindowOpened = 0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
        }
    }

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
                return context->m_WindowOpened;
            default:
                return 0;
        }
    }

    uint32_t GetWidth(HContext context)
    {
        return stage->stageWidth;
    }

    uint32_t GetHeight(HContext context)
    {
        return stage->stageHeight;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return stage->stageWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return stage->stageHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        dmLogWarning("SetWindowSize is not supported on flash");
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
        uint32_t mask = 0;

        if (flags & dmGraphics::BUFFER_TYPE_COLOR_BIT) {
            mask |= flash::display3D::Context3DClearMask::COLOR;
        }
        if (flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT) {
            mask |= flash::display3D::Context3DClearMask::DEPTH;
        }
        if (flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT) {
            mask |= flash::display3D::Context3DClearMask::STENCIL;
        }

        context->m_Ctx3d->clear(red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f, depth, mask);
    }

    void Flip(HContext context)
    {
        context->m_Ctx3d->present();
    }

    void SetSwapInterval(HContext /*context*/, uint32_t /*swap_interval*/)
    {
        // TODO:
    }

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        // NOTE: We can't create the vertex buffer at this point
        // as a vertexbuffer in stage3d specifies number of
        // vertex-attributes and we don't know the vertex-layout at
        // this point
        VertexBuffer* vb = new VertexBuffer();
        vb->m_Size = size;
        vb->m_Buffer = new char[size];
        memcpy(vb->m_Buffer, data, size);
        return (uint32_t)vb;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        delete [] vb->m_Buffer;
        delete vb;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        delete [] vb->m_Buffer;
        vb->m_Buffer = new char[size];
        vb->m_Size = size;
        if (data != 0x0)
            memcpy(vb->m_Buffer, data, size);
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        if (offset + size <= vb->m_Size && data != 0x0)
            memcpy(&(vb->m_Buffer)[offset], data, size);
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        assert(false && "Not supported");
        return 0;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        assert(false && "Not supported");
        return false;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        // NOTE: We assume int-indices
        IndexBuffer* ib = new IndexBuffer();
        ib->m_IB = context->m_Ctx3d->createIndexBuffer(size / 4);
        ib->m_Size = size;
        ib->m_IB->uploadFromByteArray(internal::get_ram(), (int)data, 0, size / 4, (void*)data);

        return (uint32_t)ib;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        delete ib;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_IB->uploadFromByteArray(internal::get_ram(), (int)data, 0, size / 4, (void*) data);
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_IB->uploadFromByteArray(internal::get_ram(), (int)data, offset, size / 4, (void*) data);
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        assert(false && "Not supported");
        return 0;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        assert(false && "Not supported");
        return false;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return NewVertexDeclaration(context, element, count);
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(*vd));
        for (uint32_t i = 0; i < count; ++i)
        {
            assert(vd->m_Elements[element[i].m_Stream].m_Size == 0);
            vd->m_Elements[element[i].m_Stream] = element[i];
        }
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

/*    static void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);
        VertexStream& s = context->m_VertexStreams[stream];
        s.m_Size = 0;
        if (s.m_Buffer != 0x0)
        {
            delete [] (char*)s.m_Buffer;
            s.m_Buffer = 0x0;
        }
        s.m_Source = 0x0;
    }*/

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        // TODO: Create vertex buffer, convert (if required) and upload
        assert(context);
        assert(vertex_declaration);
        assert(vertex_buffer);
        VertexBuffer* vb = (VertexBuffer*)vertex_buffer;
        uint16_t stride = 0;
        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
            stride += vertex_declaration->m_Elements[i].m_Size * TYPE_SIZE[vertex_declaration->m_Elements[i].m_Type - dmGraphics::TYPE_BYTE];
        uint32_t offset = 0;
        for (uint16_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
        {
            VertexElement& ve = vertex_declaration->m_Elements[i];
            if (ve.m_Size > 0)
            {
                // TODO: float4 is hard-coded
                context->m_Ctx3d->setVertexBufferAt(i, vb->m_VB, offset, "float4");
                offset += ve.m_Size * TYPE_SIZE[ve.m_Type - dmGraphics::TYPE_BYTE];
            }
        }
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);
        // TODO: How to disable streams in stage3d?
        /*
        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
            if (vertex_declaration->m_Elements[i].m_Size > 0)
                DisableVertexStream(context, i);
                */
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        if (prim_type != PRIMITIVE_TRIANGLES) {
            dmLogError("Only triangles are supported in flash");
            return;
        }

        IndexBuffer* ib = (IndexBuffer*) index_buffer;
        context->m_Ctx3d->drawTriangles(ib->m_IB, first, count / 3);
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
        // TODO: Check that count is within range of context->m_IndexBuffer
        context->m_Ctx3d->drawTriangles(context->m_IndexBuffer, first, count / 3);
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        return (HProgram) new Program((VertexProgram*) vertex_program, (FragmentProgram*) fragment_program);
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        delete (Program*) program;
    }

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = new VertexProgram();
        p->m_Data = new char[program_size];
        memcpy(p->m_Data, program, program_size);
        return (uint32_t)p;
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = new FragmentProgram();
        p->m_Data = new char[program_size];
        memcpy(p->m_Data, program, program_size);
        return (uint32_t)p;
    }

    void ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
    }

    void ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void EnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_Program = (void*)program;
    }

    void DisableProgram(HContext context)
    {
        assert(context);
        context->m_Program = 0x0;
    }

    void ReloadProgram(HContext context, HProgram program)
    {
        // TODO:
        (void) context;
        (void) program;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        // TODO:
        return 0;
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        // TODO:
        assert(false);
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        // TODO:
        return -1;
    }

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        // TODO:
        assert(context);
    }

    void SetConstantV4(HContext context, const Vector4* data, int base_register)
    {
        // TODO:
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4));
    }

    void SetConstantM4(HContext context, const Vector4* data, int base_register)
    {
        // TODO:
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * 4);
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {
        // TODO:
    }

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        // TODO:
        assert(false && "Not supported");
        return 0;
    }

    void DeleteRenderTarget(HRenderTarget rt)
    {
        // TODO:
        assert(false && "Not supported");
    }

    void EnableRenderTarget(HContext context, HRenderTarget rendertarget)
    {
        // TODO:
        assert(context);
        assert(rendertarget);
        assert(false && "Not supported");
    }

    void DisableRenderTarget(HContext context, HRenderTarget rendertarget)
    {
        // TODO:
        assert(context);
        assert(rendertarget);
        assert(false && "Not supported");
    }

    HTexture GetRenderTargetTexture(HRenderTarget rendertarget, BufferType buffer_type)
    {
        // TODO:
        assert(false && "Not supported");
        return 0;
    }

    bool GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        // TODO:
        assert(false && "Not supported");
        return false;
    }

    void SetRenderTargetSize(HRenderTarget rt, uint32_t width, uint32_t height)
    {
        // TODO:
        assert(false && "Not supported");
    }

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    HTexture NewTexture(HContext context, const TextureParams& params)
    {
        Texture* tex = new Texture();
        tex->m_InternalFormat = TEXTURE_FORMAT_RGBA; // Actually BGRA in flash..
        String flash_format = flash::display3D::Context3DTextureFormat::BGRA;
        tex->m_Texture = context->m_Ctx3d->createTexture(params.m_Width, params.m_Height, flash_format, false, 0);
        SetTexture(tex, params);
        return tex;
    }

    void DeleteTexture(HTexture t)
    {
        assert(t);
        delete t;
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        assert(texture);
        texture->m_Format = params.m_Format;
        if (params.m_MipMap == 0)
        {
            texture->m_Width = params.m_Width;
            texture->m_Height = params.m_Height;

            if (params.m_OriginalWidth == 0) {
                texture->m_OriginalWidth = params.m_Width;
                texture->m_OriginalHeight = params.m_Height;
            } else {
                texture->m_OriginalWidth = params.m_OriginalWidth;
                texture->m_OriginalHeight = params.m_OriginalHeight;
            }
        }
        if (params.m_DataSize > 0)
        {
            // TODO: Upload texture data...
            //texture->m_Texture.uploadFromByteArray(internal::get_ram(), 0, params.m_)
        }
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        Texture* t = (Texture*) texture;
        context->m_Ctx3d->setTextureAt(unit, t->m_Texture);
    }

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        context->m_Ctx3d->setTextureAt(unit, internal::_null);
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        assert(false && "Not supported");
    }

    String ToFlash(BlendFactor f) {
        switch (f) {
            case BLEND_FACTOR_ZERO: return flash::display3D::Context3DBlendFactor::ZERO;
            case BLEND_FACTOR_ONE: return flash::display3D::Context3DBlendFactor::ONE;
            case BLEND_FACTOR_SRC_COLOR: return flash::display3D::Context3DBlendFactor::SOURCE_COLOR;
            case BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return flash::display3D::Context3DBlendFactor::ONE_MINUS_SOURCE_COLOR;
            case BLEND_FACTOR_DST_COLOR: return flash::display3D::Context3DBlendFactor::DESTINATION_COLOR;
            case BLEND_FACTOR_ONE_MINUS_DST_COLOR: return flash::display3D::Context3DBlendFactor::ONE_MINUS_DESTINATION_COLOR;
            case BLEND_FACTOR_SRC_ALPHA: return flash::display3D::Context3DBlendFactor::SOURCE_ALPHA;
            case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return flash::display3D::Context3DBlendFactor::ONE_MINUS_SOURCE_ALPHA;
            case BLEND_FACTOR_DST_ALPHA: return flash::display3D::Context3DBlendFactor::DESTINATION_ALPHA;
            case BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return flash::display3D::Context3DBlendFactor::ONE_MINUS_DESTINATION_ALPHA;

            default:
                dmLogWarning("Unsupported blend-factor: %d", f);
                return flash::display3D::Context3DBlendFactor::ONE;
        }
    }

    String ToFlash(FaceType f) {
        switch (f) {
        case FACE_TYPE_BACK:
            return flash::display3D::Context3DTriangleFace::BACK;
        case FACE_TYPE_FRONT:
            return flash::display3D::Context3DTriangleFace::FRONT;
        case FACE_TYPE_FRONT_AND_BACK:
            return flash::display3D::Context3DTriangleFace::FRONT_AND_BACK;
        default:
            assert(false);
        }
        return flash::display3D::Context3DTriangleFace::FRONT_AND_BACK;
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
        switch (state) {
            case STATE_DEPTH_TEST:
                context->m_Ctx3d->setDepthTest(context->m_DepthMask, flash::display3D::Context3DCompareMode::LESS);
                break;
            case STATE_STENCIL_TEST:
                break;
            case STATE_ALPHA_TEST:
                break;
            case STATE_BLEND:
                context->m_Ctx3d->setBlendFactors(ToFlash(context->m_SourceFactor), ToFlash(context->m_DestinationFactor));
                break;
            case STATE_CULL_FACE:
                context->m_Ctx3d->setCulling(ToFlash(context->m_CullFace));
                break;
            case STATE_POLYGON_OFFSET_FILL:
                break;
        }
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
        switch (state) {
            case STATE_DEPTH_TEST:
                context->m_Ctx3d->setDepthTest(context->m_DepthMask, flash::display3D::Context3DCompareMode::ALWAYS); // TODO: Is ALWAYS correct?
                break;
            case STATE_STENCIL_TEST:
                break;
            case STATE_ALPHA_TEST:
                break;
            case STATE_BLEND:
                context->m_Ctx3d->setBlendFactors(ToFlash(context->m_SourceFactor), ToFlash(context->m_DestinationFactor));
                break;
            case STATE_CULL_FACE:
                context->m_Ctx3d->setCulling(ToFlash(context->m_CullFace));
                break;
            case STATE_POLYGON_OFFSET_FILL:
                break;
        }
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        context->m_SourceFactor = source_factor;
        context->m_DestinationFactor = destinaton_factor;
        context->m_Ctx3d->setBlendFactors(ToFlash(context->m_SourceFactor), ToFlash(context->m_DestinationFactor));
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        context->m_RedMask = red;
        context->m_GreenMask = green;
        context->m_BlueMask = blue;
        context->m_AlphaMask = alpha;
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        context->m_DepthMask = mask; // TODO: Remove shadow state?
        context->m_Ctx3d->setDepthTest(mask, "LESS");
    }

    void SetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        context->m_DepthFunc = func;
        // TODO:
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        context->m_StencilMask = mask;
        // TODO:
    }

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_StencilFunc = func;
        context->m_StencilFuncRef = ref;
        context->m_StencilFuncMask = mask;
        // TODO:
    }

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_StencilOpSFail = sfail;
        context->m_StencilOpDPFail = dpfail;
        context->m_StencilOpDPPass = dppass;
        // TODO:
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        context->m_CullFace = face_type;
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
        // TODO:
    }

    bool AcquireSharedContext()
    {
        return false;
    }

    void UnacquireContext()
    {

    }

    void SetTextureAsync(HTexture texture, const TextureParams& params)
    {
        SetTexture(texture, params);
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

}
