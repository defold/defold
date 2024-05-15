// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GRAPHICS_H
#define DMSDK_GRAPHICS_H

#include <stdint.h>

/*# Graphics API documentation
 * [file:<dmsdk/graphics/graphics.h>]
 *
 * Graphics API
 *
 * @document
 * @name Graphics
 * @namespace dmGraphics
 */

namespace dmGraphics
{
    /*#
     * Context handle
     * @typedef
     * @name HContext
     */
    typedef void* HContext;

    /*#
     * Texture handle
     * @typedef
     * @name HTexture
     */
    typedef uint64_t HTexture;

    /*#
     * Rendertarget handle
     * @typedef
     * @name HRenderTarget
     */
    typedef uint64_t HRenderTarget; // Where is this currently used?

    /*#
     * Vertex program handle
     * @typedef
     * @name HVertexProgram
     */
    typedef uintptr_t HVertexProgram;

    /*#
     * Fragment program handle
     * @typedef
     * @name HFragmentProgram
     */
    typedef uintptr_t HFragmentProgram;

    /*#
     * Program handle
     * @typedef
     * @name HProgram
     */
    typedef uintptr_t HProgram;

    /*#
     * Vertex buffer handle
     * @typedef
     * @name HVertexBuffer
     */
    typedef uintptr_t HVertexBuffer;

    /*#
     * Index buffer handle
     * @typedef
     * @name HIndexBuffer
     */
    typedef uintptr_t HIndexBuffer;

    /*#
     * Storage buffer handle
     * @typedef
     * @name HStorageBuffer
     */
    typedef uintptr_t HStorageBuffer;

    /*#
     * Uniform location handle
     * @typedef
     * @name HUniformLocation
     */
    typedef int64_t HUniformLocation;

    /*#
     * Vertex declaration handle
     * @typedef
     * @name HVertexDeclaration
     */
    typedef struct VertexDeclaration* HVertexDeclaration;

    /*#
     * Vertex stream declaration handle
     * @typedef
     * @name HVertexStreamDeclaration
     */
    typedef struct VertexStreamDeclaration* HVertexStreamDeclaration;

    /*#
     * PipelineState handle
     * @typedef
     * @name HPipelineState
     */
    typedef struct PipelineState* HPipelineState;

    /*#
     * Invalid stream offset
     * @constant
     * @name INVALID_STREAM_OFFSET
     */
    const uint32_t INVALID_STREAM_OFFSET = 0xFFFFFFFF;

    /*#
     * Max buffer color attachments
     * @constant
     * @name MAX_BUFFER_COLOR_ATTACHMENTS
     */
    const uint8_t  MAX_BUFFER_COLOR_ATTACHMENTS = 4;

    /*#
     * @enum
     * @name HandleResult
     * @member HANDLE_RESULT_OK
     * @member HANDLE_RESULT_NOT_AVAILABLE
     * @member HANDLE_RESULT_ERROR
     */
    enum HandleResult
    {
        HANDLE_RESULT_OK = 0,
        HANDLE_RESULT_NOT_AVAILABLE = -1,
        HANDLE_RESULT_ERROR = -2
    };

    /*#
     * @enum
     * @name RenderTargetAttachment
     * @member ATTACHMENT_COLOR
     * @member ATTACHMENT_DEPTH
     * @member ATTACHMENT_STENCIL
     * @member MAX_ATTACHMENT_COUNT
     */
    enum RenderTargetAttachment
    {
        ATTACHMENT_COLOR     = 0,
        ATTACHMENT_DEPTH     = 1,
        ATTACHMENT_STENCIL   = 2,
        MAX_ATTACHMENT_COUNT = 3
    };

    /*#
     * @enum
     * @name AttachmentOp
     * @member ATTACHMENT_OP_DONT_CARE
     * @member ATTACHMENT_OP_LOAD
     * @member ATTACHMENT_OP_STORE
     * @member ATTACHMENT_OP_CLEAR
     */
    enum AttachmentOp
    {
        ATTACHMENT_OP_DONT_CARE,
        ATTACHMENT_OP_LOAD,
        ATTACHMENT_OP_STORE,
        ATTACHMENT_OP_CLEAR,
    };

