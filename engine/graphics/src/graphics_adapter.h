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
    typedef HContext                     (*GraphicsAdapterGetContextCb)();

    struct GraphicsAdapter
    {
        GraphicsAdapter(AdapterFamily family)
        : m_Family(family) {}

        struct GraphicsAdapter*            m_Next;
        GraphicsAdapterRegisterFunctionsCb m_RegisterCb;
        GraphicsAdapterIsSupportedCb       m_IsSupportedCb;
        GraphicsAdapterGetContextCb        m_GetContextCb;
        AdapterFamily                      m_Family;
        int8_t                             m_Priority;
    };

    void RegisterGraphicsAdapter(GraphicsAdapter* adapter, GraphicsAdapterIsSupportedCb is_supported_cb, GraphicsAdapterRegisterFunctionsCb register_functions_cb, GraphicsAdapterGetContextCb get_context_cb, int8_t priority);

    // This snippet is taken from extension.h (SDK)
    #define DM_REGISTER_GRAPHICS_ADAPTER(adapter_name, adapter_ptr, is_supported_cb, register_functions_cb, get_context_cb, priority) extern "C" void adapter_name () { \
        RegisterGraphicsAdapter(adapter_ptr, is_supported_cb, register_functions_cb, get_context_cb, priority); \
    }

    typedef HContext (*NewContextFn)(const ContextParams& params);
    typedef void (*DeleteContextFn)(HContext context);
    typedef void (*FinalizeFn)();
    typedef void (*AppBootstrapFn)(int argc, char** argv, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn);
    typedef void (*CloseWindowFn)(HContext context);
    typedef dmPlatform::HWindow (*GetWindowFn)(HContext context);
    typedef uint32_t (*GetDisplayDpiFn)(HContext context);
    typedef uint32_t (*GetWidthFn)(HContext context);
    typedef uint32_t (*GetHeightFn)(HContext context);
    typedef PipelineState (*GetPipelineStateFn)(HContext context);
    typedef void (*SetWindowSizeFn)(HContext context, uint32_t width, uint32_t height);
    typedef void (*ResizeWindowFn)(HContext context, uint32_t width, uint32_t height);
    typedef void (*GetDefaultTextureFiltersFn)(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter);
    typedef void (*BeginFrameFn)(HContext context);
    typedef void (*FlipFn)(HContext context);
    typedef void (*ClearFn)(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);
    typedef HVertexBuffer (*NewVertexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*DeleteVertexBufferFn)(HVertexBuffer buffer);
    typedef void (*SetVertexBufferDataFn)(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*SetVertexBufferSubDataFn)(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    typedef uint32_t (*GetVertexBufferSizeFn)(HVertexBuffer buffer);
    typedef uint32_t (*GetMaxElementsVerticesFn)(HContext context);
    typedef HIndexBuffer (*NewIndexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*DeleteIndexBufferFn)(HIndexBuffer buffer);
    typedef void (*SetIndexBufferDataFn)(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef void (*SetIndexBufferSubDataFn)(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);
    typedef uint32_t (*GetIndexBufferSizeFn)(HIndexBuffer buffer);
    typedef bool (*IsIndexBufferFormatSupportedFn)(HContext context, IndexBufferFormat format);
    typedef uint32_t (*GetMaxElementsIndicesFn)(HContext context);
    typedef HVertexDeclaration (*NewVertexDeclarationFn)(HContext context, HVertexStreamDeclaration stream_declaration);
    typedef HVertexDeclaration (*NewVertexDeclarationStrideFn)(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride);
    typedef void (*EnableVertexDeclarationFn)(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program);
    typedef void (*DisableVertexDeclarationFn)(HContext context, HVertexDeclaration vertex_declaration);
    typedef uint32_t (*GetVertexDeclarationFn)(HVertexDeclaration vertex_declaration);
    typedef void (*EnableVertexBufferFn)(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index);
    typedef void (*DisableVertexBufferFn)(HContext context, HVertexBuffer vertex_buffer);
    typedef void (*DrawElementsFn)(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count);
    typedef void (*DrawFn)(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count);
    typedef void (*DispatchComputeFn)(HContext context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
    typedef HProgram (*NewProgramFn)(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size);
    typedef bool (*ReloadProgramFn)(HContext context, HProgram program, ShaderDesc* ddf);
    typedef void (*DeleteProgramFn)(HContext context, HProgram program);
    typedef bool (*IsShaderLanguageSupportedFn)(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type);
    typedef ShaderDesc::Language (*GetProgramLanguageFn)(HProgram program);
    typedef void (*EnableProgramFn)(HContext context, HProgram program);
    typedef void (*DisableProgramFn)(HContext context);
    typedef uint32_t (*GetAttributeCountFn)(HProgram prog);
    typedef void (*GetAttributeFn)(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location);
    typedef void (*SetConstantV4Fn)(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    typedef void (*SetConstantM4Fn)(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location);
    typedef void (*SetSamplerFn)(HContext context, HUniformLocation location, int32_t unit);
    typedef void (*SetViewportFn)(HContext context, int32_t x, int32_t y, int32_t width, int32_t height);
    typedef void (*EnableStateFn)(HContext context, State state);
    typedef void (*DisableStateFn)(HContext context, State state);
    typedef void (*SetBlendFuncFn)(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor);
    typedef void (*SetColorMaskFn)(HContext context, bool red, bool green, bool blue, bool alpha);
    typedef void (*SetDepthMaskFn)(HContext context, bool enable_mask);
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
    typedef HRenderTarget (*NewRenderTargetFn)(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params);
    typedef void (*DeleteRenderTargetFn)(HContext context, HRenderTarget render_target);
    typedef void (*SetRenderTargetFn)(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types);
    typedef HTexture (*GetRenderTargetTextureFn)(HContext context, HRenderTarget render_target, BufferType buffer_type);
    typedef void (*GetRenderTargetSizeFn)(HContext context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height);
    typedef void (*SetRenderTargetSizeFn)(HContext context, HRenderTarget render_target, uint32_t width, uint32_t height);
    typedef bool (*IsTextureFormatSupportedFn)(HContext context, TextureFormat format);
    typedef HTexture (*NewTextureFn)(HContext context, const TextureCreationParams& params);
    typedef void (*DeleteTextureFn)(HContext context, HTexture t);
    typedef void (*SetTextureFn)(HContext context, HTexture texture, const TextureParams& params);
    typedef void (*SetTextureAsyncFn)(HContext context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data);
    typedef void (*SetTextureParamsFn)(HContext context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy);
    typedef uint32_t (*GetTextureResourceSizeFn)(HContext context, HTexture texture);
    typedef uint16_t (*GetTextureWidthFn)(HContext context, HTexture texture);
    typedef uint16_t (*GetTextureHeightFn)(HContext context, HTexture texture);
    typedef uint16_t (*GetOriginalTextureWidthFn)(HContext context, HTexture texture);
    typedef uint16_t (*GetOriginalTextureHeightFn)(HContext context, HTexture texture);
    typedef uint16_t (*GetTextureDepthFn)(HContext context, HTexture texture);
    typedef uint8_t (*GetTextureMipmapCountFn)(HContext context, HTexture texture);
    typedef TextureType (*GetTextureTypeFn)(HContext context, HTexture texture);
    typedef void (*EnableTextureFn)(HContext context, uint32_t unit, uint8_t id_index, HTexture texture);
    typedef void (*DisableTextureFn)(HContext context, uint32_t unit, HTexture texture);
    typedef uint32_t (*GetMaxTextureSizeFn)(HContext context);
    typedef uint32_t (*GetTextureStatusFlagsFn)(HContext context, HTexture texture);
    typedef void (*ReadPixelsFn)(HContext context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size);
    typedef void (*RunApplicationLoopFn)(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running);
    typedef HandleResult (*GetTextureHandleFn)(HTexture texture, void** out_handle);
    typedef bool (*IsExtensionSupportedFn)(HContext context, const char* extension);
    typedef uint32_t (*GetNumSupportedExtensionsFn)(HContext context);
    typedef const char* (*GetSupportedExtensionFn)(HContext context, uint32_t index);
    typedef uint8_t (*GetNumTextureHandlesFn)(HContext context, HTexture texture);
    typedef uint32_t (*GetTextureUsageHintFlagsFn)(HContext context, HTexture texture);
    typedef uint8_t (*GetTexturePageCountFn)(HTexture texture);
    typedef bool (*IsContextFeatureSupportedFn)(HContext context, ContextFeature feature);
    typedef bool (*IsAssetHandleValidFn)(HContext context, HAssetHandle asset_handle);
    typedef void (*InvalidateGraphicsHandlesFn)(HContext context);
    typedef void (*GetViewportFn)(HContext context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height);
    typedef HUniformBuffer (*NewUniformBufferFn)(HContext context, const UniformBufferLayout& layout);
    typedef void (*DeleteUniformBufferFn)(HContext, HUniformBuffer uniform_buffer);
    typedef void (*SetUniformBufferFn)(HContext context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data);
    typedef void (*EnableUniformBufferFn)(HContext context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set);
    typedef void (*DisableUniformBufferFn)(HContext context, HUniformBuffer uniform_buffer);

    struct GraphicsAdapterFunctionTable
    {
        NewContextFn m_NewContext;
        DeleteContextFn m_DeleteContext;
        FinalizeFn m_Finalize;
        CloseWindowFn m_CloseWindow;
        GetWindowFn m_GetWindow;
        GetDisplayDpiFn m_GetDisplayDpi;
        GetWidthFn m_GetWidth;
        GetHeightFn m_GetHeight;
        SetWindowSizeFn m_SetWindowSize;
        ResizeWindowFn m_ResizeWindow;
        GetDefaultTextureFiltersFn m_GetDefaultTextureFilters;
        BeginFrameFn m_BeginFrame;
        FlipFn m_Flip;
        ClearFn m_Clear;
        NewVertexBufferFn m_NewVertexBuffer;
        DeleteVertexBufferFn m_DeleteVertexBuffer;
        SetVertexBufferDataFn m_SetVertexBufferData;
        SetVertexBufferSubDataFn m_SetVertexBufferSubData;
        GetVertexBufferSizeFn m_GetVertexBufferSize;
        GetMaxElementsVerticesFn m_GetMaxElementsVertices;
        NewIndexBufferFn m_NewIndexBuffer;
        DeleteIndexBufferFn m_DeleteIndexBuffer;
        SetIndexBufferDataFn m_SetIndexBufferData;
        SetIndexBufferSubDataFn m_SetIndexBufferSubData;
        GetIndexBufferSizeFn m_GetIndexBufferSize;
        IsIndexBufferFormatSupportedFn m_IsIndexBufferFormatSupported;
        GetMaxElementsIndicesFn m_GetMaxElementsIndices;
        NewVertexDeclarationFn m_NewVertexDeclaration;
        NewVertexDeclarationStrideFn m_NewVertexDeclarationStride;
        EnableVertexDeclarationFn m_EnableVertexDeclaration;
        DisableVertexDeclarationFn m_DisableVertexDeclaration;
        EnableVertexBufferFn m_EnableVertexBuffer;
        DisableVertexBufferFn m_DisableVertexBuffer;
        DrawElementsFn m_DrawElements;
        DrawFn m_Draw;
        DispatchComputeFn m_DispatchCompute;
        NewProgramFn m_NewProgram;
        DeleteProgramFn m_DeleteProgram;
        GetProgramLanguageFn m_GetProgramLanguage;
        IsShaderLanguageSupportedFn m_IsShaderLanguageSupported;
        EnableProgramFn m_EnableProgram;
        DisableProgramFn m_DisableProgram;
        ReloadProgramFn m_ReloadProgram;
        GetAttributeCountFn m_GetAttributeCount;
        GetAttributeFn m_GetAttribute;
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
        GetTextureTypeFn m_GetTextureType;
        GetOriginalTextureWidthFn m_GetOriginalTextureWidth;
        GetOriginalTextureHeightFn m_GetOriginalTextureHeight;
        GetTextureDepthFn m_GetTextureDepth;
        GetTextureMipmapCountFn m_GetTextureMipmapCount;
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
        GetNumTextureHandlesFn m_GetNumTextureHandles;
        GetTextureUsageHintFlagsFn m_GetTextureUsageHintFlags;
        GetTexturePageCountFn m_GetTexturePageCount;
        GetPipelineStateFn m_GetPipelineState;
        IsContextFeatureSupportedFn m_IsContextFeatureSupported;
        IsAssetHandleValidFn m_IsAssetHandleValid;
        InvalidateGraphicsHandlesFn m_InvalidateGraphicsHandles;
        GetViewportFn m_GetViewport;
        NewUniformBufferFn m_NewUniformBuffer;
        DeleteUniformBufferFn m_DeleteUniformBuffer;
        SetUniformBufferFn m_SetUniformBuffer;
        EnableUniformBufferFn m_EnableUniformBuffer;
        DisableUniformBufferFn m_DisableUniformBuffer;
    };

    #define DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, fn_name) \
        tbl.m_##fn_name = adapter_name##fn_name
    #define DM_REGISTER_GRAPHICS_FUNCTION_TABLE(tbl, adapter_name) \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewContext); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteContext); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, Finalize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, CloseWindow); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetWindow); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetDisplayDpi); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetWidth); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetHeight); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetWindowSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, ResizeWindow); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetDefaultTextureFilters); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, BeginFrame); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, Flip); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, Clear); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewVertexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteVertexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetVertexBufferData); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetVertexBufferSubData); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetVertexBufferSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetMaxElementsVertices); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewIndexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteIndexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetIndexBufferData); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetIndexBufferSubData); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetIndexBufferSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsIndexBufferFormatSupported); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewVertexDeclaration); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewVertexDeclarationStride); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableVertexDeclaration); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableVertexDeclaration); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableVertexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableVertexBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DrawElements); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, Draw); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DispatchCompute); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewProgram); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteProgram); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetProgramLanguage); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsShaderLanguageSupported); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableProgram); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableProgram); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, ReloadProgram); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetAttributeCount); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetAttribute); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetConstantV4); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetConstantM4); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetSampler); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetViewport); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableState); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableState); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetBlendFunc); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetColorMask); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetDepthMask); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetDepthFunc); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetScissor); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetStencilMask); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetStencilFunc); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetStencilFuncSeparate); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetStencilOp); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetStencilOpSeparate); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetCullFace); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetFaceWinding); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetPolygonOffset); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewRenderTarget); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteRenderTarget); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetRenderTarget); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetRenderTargetTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetRenderTargetSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetRenderTargetSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsTextureFormatSupported); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetTextureAsync); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetTextureParams); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureResourceSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureWidth); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureHeight); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetOriginalTextureWidth); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetOriginalTextureHeight); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureDepth); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureMipmapCount); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureType); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableTexture); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetMaxTextureSize); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureStatusFlags); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, ReadPixels); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, RunApplicationLoop); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureHandle); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetMaxElementsIndices); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsExtensionSupported); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetNumSupportedExtensions); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetSupportedExtension); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetNumTextureHandles); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTextureUsageHintFlags); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetTexturePageCount); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetPipelineState); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsContextFeatureSupported); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, IsAssetHandleValid); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, InvalidateGraphicsHandles); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, GetViewport); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, NewUniformBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DeleteUniformBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, SetUniformBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, EnableUniformBuffer); \
        DM_REGISTER_GRAPHICS_FUNCTION(tbl, adapter_name, DisableUniformBuffer);
}

#endif
