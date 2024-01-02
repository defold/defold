// Copyright 2020-2023 The Defold Foundation
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

#include <string.h>
#include <assert.h>
#include <dmsdk/dlib/vmath.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <platform/platform_window.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"

#include "graphics_dx12_private.h"

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable();
    static bool                         DX12IsSupported();
    static bool 						DX12Initialize(HContext _context);
    static const int8_t    g_dx12_adapter_priority = 0;
    static GraphicsAdapter g_dx12_adapter(ADAPTER_FAMILY_DIRECTX);
    static DX12Context*    g_DX12Context = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterDX12, &g_dx12_adapter, DX12IsSupported, DX12RegisterFunctionTable, g_dx12_adapter_priority);

    DX12Context::DX12Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_NumFramesInFlight       = DM_MAX_FRAMES_IN_FLIGHT;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
        m_Window                  = params.m_Window;
        m_Width                   = params.m_Width;
        m_Height                  = params.m_Height;

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));
    }

    static HContext DX12NewContext(const ContextParams& params)
    {
        if (!g_DX12Context)
        {
            g_DX12Context = new DX12Context(params);

            if (DX12Initialize(g_DX12Context))
            {
                return g_DX12Context;
            }

            DeleteContext(g_DX12Context);
        }
        return 0x0;
    }

    static bool DX12IsSupported()
    {
        return true;
    }

    static void DX12DeleteContext(HContext _context)
    {
        assert(_context);
        if (g_DX12Context)
        {
            DX12Context* context = (DX12Context*) _context;
            delete (DX12Context*) context;
            g_DX12Context = 0x0;
        }
    }

    static bool DX12Initialize(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;

        if (context->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: null");
        }
        return true;
    }

    static void DX12Finalize()
    {

    }

    static void DX12CloseWindow(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
        }
    }

    static void DX12RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
    }

    static dmPlatform::HWindow DX12GetWindow(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Window;
    }

    static uint32_t DX12GetDisplayDpi(HContext context)
    {
        assert(context);
        return 0;
    }

    static uint32_t DX12GetWidth(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Width;
    }

    static uint32_t DX12GetHeight(HContext _context)
    {
        DX12Context* context = (DX12Context*) _context;
        return context->m_Height;
    }

    static void DX12SetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        DX12Context* context = (DX12Context*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void DX12ResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        DX12Context* context = (DX12Context*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void DX12GetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        DX12Context* context = (DX12Context*) _context;
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void DX12Clear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        DX12Context* context = (DX12Context*) _context;
    }

    static void DX12BeginFrame(HContext context)
    {
        // NOP
    }

    static void DX12Flip(HContext _context)
    {
    }

    static HVertexBuffer DX12NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return 0;
    }

    static void DX12DeleteVertexBuffer(HVertexBuffer buffer)
    {
    }

    static void DX12SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
    }

    static void DX12SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
    }

    static uint32_t DX12GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    static HIndexBuffer DX12NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
    	return 0;
    }

    static void DX12DeleteIndexBuffer(HIndexBuffer buffer)
    {
    }

    static void DX12SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
    }

    static void DX12SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
    }

    static bool DX12IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    static uint32_t DX12GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    static HVertexDeclaration DX12NewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        return NewVertexDeclaration(context, stream_declaration);
    }

    static HVertexDeclaration DX12NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        return 0;
    }

    bool DX12SetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset)
    {
        return true;
    }

    static void DX12DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
    }

    static void DX12EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
    }

    static void DX12EnableVertexDeclarationProgram(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        DX12EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    static void DX12DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
    }

    static void DX12HashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration)
    {
    }

    static uint32_t GetIndex(Type type, HIndexBuffer ib, uint32_t index)
    {
        return ~0;
    }

    static void DX12DrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
    }

    static void DX12Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
    }

    static HComputeProgram DX12NewComputeProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static HProgram DX12NewProgramFromCompute(HContext context, HComputeProgram compute_program)
    {
        return (HProgram) 0;
    }

    static void DX12DeleteComputeProgram(HComputeProgram prog)
    {
    }

    static HProgram DX12NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
    	return 0;
    }

    static void DX12DeleteProgram(HContext context, HProgram program)
    {
    }

    static HVertexProgram DX12NewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static HFragmentProgram DX12NewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static bool DX12ReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static bool DX12ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        return 0;
    }

    static void DX12DeleteVertexProgram(HVertexProgram program)
    {
    }

    static void DX12DeleteFragmentProgram(HFragmentProgram program)
    {
    }

    static ShaderDesc::Language DX12GetProgramLanguage(HProgram program)
    {
        return (ShaderDesc::Language) 0;
    }

    static ShaderDesc::Language DX12GetShaderProgramLanguage(HContext context)
    {
    	return ShaderDesc::LANGUAGE_GLSL_SM140;
    }

    static void DX12EnableProgram(HContext context, HProgram program)
    {
    }

    static void DX12DisableProgram(HContext context)
    {
    }

    static bool DX12ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
    	return true;
    }

    static uint32_t DX12GetAttributeCount(HProgram prog)
    {
        return 0;
    }

    static uint32_t GetElementCount(Type type)
    {
        return 0;
    }

    static void DX12GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
    }

    static uint32_t DX12GetUniformCount(HProgram prog)
    {
        return 0;
    }

    static uint32_t DX12GetVertexDeclarationStride(HVertexDeclaration vertex_declaration)
    {
        return 0;
    }

    static uint32_t DX12GetVertexStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash)
    {
        return dmGraphics::INVALID_STREAM_OFFSET;
    }

    static uint32_t DX12GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        return 0;
    }

    static HUniformLocation DX12GetUniformLocation(HProgram prog, const char* name)
    {
        return INVALID_UNIFORM_LOCATION;
    }

    static void DX12SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
    }

    static void DX12SetConstantV4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
    }

    static void DX12SetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
    }

    static void DX12SetSampler(HContext context, HUniformLocation location, int32_t unit)
    {
    }


    static HRenderTarget DX12NewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
    	return 0;
    }

    static void DX12DeleteRenderTarget(HRenderTarget render_target)
    {
    }

    static void DX12SetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
    }

    static HTexture DX12GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return 0;
    }

    static void DX12GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
    }

    static void DX12SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
    }

    static bool DX12IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return 0;
    }

    static uint32_t DX12GetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    static HTexture DX12NewTexture(HContext _context, const TextureCreationParams& params)
    {
        return 0;
    }

    static void DX12DeleteTexture(HTexture texture)
    {
    }

    static HandleResult DX12GetTextureHandle(HTexture texture, void** out_handle)
    {

        return HANDLE_RESULT_OK;
    }

    static void DX12SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
    }

    static void DX12SetTexture(HTexture texture, const TextureParams& params)
    {

    }

    static uint32_t DX12GetTextureResourceSize(HTexture texture)
    {
        return 0;
    }

    static uint16_t DX12GetTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t DX12GetTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t DX12GetOriginalTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t DX12GetOriginalTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static void DX12EnableTexture(HContext _context, uint32_t unit, uint8_t value_index, HTexture texture)
    {
    }

    static void DX12DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
    }

    static void DX12ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
    }

    static void DX12EnableState(HContext context, State state)
    {
    }

    static void DX12DisableState(HContext context, State state)
    {
    }

    static void DX12SetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
    }

    static void DX12SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
    }

    static void DX12SetDepthMask(HContext context, bool mask)
    {
    }

    static void DX12SetDepthFunc(HContext context, CompareFunc func)
    {
    }

    static void DX12SetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
    }

    static void DX12SetStencilMask(HContext context, uint32_t mask)
    {
    }

    static void DX12SetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
    }

    static void DX12SetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
    }

    static void DX12SetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
    }

    static void DX12SetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
    }

    static void DX12SetFaceWinding(HContext context, FaceWinding face_winding)
    {
    }

    static void DX12SetCullFace(HContext context, FaceType face_type)
    {
    }

    static void DX12SetPolygonOffset(HContext context, float factor, float units)
    {
    }

    static PipelineState DX12GetPipelineState(HContext context)
    {
    	PipelineState s;
    	return s;
    }

    static void DX12SetTextureAsync(HTexture texture, const TextureParams& params)
    {
    }

    static uint32_t DX12GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    static bool DX12IsExtensionSupported(HContext context, const char* extension)
    {
        return true;
    }

    static TextureType DX12GetTextureType(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Type;
    }

    static uint32_t DX12GetNumSupportedExtensions(HContext context)
    {
        return 0;
    }

    static const char* DX12GetSupportedExtension(HContext context, uint32_t index)
    {
        return "";
    }

    static uint8_t DX12GetNumTextureHandles(HTexture texture)
    {
        return 1;
    }

    static bool DX12IsContextFeatureSupported(HContext context, ContextFeature feature)
    {
        return true;
    }

    static uint16_t DX12GetTextureDepth(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t DX12GetTextureMipmapCount(HTexture texture)
    {
        return GetAssetFromContainer<DX12Texture>(g_DX12Context->m_AssetHandleContainer, texture)->m_MipMapCount;
    }

    static bool DX12IsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        assert(_context);
        if (asset_handle == 0)
        {
            return false;
        }
        DX12Context* context = (DX12Context*) _context;
        AssetType type       = GetAssetType(asset_handle);
        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<DX12Texture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<DX12RenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    static GraphicsAdapterFunctionTable DX12RegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, DX12);
        return fn_table;
    }
}