    /*#
     * @enum
     * @name TextureFormat
     * @member TEXTURE_FORMAT_LUMINANCE
     * @member TEXTURE_FORMAT_LUMINANCE_ALPHA
     * @member TEXTURE_FORMAT_RGB
     * @member TEXTURE_FORMAT_RGBA
     * @member TEXTURE_FORMAT_RGB_16BPP
     * @member TEXTURE_FORMAT_RGBA_16BPP
     * @member TEXTURE_FORMAT_DEPTH
     * @member TEXTURE_FORMAT_STENCIL
     * @member TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
     * @member TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
     * @member TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
     * @member TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
     * @member TEXTURE_FORMAT_RGB_ETC1
     * @member TEXTURE_FORMAT_R_ETC2
     * @member TEXTURE_FORMAT_RG_ETC2
     * @member TEXTURE_FORMAT_RGBA_ETC2
     * @member TEXTURE_FORMAT_RGBA_ASTC_4x4
     * @member TEXTURE_FORMAT_RGB_BC1
     * @member TEXTURE_FORMAT_RGBA_BC3
     * @member TEXTURE_FORMAT_R_BC4
     * @member TEXTURE_FORMAT_RG_BC5
     * @member TEXTURE_FORMAT_RGBA_BC7
     * @member TEXTURE_FORMAT_RGB16F
     * @member TEXTURE_FORMAT_RGB32F
     * @member TEXTURE_FORMAT_RGBA16F
     * @member TEXTURE_FORMAT_RGBA32F
     * @member TEXTURE_FORMAT_R16F
     * @member TEXTURE_FORMAT_RG16F
     * @member TEXTURE_FORMAT_R32F
     * @member TEXTURE_FORMAT_RG32F
     * @member TEXTURE_FORMAT_RGBA32UI
     * @member TEXTURE_FORMAT_COUNT
     */
    enum TextureFormat
    {
        TEXTURE_FORMAT_LUMINANCE            = 0,
        TEXTURE_FORMAT_LUMINANCE_ALPHA      = 1,
        TEXTURE_FORMAT_RGB                  = 2,
        TEXTURE_FORMAT_RGBA                 = 3,
        TEXTURE_FORMAT_RGB_16BPP            = 4,
        TEXTURE_FORMAT_RGBA_16BPP           = 5,
        TEXTURE_FORMAT_DEPTH                = 6,
        TEXTURE_FORMAT_STENCIL              = 7,
        // Compressed formats
        TEXTURE_FORMAT_RGB_PVRTC_2BPPV1     = 8,
        TEXTURE_FORMAT_RGB_PVRTC_4BPPV1     = 9,
        TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1    = 10,
        TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1    = 11,
        TEXTURE_FORMAT_RGB_ETC1             = 12,
        TEXTURE_FORMAT_R_ETC2               = 13,
        TEXTURE_FORMAT_RG_ETC2              = 14,
        TEXTURE_FORMAT_RGBA_ETC2            = 15,
        TEXTURE_FORMAT_RGBA_ASTC_4x4        = 16,
        TEXTURE_FORMAT_RGB_BC1              = 17,
        TEXTURE_FORMAT_RGBA_BC3             = 18,
        TEXTURE_FORMAT_R_BC4                = 19,
        TEXTURE_FORMAT_RG_BC5               = 20,
        TEXTURE_FORMAT_RGBA_BC7             = 21,
        // Floating point texture formats
        TEXTURE_FORMAT_RGB16F               = 22,
        TEXTURE_FORMAT_RGB32F               = 23,
        TEXTURE_FORMAT_RGBA16F              = 24,
        TEXTURE_FORMAT_RGBA32F              = 25,
        TEXTURE_FORMAT_R16F                 = 26,
        TEXTURE_FORMAT_RG16F                = 27,
        TEXTURE_FORMAT_R32F                 = 28,
        TEXTURE_FORMAT_RG32F                = 29,
        // Internal formats (not exposed via script APIs)
        TEXTURE_FORMAT_RGBA32UI             = 30,
        TEXTURE_FORMAT_BGRA8U               = 31,
        TEXTURE_FORMAT_R32UI                = 32,

