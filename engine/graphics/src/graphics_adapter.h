// Copyright 2020-2022 The Defold Foundation
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

#ifndef DM_GRAPHICS_ADAPTER_H
#define DM_GRAPHICS_ADAPTER_H

#include <dlib/hash.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/dlib/vmath.h>

namespace dmGraphics
{
    struct GraphicsAdapterFunctionTable;
    typedef GraphicsAdapterFunctionTable (*GraphicsAdapterRegisterFunctionsCb)();
    typedef bool                         (*GraphicsAdapterIsSupportedCb)();

    struct GraphicsAdapter
    {
        GraphicsAdapter(AdapterType adapter_type)
        : m_AdapterType(adapter_type) {}

        struct GraphicsAdapter*            m_Next;
        GraphicsAdapterRegisterFunctionsCb m_RegisterCb;
        GraphicsAdapterIsSupportedCb       m_IsSupportedCb;
        AdapterType                        m_AdapterType;
        int8_t                             m_Priority;
    };

    void RegisterGraphicsAdapter(GraphicsAdapter* adapter, GraphicsAdapterIsSupportedCb is_supported_cb, GraphicsAdapterRegisterFunctionsCb register_functions_cb, int8_t priority);

    // This snippet is taken from extension.h (SDK)
    #ifdef __GNUC__
        #define DM_REGISTER_GRAPHICS_ADAPTER(adapter_name, adapter_ptr, is_supported_cb, register_functions_cb, priority) extern "C" void __attribute__((constructor)) adapter_name () { \
            RegisterGraphicsAdapter(adapter_ptr, is_supported_cb, register_functions_cb, priority); \
        }
    #else
        #define DM_REGISTER_GRAPHICS_ADAPTER(adapter_name, adapter_ptr, is_supported_cb, register_functions_cb, priority) extern "C" void adapter_name () { \
            RegisterGraphicsAdapter(adapter_ptr, is_supported_cb, register_functions_cb, priority); \
            }\
            int adapter_name ## Wrapper(void) { adapter_name(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## adapter_name)(void) = adapter_name ## Wrapper;
    #endif

    typedef HContext (*NewContextFn)(const ContextParams& params);
    typedef void (*DeleteContextFn)(HContext context);
    typedef bool (*InitializeFn)();
    typedef void (*FinalizeFn)();
    typedef void (*AppBootstrapFn)(int argc, char** argv, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);
    typedef uint32_t (*GetWindowRefreshRateFn)(HContext context);
    typedef WindowResult (*OpenWindowFn)(HContext context, WindowParams *params);
    typedef void (*CloseWindowFn)(HContext context);
    typedef void (*IconifyWindowFn)(HContext context);
    typedef uint32_t (*GetWindowStateFn)(HContext context, WindowState state);
    typedef uint32_t (*GetDisplayDpiFn)(HContext context);
    typedef uint32_t (*GetWidthFn)(HContext context);
    typedef uint32_t (*GetHeightFn)(HContext context);
    typedef uint32_t (*GetWindowWidthFn)(HContext context);
    typedef uint32_t (*GetWindowHeightFn)(HContext context);
    typedef PipelineState (*GetPipelineStateFn)(HContext context);
    typedef float (*GetDisplayScaleFactorFn)(HContext context);
    typedef void (*SetWindowSizeFn)(HContext context, uint32_t width, uint32_t height);
    typedef void (*ResizeWindowFn)(HContext context, uint32_t width, uint32_t height);
    typedef void (*GetDefaultTextureFiltersFn)(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter);
    typedef void (*BeginFrameFn)(HContext context);
    typedef void (*FlipFn)(HContext context);
    typedef void (*SetSwapIntervalFn)(HContext context, uint32_t swap_interval);
    typedef void (*ClearFn)(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);
    typedef HVertexBuffer (*NewVertexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*DeleteVertexBufferFn)(HVertexBuffer buffer);
    typedef void (*SetVertexBufferDataFn)(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*SetVertexBufferSubDataFn)(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    typedef void* (*MapVertexBufferFn)(HVertexBuffer buffer, BufferAccess access);
    typedef bool (*UnmapVertexBufferFn)(HVertexBuffer buffer);
    typedef uint32_t (*GetMaxElementsVerticesFn)(HContext context);
    typedef HIndexBuffer (*NewIndexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*DeleteIndexBufferFn)(HIndexBuffer buffer);
    typedef void (*SetIndexBufferDataFn)(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*SetIndexBufferSubDataFn)(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    typedef void* (*MapIndexBufferFn)(HIndexBuffer buffer, BufferAccess access);
    typedef bool (*UnmapIndexBufferFn)(HIndexBuffer buffer);
    typedef bool (*IsIndexBufferFormatSupportedFn)(HContext context, IndexBufferFormat format);
    typedef uint32_t (*GetMaxElementsIndicesFn)(HContext context);
    typedef HVertexDeclaration (*NewVertexDeclarationFn)(HContext context, VertexElement* element, uint32_t count);
    typedef HVertexDeclaration (*NewVertexDeclarationStrideFn)(HContext context, VertexElement* element, uint32_t count, uint32_t stride);
    typedef bool (*SetStreamOffsetFn)(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset);
    typedef void (*DeleteVertexDeclarationFn)(HVertexDeclaration vertex_declaration);
    typedef void (*EnableVertexDeclarationFn)(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer);
    typedef void (*EnableVertexDeclarationProgramFn)(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program);
    typedef void (*DisableVertexDeclarationFn)(HContext context, HVertexDeclaration vertex_declaration);
    typedef void (*HashVertexDeclarationFn)(HashState32* state, HVertexDeclaration vertex_declaration);
    typedef void (*DrawElementsFn)(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer);
    typedef void (*DrawFn)(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count);
    typedef HVertexProgram (*NewVertexProgramFn)(HContext context, ShaderDesc::Shader* ddf);
    typedef HFragmentProgram (*NewFragmentProgramFn)(HContext context, ShaderDesc::Shader* ddf);
    typedef HProgram (*NewProgramFn)(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program);
    typedef void (*DeleteProgramFn)(HContext context, HProgram program);
    typedef bool (*ReloadVertexProgramFn)(HVertexProgram prog, ShaderDesc::Shader* ddf);
    typedef bool (*ReloadFragmentProgramFn)(HFragmentProgram prog, ShaderDesc::Shader* ddf);
    typedef void (*DeleteVertexProgramFn)(HVertexProgram prog);
    typedef void (*DeleteFragmentProgramFn)(HFragmentProgram prog);
    typedef ShaderDesc::Language (*GetShaderProgramLanguageFn)(HContext context);
    typedef void (*EnableProgramFn)(HContext context, HProgram program);
    typedef void (*DisableProgramFn)(HContext context);
    typedef bool (*ReloadProgramFn)(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program);
    typedef uint32_t (*GetUniformNameFn)(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size);
    typedef uint32_t (*GetUniformCountFn)(HProgram prog);
    typedef int32_t (* GetUniformLocationFn)(HProgram prog, const char* name);
    typedef void (*SetConstantV4Fn)(HContext context, const dmVMath::Vector4* data, int count, int base_register);
    typedef void (*SetConstantM4Fn)(HContext context, const dmVMath::Vector4* data, int count, int base_register);
    typedef void (*SetSamplerFn)(HContext context, int32_t location, int32_t unit);
    typedef void (*SetViewportFn)(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);
    typedef void (*EnableStateFn)(HContext context, State state);
    typedef void (*DisableStateFn)(HContext context, State state);
    typedef void (*SetBlendFuncFn)(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);
    typedef void (*SetColorMaskFn)(HContext context, bool red, bool green, bool blue, bool alpha);
    typedef void (*SetDepthMaskFn)(HContext context, bool mask);
    typedef void (*SetDepthFuncFn)(HContext context, CompareFunc func);
    typedef void (*SetScissorFn)(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);
    typedef void (*SetStencilMaskFn)(HContext context, uint32_t mask);
    typedef void (*SetStencilFuncFn)(HContext context, CompareFunc func, uint32_t ref, uint32_t mask);
    typedef void (*SetStencilFuncSeparateFn)(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask);
    typedef void (*SetStencilOpFn)(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass);
    typedef void (*SetStencilOpSeparateFn)(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass);
    typedef void (*SetCullFaceFn)(HContext context, FaceType face_type);
    typedef void (*SetFaceWindingFn)(HContext context, FaceWinding face_winding);
    typedef void (*SetPolygonOffsetFn)(HContext context, float factor, float units);
    typedef HRenderTarget (*NewRenderTargetFn)(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT]);
    typedef void (*DeleteRenderTargetFn)(HRenderTarget render_target);
    typedef void (*SetRenderTargetFn)(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);
    typedef HTexture (*GetRenderTargetTextureFn)(HRenderTarget render_target, BufferType buffer_type);
    typedef void (*GetRenderTargetSizeFn)(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);
    typedef void (*SetRenderTargetSizeFn)(HRenderTarget render_target, uint32_t width, uint32_t height);
    typedef bool (*IsTextureFormatSupportedFn)(HContext context, TextureFormat format);
    typedef HTexture (*NewTextureFn)(HContext context, const TextureCreationParams& params);
    typedef void (*DeleteTextureFn)(HTexture t);
    typedef void (*SetTextureFn)(HTexture texture, const TextureParams& params);
    typedef void (*SetTextureAsyncFn)(HTexture texture, const TextureParams& paramsa);
    typedef void (*SetTextureParamsFn)(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
    typedef uint32_t (*GetTextureResourceSizeFn)(HTexture texture);
    typedef uint16_t (*GetTextureWidthFn)(HTexture texture);
    typedef uint16_t (*GetTextureHeightFn)(HTexture texture);
    typedef uint16_t (*GetOriginalTextureWidthFn)(HTexture texture);
    typedef uint16_t (*GetOriginalTextureHeightFn)(HTexture texture);
    typedef void (*EnableTextureFn)(HContext context, uint32_t unit, HTexture texture);
    typedef void (*DisableTextureFn)(HContext context, uint32_t unit, HTexture texture);
    typedef uint32_t (*GetMaxTextureSizeFn)(HContext context);
    typedef uint32_t (*GetTextureStatusFlagsFn)(HTexture texture);
    typedef void (*ReadPixelsFn)(HContext context, void* buffer, uint32_t buffer_size);
    typedef void (*RunApplicationLoopFn)(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running);
    typedef HandleResult (*GetTextureHandleFn)(HTexture texture, void** out_handle);
    typedef bool (*IsExtensionSupportedFn)(HContext context, const char* extension);
    typedef uint32_t (*GetNumSupportedExtensionsFn)(HContext context);
    typedef const char* (*GetSupportedExtensionFn)(HContext context, uint32_t index);
    typedef bool (*IsMultiTargetRenderingSupportedFn)(HContext context);

    struct GraphicsAdapterFunctionTable
    {
        NewContextFn m_NewContext;
        DeleteContextFn m_DeleteContext;
        InitializeFn m_Initialize;
        FinalizeFn m_Finalize;
        GetWindowRefreshRateFn m_GetWindowRefreshRate;
        OpenWindowFn m_OpenWindow;
        CloseWindowFn m_CloseWindow;
        IconifyWindowFn m_IconifyWindow;
        GetWindowStateFn m_GetWindowState;
        GetDisplayDpiFn m_GetDisplayDpi;
        GetWidthFn m_GetWidth;
        GetHeightFn m_GetHeight;
        GetWindowWidthFn m_GetWindowWidth;
        GetWindowHeightFn m_GetWindowHeight;
        GetDisplayScaleFactorFn m_GetDisplayScaleFactor;
        SetWindowSizeFn m_SetWindowSize;
        ResizeWindowFn m_ResizeWindow;
        GetDefaultTextureFiltersFn m_GetDefaultTextureFilters;
        BeginFrameFn m_BeginFrame;
        FlipFn m_Flip;
        SetSwapIntervalFn m_SetSwapInterval;
        ClearFn m_Clear;
        NewVertexBufferFn m_NewVertexBuffer;
        DeleteVertexBufferFn m_DeleteVertexBuffer;
        SetVertexBufferDataFn m_SetVertexBufferData;
        SetVertexBufferSubDataFn m_SetVertexBufferSubData;
        MapVertexBufferFn m_MapVertexBuffer;
        UnmapVertexBufferFn m_UnmapVertexBuffer;
        GetMaxElementsVerticesFn m_GetMaxElementsVertices;
        NewIndexBufferFn m_NewIndexBuffer;
        DeleteIndexBufferFn m_DeleteIndexBuffer;
        SetIndexBufferDataFn m_SetIndexBufferData;
        SetIndexBufferSubDataFn m_SetIndexBufferSubData;
        MapIndexBufferFn m_MapIndexBuffer;
        UnmapIndexBufferFn m_UnmapIndexBuffer;
        IsIndexBufferFormatSupportedFn m_IsIndexBufferFormatSupported;
        GetMaxElementsIndicesFn m_GetMaxElementsIndices;
        NewVertexDeclarationFn m_NewVertexDeclaration;
        NewVertexDeclarationStrideFn m_NewVertexDeclarationStride;
        SetStreamOffsetFn m_SetStreamOffset;
        DeleteVertexDeclarationFn m_DeleteVertexDeclaration;
        EnableVertexDeclarationFn m_EnableVertexDeclaration;
        EnableVertexDeclarationProgramFn m_EnableVertexDeclarationProgram;
        DisableVertexDeclarationFn m_DisableVertexDeclaration;
        HashVertexDeclarationFn m_HashVertexDeclaration;
        DrawElementsFn m_DrawElements;
        DrawFn m_Draw;
        NewVertexProgramFn m_NewVertexProgram;
        NewFragmentProgramFn m_NewFragmentProgram;
        NewProgramFn m_NewProgram;
        DeleteProgramFn m_DeleteProgram;
        ReloadVertexProgramFn m_ReloadVertexProgram;
        ReloadFragmentProgramFn m_ReloadFragmentProgram;
        DeleteVertexProgramFn m_DeleteVertexProgram;
        DeleteFragmentProgramFn m_DeleteFragmentProgram;
        GetShaderProgramLanguageFn m_GetShaderProgramLanguage;
        EnableProgramFn m_EnableProgram;
        DisableProgramFn m_DisableProgram;
        ReloadProgramFn m_ReloadProgram;
        GetUniformNameFn m_GetUniformName;
        GetUniformCountFn m_GetUniformCount;
        GetUniformLocationFn m_GetUniformLocation;
        SetConstantV4Fn m_SetConstantV4;
        SetConstantM4Fn m_SetConstantM4;
        SetSamplerFn m_SetSampler;
        SetViewportFn m_SetViewport;
        EnableStateFn m_EnableState;
        DisableStateFn m_DisableState;
        SetBlendFuncFn m_SetBlendFunc;
        SetColorMaskFn m_SetColorMask;
        SetDepthMaskFn m_SetDepthMask;
        SetDepthFuncFn m_SetDepthFunc;
        SetScissorFn m_SetScissor;
        SetStencilMaskFn m_SetStencilMask;
        SetStencilFuncFn m_SetStencilFunc;
        SetStencilFuncSeparateFn m_SetStencilFuncSeparate;
        SetStencilOpFn m_SetStencilOp;
        SetStencilOpSeparateFn m_SetStencilOpSeparate;
        SetCullFaceFn m_SetCullFace;
        SetFaceWindingFn m_SetFaceWinding;
        SetPolygonOffsetFn m_SetPolygonOffset;
        NewRenderTargetFn m_NewRenderTarget;
        DeleteRenderTargetFn m_DeleteRenderTarget;
        SetRenderTargetFn m_SetRenderTarget;
        GetRenderTargetTextureFn m_GetRenderTargetTexture;
        GetRenderTargetSizeFn m_GetRenderTargetSize;
        SetRenderTargetSizeFn m_SetRenderTargetSize;
        IsTextureFormatSupportedFn m_IsTextureFormatSupported;
        NewTextureFn m_NewTexture;
        DeleteTextureFn m_DeleteTexture;
        SetTextureFn m_SetTexture;
        SetTextureAsyncFn m_SetTextureAsync;
        SetTextureParamsFn m_SetTextureParams;
        GetTextureResourceSizeFn m_GetTextureResourceSize;
        GetTextureWidthFn m_GetTextureWidth;
        GetTextureHeightFn m_GetTextureHeight;
        GetOriginalTextureWidthFn m_GetOriginalTextureWidth;
        GetOriginalTextureHeightFn m_GetOriginalTextureHeight;
        EnableTextureFn m_EnableTexture;
        DisableTextureFn m_DisableTexture;
        GetMaxTextureSizeFn m_GetMaxTextureSize;
        GetTextureStatusFlagsFn m_GetTextureStatusFlags;
        ReadPixelsFn m_ReadPixels;
        RunApplicationLoopFn m_RunApplicationLoop;
        GetTextureHandleFn m_GetTextureHandle;
        IsExtensionSupportedFn m_IsExtensionSupported;
        GetNumSupportedExtensionsFn m_GetNumSupportedExtensions;
        GetSupportedExtensionFn m_GetSupportedExtension;
        IsMultiTargetRenderingSupportedFn m_IsMultiTargetRenderingSupported;
        GetPipelineStateFn m_GetPipelineState;
    };
}

#endif