        TEXTURE_FORMAT_COUNT
    };

    /*#
     * Get the attachment texture from a render target. Returns zero if no such attachment texture exists.
     * @name GetRenderTargetAttachment
     * @param render_target [type: dmGraphics::HRenderTarget] the render target
     * @param attachment_type [type: dmGraphics::RenderTargetAttachment] the attachment to get
     * @return attachment [type: dmGraphics::HTexture] the attachment texture
     */
    HTexture GetRenderTargetAttachment(HRenderTarget render_target, RenderTargetAttachment attachment_type);

    /*#
     * Get the native graphics API texture object from an engine texture handle. This depends on the graphics backend and is not
     * guaranteed to be implemented on the currently running adapter.
     * @name GetTextureHandle
     * @param texture [type: dmGraphics::HTexture] the texture handle
     * @param out_handle [type: void**] a pointer to where the raw object should be stored
     * @return handle_result [type: dmGraphics::HandleResult] the result of the query
     */
    HandleResult GetTextureHandle(HTexture texture, void** out_handle);

    /*#
     * @enum
     * @name CompareFunc
     * @member COMPARE_FUNC_NEVER
     * @member COMPARE_FUNC_LESS
     * @member COMPARE_FUNC_LEQUAL
     * @member COMPARE_FUNC_GREATER
     * @member COMPARE_FUNC_GEQUAL
     * @member COMPARE_FUNC_EQUAL
     * @member COMPARE_FUNC_NOTEQUAL
     * @member COMPARE_FUNC_ALWAYS
     */
    enum CompareFunc
    {
        COMPARE_FUNC_NEVER    = 0,
        COMPARE_FUNC_LESS     = 1,
        COMPARE_FUNC_LEQUAL   = 2,
        COMPARE_FUNC_GREATER  = 3,
        COMPARE_FUNC_GEQUAL   = 4,
        COMPARE_FUNC_EQUAL    = 5,
        COMPARE_FUNC_NOTEQUAL = 6,
        COMPARE_FUNC_ALWAYS   = 7,
    };

    /*#
     * @enum
     * @name FaceWinding
     * @member FACE_WINDING_CCW
     * @member FACE_WINDING_CW
     */
    enum FaceWinding
    {
        FACE_WINDING_CCW = 0,
        FACE_WINDING_CW  = 1,
    };

    /*#
     * @enum
     * @name StencilOp
     * @member STENCIL_OP_KEEP
     * @member STENCIL_OP_ZERO
     * @member STENCIL_OP_REPLACE
     * @member STENCIL_OP_INCR
     * @member STENCIL_OP_INCR_WRAP
     * @member STENCIL_OP_DECR
     * @member STENCIL_OP_DECR_WRAP
     * @member STENCIL_OP_INVERT
     */
    enum StencilOp
    {
        STENCIL_OP_KEEP      = 0,
        STENCIL_OP_ZERO      = 1,
        STENCIL_OP_REPLACE   = 2,
        STENCIL_OP_INCR      = 3,
        STENCIL_OP_INCR_WRAP = 4,
        STENCIL_OP_DECR      = 5,
        STENCIL_OP_DECR_WRAP = 6,
        STENCIL_OP_INVERT    = 7,
    };

    /*#
     * @enum
     * @name BufferUsage
     * @member BUFFER_USAGE_STREAM_DRAW
     * @member BUFFER_USAGE_DYNAMIC_DRAW
     * @member BUFFER_USAGE_STATIC_DRAW     Preferred for buffers that never change
     */
    enum BufferUsage
    {
        BUFFER_USAGE_STREAM_DRAW  = 0,
        BUFFER_USAGE_DYNAMIC_DRAW = 1,
        BUFFER_USAGE_STATIC_DRAW  = 2,
        BUFFER_USAGE_TRANSFER     = 4,
    };

    /*#
     * @enum
     * @name BufferAccess
     * @member BUFFER_ACCESS_READ_ONLY
     * @member BUFFER_ACCESS_WRITE_ONLY
     * @member BUFFER_ACCESS_READ_WRITE
     */
    enum BufferAccess
    {
        BUFFER_ACCESS_READ_ONLY  = 0,
        BUFFER_ACCESS_WRITE_ONLY = 1,
        BUFFER_ACCESS_READ_WRITE = 2,
    };


    /*#
     * @enum
     * @name IndexBufferFormat
     * @member INDEXBUFFER_FORMAT_16
     * @member INDEXBUFFER_FORMAT_32
     */
    enum IndexBufferFormat
    {
        INDEXBUFFER_FORMAT_16 = 0,
        INDEXBUFFER_FORMAT_32 = 1,
    };

    /*#
     * Primitive type
     * @enum
     * @name PrimitiveType
     * @member PRIMITIVE_LINES
     * @member PRIMITIVE_TRIANGLES
     * @member PRIMITIVE_TRIANGLE_STRIP
     */
    enum PrimitiveType
    {
        PRIMITIVE_LINES          = 0,
        PRIMITIVE_TRIANGLES      = 1,
        PRIMITIVE_TRIANGLE_STRIP = 2,
    };

    /*#
     * Data type
     * @enum
     * @name Type
     * @member TYPE_BYTE
     * @member TYPE_UNSIGNED_BYTE
     * @member TYPE_SHORT
     * @member TYPE_UNSIGNED_SHORT
     * @member TYPE_INT
     * @member TYPE_UNSIGNED_INT
     * @member TYPE_FLOAT
     * @member TYPE_FLOAT_VEC4
     * @member TYPE_FLOAT_MAT4
     * @member TYPE_SAMPLER_2D
     * @member TYPE_SAMPLER_CUBE
     * @member TYPE_SAMPLER_2D_ARRAY
     * @member TYPE_FLOAT_VEC2
     * @member TYPE_FLOAT_VEC3
     * @member TYPE_FLOAT_MAT2
     * @member TYPE_FLOAT_MAT3
     * @member TYPE_IMAGE_2D
     */
    enum Type
    {
        TYPE_BYTE             = 0,
        TYPE_UNSIGNED_BYTE    = 1,
        TYPE_SHORT            = 2,
        TYPE_UNSIGNED_SHORT   = 3,
        TYPE_INT              = 4,
        TYPE_UNSIGNED_INT     = 5,
        TYPE_FLOAT            = 6,
        TYPE_FLOAT_VEC4       = 7,
        TYPE_FLOAT_MAT4       = 8,
        TYPE_SAMPLER_2D       = 9,
        TYPE_SAMPLER_CUBE     = 10,
        TYPE_SAMPLER_2D_ARRAY = 11,
        TYPE_FLOAT_VEC2       = 12,
        TYPE_FLOAT_VEC3       = 13,
        TYPE_FLOAT_MAT2       = 14,
        TYPE_FLOAT_MAT3       = 15,
        TYPE_IMAGE_2D         = 16,
    };

    /*#
     * Blend factor
     * @enum
     * @name BlendFactor
     * @member BLEND_FACTOR_ZERO
     * @member BLEND_FACTOR_ONE
     * @member BLEND_FACTOR_SRC_COLOR
     * @member BLEND_FACTOR_ONE_MINUS_SRC_COLOR
     * @member BLEND_FACTOR_DST_COLOR
     * @member BLEND_FACTOR_ONE_MINUS_DST_COLOR
     * @member BLEND_FACTOR_SRC_ALPHA
     * @member BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
     * @member BLEND_FACTOR_DST_ALPHA
     * @member BLEND_FACTOR_ONE_MINUS_DST_ALPHA
     * @member BLEND_FACTOR_SRC_ALPHA_SATURATE
     * @member BLEND_FACTOR_CONSTANT_COLOR
     * @member BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
     * @member BLEND_FACTOR_CONSTANT_ALPHA
     * @member BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
     */
    enum BlendFactor
    {
        BLEND_FACTOR_ZERO                     = 0,
        BLEND_FACTOR_ONE                      = 1,
        BLEND_FACTOR_SRC_COLOR                = 2,
        BLEND_FACTOR_ONE_MINUS_SRC_COLOR      = 3,
        BLEND_FACTOR_DST_COLOR                = 4,
        BLEND_FACTOR_ONE_MINUS_DST_COLOR      = 5,
        BLEND_FACTOR_SRC_ALPHA                = 6,
        BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      = 7,
        BLEND_FACTOR_DST_ALPHA                = 8,
        BLEND_FACTOR_ONE_MINUS_DST_ALPHA      = 9,
        BLEND_FACTOR_SRC_ALPHA_SATURATE       = 10,
        BLEND_FACTOR_CONSTANT_COLOR           = 11,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 12,
        BLEND_FACTOR_CONSTANT_ALPHA           = 13,
        BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 14,
    };

    /*#
     * Vertex step function. Dictates how the data for a vertex attribute should be read in a vertex shader.
     * @enum
     * @name VertexStepFunction
     * @member VERTEX_STEP_FUNCTION_VERTEX
     * @member VERTEX_STEP_FUNCTION_INSTANCE
     */
    enum VertexStepFunction
    {
        VERTEX_STEP_FUNCTION_VERTEX,
        VERTEX_STEP_FUNCTION_INSTANCE,
    };

    /*#
     * Create new vertex stream declaration. A stream declaration contains a list of vertex streams
     * that should be used to create a vertex declaration from.
     * @name NewVertexStreamDeclaration
     * @param context [type: dmGraphics::HContext] the context
     * @return declaration [type: dmGraphics::HVertexStreamDeclaration] the vertex declaration
     */
    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context);

    /*#
     * Adds a stream to a stream declaration
     * @name AddVertexStream
     * @param context [type: dmGraphics::HContext] the context
     * @param name [type: const char*] the name of the stream
     * @param size [type: uint32_t] the size of the stream, i.e number of components
     * @param type [type: dmGraphics::Type] the data type of the stream
     * @param normalize [type: bool] true if the stream should be normalized in the 0..1 range
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, const char* name, uint32_t size, Type type, bool normalize);

    /*#
     * Adds a stream to a stream declaration
     * @name AddVertexStream
     * @param context [type: dmGraphics::HContext] the context
     * @param name_hash [type: uint64_t] the name hash of the stream
     * @param size [type: uint32_t] the size of the stream, i.e number of components
     * @param type [type: dmGraphics::Type] the data type of the stream
     * @param normalize [type: bool] true if the stream should be normalized in the 0..1 range
     */
    void AddVertexStream(HVertexStreamDeclaration stream_declaration, uint64_t name_hash, uint32_t size, Type type, bool normalize);

    /*#
     * Delete vertex stream declaration
     * @name DeleteVertexStreamDeclaration
     * @param stream_declaration [type: dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     */
    void DeleteVertexStreamDeclaration(HVertexStreamDeclaration stream_declaration);

    /*#
     * Create new vertex declaration from a vertex stream declaration
     * @name NewVertexDeclaration
     * @param context [type: dmGraphics::HContext] the context
     * @param stream_declaration [type: dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     * @return declaration [type: dmGraphics::HVertexDeclaration] the vertex declaration
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration);

    /*#
     * Create new vertex declaration from a vertex stream declaration and an explicit stride value,
     * where the stride is the number of bytes between each consecutive vertex in a vertex buffer
     * @name NewVertexDeclaration
     * @param context [type: dmGraphics::HContext] the context
     * @param stream_declaration [type: dmGraphics::HVertexStreamDeclaration] the vertex stream declaration
     * @param stride [type: uint32_t] the stride between the start of each vertex (in bytes)
     * @return declaration [type: dmGraphics::HVertexDeclaration] the vertex declaration
     */
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride);

    /*#
     * Delete vertex declaration
     * @name DeleteVertexDeclaration
     * @param vertex_declaration [type: dmGraphics::HVertexDeclaration] the vertex declaration
     */
    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration);

    /*#
     * Get the physical offset into the vertex data for a particular stream
     * @name GetVertexStreamOffset
     * @param vertex_declaration [type: dmGraphics::HVertexDeclaration] the vertex declaration
     * @param name_hash [type: uint64_t] the name hash of the vertex stream (as passed into AddVertexStream())
     * @return Offset in bytes into the vertex or INVALID_STREAM_OFFSET if not found
     */
    uint32_t GetVertexStreamOffset(HVertexDeclaration vertex_declaration, uint64_t name_hash);

    /*#
     * Create new vertex buffer with initial data
     * @name NewVertexBuffer
     * @param context [type: dmGraphics::HContext] the context
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     * @param buffer_usage [type: dmGraphics::BufferUsage] the usage
     * @return buffer [type: dmGraphics::HVertexBuffer] the vertex buffer
     */
    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Delete vertex buffer
     * @name DeleteVertexBuffer
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer
     */
    void DeleteVertexBuffer(HVertexBuffer buffer);

    /*#
     * Set vertex buffer data
     * @name SetVertexBufferData
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     * @param buffer_usage [type: dmGraphics::BufferUsage] the usage
     */
    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Set subset of vertex buffer data
     * @name SetVertexBufferSubData
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer
     * @param offset [type: uint32_t] the offset into the desination buffer (in bytes)
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     */
    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

    /*#
     * Get the max number of vertices allowed by the system in a vertex buffer
     * @name GetMaxElementsVertices
     * @param context [type: dmGraphics::HContext] the context
     * @return count [type: uint32_t] the count
     */
    uint32_t GetMaxElementsVertices(HContext context);

    /*#
     * Create new index buffer with initial data
     * @note The caller need to track if the indices are 16 or 32 bit.
     * @name NewIndexBuffer
     * @param context [type: dmGraphics::HContext] the context
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     * @param buffer_usage [type: dmGraphics::BufferUsage] the usage
     * @return buffer [type: dmGraphics::HIndexBuffer] the index buffer
     */
    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Delete the index buffer
     * @name DeleteIndexBuffer
     * @param buffer [type: dmGraphics::HIndexBuffer] the index buffer
     */
    void DeleteIndexBuffer(HIndexBuffer buffer);

    /*#
     * Set index buffer data
     * @name SetIndexBufferData
     * @param buffer [type: dmGraphics::HIndexBuffer] the buffer
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     * @param buffer_usage [type: dmGraphics::BufferUsage] the usage
     */
    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage);

    /*#
     * Set subset of index buffer data
     * @name SetIndexBufferSubData
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer
     * @param offset [type: uint32_t] the offset into the desination buffer (in bytes)
     * @param size [type: uint32_t] the size of the buffer (in bytes). May be 0
     * @param data [type: void*] the data
     */
    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data);

    /*#
     * Check if the index format is supported
     * @name IsIndexBufferFormatSupported
     * @param context [type: dmGraphics::HContext] the context
     * @param format [type: dmGraphics::IndexBufferFormat] the format
     * @param result [type: bool] true if the format is supoprted
     */
    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format);

    /*#
     * Get the max number of indices allowed by the system in an index buffer
     * @name GetMaxElementsIndices
     * @param context [type: dmGraphics::HContext] the context
     * @return count [type: uint32_t] the count
     */
    uint32_t GetMaxElementsIndices(HContext context);

    /*# check if an extension is supported
     * @name IsExtensionSupported
     * @param context [type:dmGraphics::HContext] the context
     * @param extension [type:const char*] the extension.
     * @return result [type:bool] true if the extension was supported
     */
    bool IsExtensionSupported(HContext context, const char* extension);

    /*# check if a specific texture format is supported
     * @name IsTextureFormatSupported
     * @param context [type:dmGraphics::HContext] the context
     * @param format [type:TextureFormat] the texture format.
     * @return result [type:bool] true if the texture format was supported
     */
    bool IsTextureFormatSupported(HContext context, TextureFormat format);

    /*#
     * @name GetNumSupportedExtensions
     * @param context [type:dmGraphics::HContext] the context
     * @return count [type:uint32_t] the number of supported extensions
     */
    uint32_t GetNumSupportedExtensions(HContext context);

    /*# get the supported extension
     * @name GetSupportedExtension
     * @param context [type:dmGraphics::HContext] the context
     * @param index [type:uint32_t] the index of the extension
     * @return extension [type:const char*] the extension. 0 if index was out of bounds
     */
    const char* GetSupportedExtension(HContext context, uint32_t index);
}

#endif // DMSDK_GRAPHICS_H
