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

#ifndef DMSDK_GRAPHICS_VULKAN_H
#define DMSDK_GRAPHICS_VULKAN_H

#include <dmsdk/graphics/graphics.h>

/*# Graphics API documentation
 * [file:<dmsdk/graphics/graphics_vulkan.h>]
 *
 * Graphics Vulkan API
 *
 * @document
 * @name Graphics Vulkan
 * @namespace dmGraphics
 */

namespace dmGraphics
{
    /*#
     * Max subpasses
     * @constant
     * @name INVALID_STREAM_OFFSET
     */
	static const uint32_t MAX_SUBPASSES = 4;

    /*#
     * Max subpass dependencies
     * @constant
     * @name MAX_SUBPASS_DEPENDENCIES
     */
    static const uint32_t MAX_SUBPASS_DEPENDENCIES  = 4;

    /*#
     * Subpass external
     * @constant
     * @name SUBPASS_EXTERNAL
     */
    static const uint8_t  SUBPASS_EXTERNAL = -1;

    /*#
     * Subpass attachment unused flag
     * @constant
     * @name SUBPASS_ATTACHMENT_UNUSED
     */
    static const uint8_t  SUBPASS_ATTACHMENT_UNUSED = -1;

    /*#
     * @enum
     * @name BarrierStageFlags
     * @member STAGE_FLAG_QUEUE_BEGIN
     * @member STAGE_FLAG_QUEUE_END
     * @member STAGE_FLAG_FRAGMENT_SHADER
     * @member STAGE_FLAG_EARLY_FRAGMENT_SHADER_TEST
     */
    enum BarrierStageFlags
    {
        STAGE_FLAG_QUEUE_BEGIN                = 1,
        STAGE_FLAG_QUEUE_END                  = 2,
        STAGE_FLAG_FRAGMENT_SHADER            = 4,
        STAGE_FLAG_EARLY_FRAGMENT_SHADER_TEST = 8,
    };

    /*#
     * @enum
     * @name BarrierAccessFlags
     * @member ACCESS_FLAG_READ
     * @member ACCESS_FLAG_WRITE
     * @member ACCESS_FLAG_SHADER
     */
    enum BarrierAccessFlags
    {
        ACCESS_FLAG_READ   = 1,
        ACCESS_FLAG_WRITE  = 2,
        ACCESS_FLAG_SHADER = 4,
    };

    struct RenderPassDescriptor
    {
        uint8_t* m_ColorAttachmentIndices;
        uint8_t  m_ColorAttachmentIndicesCount;
        uint8_t* m_DepthStencilAttachmentIndex;

        uint8_t* m_InputAttachmentIndices;
        uint8_t  m_InputAttachmentIndicesCount;
    };

    struct RenderPassDependency
    {
        uint8_t m_Src;
        uint8_t m_Dst;
    };

    struct CreateRenderPassParams
    {
        RenderPassDescriptor m_SubPasses[MAX_SUBPASSES];
        RenderPassDependency m_Dependencies[MAX_SUBPASS_DEPENDENCIES];
        uint8_t              m_SubPassCount;
        uint8_t              m_DependencyCount;
    };

    struct SetRenderTargetAttachmentsParams
    {
        HTexture     m_ColorAttachments[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp m_ColorAttachmentLoadOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        AttachmentOp m_ColorAttachmentStoreOps[MAX_BUFFER_COLOR_ATTACHMENTS];
        float        m_ColorAttachmentClearValues[MAX_BUFFER_COLOR_ATTACHMENTS][4];
        uint32_t     m_ColorAttachmentsCount;
        uint32_t     m_Width;
        uint32_t     m_Height;
        bool         m_SetDimensions;
    };

    /*#
     * Get the vulkan context of the installed adapter
     * @name VulkanGetContext
     * @return context [type: dmGraphics::HContext] the vulkan context
     */
    HContext VulkanGetContext();

	/*#
     * Copy a buffer to a texture
     * @name VulkanCopyBufferToTexture
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer to copy into the texture
     * @param texture [type: dmGraphics::HTexture] the texture to copy into
     * @param width [type: uint32_t] width of the texture slice to copy
     * @param height [type: uint32_t] height of the texture slice to copy
     * @param x [type: uint32_t] x offset into the texture
     * @param y [type: uint32_t] y offset into the texture
     * @param mipmap [type: uint32_t] the mipmap slice to upload
     */
    void VulkanCopyBufferToTexture(HContext context, HVertexBuffer buffer, HTexture texture, uint32_t width, uint32_t height);
    /*#
     * Sets the attachments for a render target
     * @name VulkanSetRenderTargetAttachments
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param render_target [type: dmGraphics::HRenderTarget] the render target
     * @param params [type: dmGraphics::SetRenderTargetAttachmentsParams] the render target attachment params
     */
    void VulkanSetRenderTargetAttachments(HContext context, HRenderTarget render_target, const SetRenderTargetAttachmentsParams& params);
    /*#
     * Bind a constant buffer to the currently bound shader
     * @name VulkanSetConstantBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HVertexBuffer] the constant buffer
     * @param buffer_offset [type: uint32_t] offset location for the buffer
     * @param base_location [type: dmGraphics::HUniformLocation] the shader location to bind to
     */
    void VulkanSetConstantBuffer(HContext context, HVertexBuffer buffer, uint32_t buffer_offset, HUniformLocation base_location);
    /*#
     * Get the current swap chain texture
     * @name VulkanGetActiveSwapChainTexture
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @return swapchain [type: dmGraphics::HTexture] the swap chain texture for the current frame
     */
    HTexture VulkanGetActiveSwapChainTexture(HContext context);
    /*#
     * Draw with index buffer instanced
     * @name VulkanDrawElementsInstanced
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param prim_type [type: dmGraphics::PrimitiveType] primitive type
     * @param first [type: uint32_t] the byte offset into the index buffer
     * @param count [type: uint32_t] number of primitives to draw
     * @param instance_count [type: uint32_t] number of instances to draw
     * @param type [type: dmGraphics::Type] the index buffer type
     * @param index_buffer [type: dmGraphics::HIndexBuffer] the index buffer
     */
    void VulkanDrawElementsInstanced(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance, Type type, HIndexBuffer index_buffer);
    /*#
     * Set the step function for a vertex declaration
     * @name VulkanSetVertexDeclarationStepFunction
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param vertex_declaration [type: dmGraphics::HVertexDeclaration] the vertex declaration to set step function for
     * @param step_function [type: dmGraphics::VertexStepFunction] the step function
     */
    void VulkanSetVertexDeclarationStepFunction(HContext context, HVertexDeclaration vertex_declaration, VertexStepFunction step_function);
    /*#
     * Draw with vertex buffer instanced
     * @name VulkanDrawBaseInstance
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param prim_type [type: dmGraphics::PrimitiveType] primitive type
     * @param first [type: uint32_t] the byte offset into the index buffer
     * @param count [type: uint32_t] number of primitives to draw
     * @param instance_count [type: uint32_t] number of instances to draw
     * @param type [type: dmGraphics::Type] the index buffer type
     * @param index_buffer [type: dmGraphics::HIndexBuffer] the index buffer
     */
    void VulkanDrawBaseInstance(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance);
    /*#
     * Create a render pass
     * @name VulkanCreateRenderPass
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param render_target [type: dmGraphics::HRenderTarget] the render target
     * @param params [type: dmGraphics::CreateRenderPassParams] params
     */
    void VulkanCreateRenderPass(HContext context, HRenderTarget render_target, const CreateRenderPassParams& params);
    /*#
     * Advance the render pass to the next sub pass
     * @name VulkanNextRenderPass
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param render_target [type: dmGraphics::HRenderTarget] the render target to advance
     */
    void VulkanNextRenderPass(HContext context, HRenderTarget render_target);
    /*#
     * Set the number of in-flight frames that the vulkan layer uses
     * @name VulkanSetFrameInFlightCount
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param num_frames_in_flight [type: uint8_t] the max number of frames in flight
     */
    void VulkanSetFrameInFlightCount(HContext context, uint8_t num_frames_in_flight);
    /*#
     * Set the entire current pipeline state
     * @name VulkanSetPipelineState
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param ps [type: dmGraphics::PipelineState] the pipeline state
     */
    void VulkanSetPipelineState(HContext context, HPipelineState ps);
    /*#
     * Clear a texture with RGBA values
     * @name VulkanClearTexture
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param texture [type: dmGraphics::HTexture] the texture to clear
     * @param values [type: float[4]] the clear values
     */
    void VulkanClearTexture(HContext context, HTexture texture, float values[4]);
    /*#
     * Insert a memory barrier
     * @name VulkanMemorybarrier
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param texture [type: dmGraphics::HTexture] the texture to set barrier for
     * @param src_stage_flags [type: uint32_t] src stage flag
     * @param dst_stage_flags [type: uint32_t] dst stage flag
     * @param src_access_flags [type: uint32_t] src access flag
     * @param dst_access_flags [type: uint32_t] dst access flag
     */
    void VulkanMemorybarrier(HContext context, HTexture texture, uint32_t src_stage_flags, uint32_t dst_stage_flags, uint32_t src_access_flags, uint32_t dst_access_flags);
    /*#
     * Get the set and binding information from a uniform index
     * @name VulkanGetUniformBinding
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param program [type: dmGraphics::HProgram] the program to query
     * @param index [type: uint32_t] uniform index
     * @param set [type: uint32_t*] a pointer to store the result of the set number
     * @param binding [type: uint32_t*] a pointer to store the result of the binding number
     * @param member_index [type: uint32_t*] a pointer to store the result of the member index
     */
    void VulkanGetUniformBinding(HContext context, HProgram program, uint32_t index, uint32_t* set, uint32_t* binding, uint32_t* member_index);
    /*#
     * Create a new storage buffer
     * @name HStorageBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer_size [type: uint32_t] the size of the storage buffer to allocate
     * @return storage_buffer [type: dmGraphics::HStorageBuffer] the storage buffer
     */
    HStorageBuffer VulkanNewStorageBuffer(HContext context, uint32_t buffer_size);
    /*#
     * Delete a storage buffer
     * @name VulkanDeleteStorageBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param storage_buffer [type: dmGraphics::HStorageBuffer] the attachment to get
     */
    void VulkanDeleteStorageBuffer(HContext context, HStorageBuffer storage_buffer);
    /*#
     * Bind a storage buffer to the render state
     * @name VulkanSetStorageBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param storage_buffer [type: dmGraphics::HStorageBuffer] the storage buffer to bind
     * @param binding_index [type: uint32_t] binding index
     * @param buffer_offset [type: uint32_t] the buffer offset
     * @param base_location [type: dmGraphics::HUniformLocation] the shader location to bind to
     */
    void VulkanSetStorageBuffer(HContext context, HStorageBuffer storage_buffer, uint32_t binding_index, uint32_t buffer_offset, HUniformLocation base_location);
    /*#
     * Set the storage buffer data
     * @name VulkanSetStorageBufferData
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param storage_buffer [type: dmGraphics::HStorageBuffer] the storage buffer
     * @param size [type: uint32_t] size of the input data
     * @param data [type: void*] the buffer data
     */
    void VulkanSetStorageBufferData(HContext context, HStorageBuffer storage_buffer, uint32_t size, const void* data);
    /*#
     * Map the GPU data for a vertex buffer
     * @name VulkanMapVertexBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer to map
     * @param buffer_access [type: dmGraphics::BufferAccess] how to access the buffer
     * @return void* [type: void*] the mapped data buffer
     */
    void* VulkanMapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access);
    /*#
     * Unmap a vertex buffer
     * @name VulkanUnmapVertexBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HVertexBuffer] the buffer
     * @return flag [type: bool] unused
     */
    bool VulkanUnmapVertexBuffer(HContext context, HVertexBuffer buffer);
    /*#
     * Map the GPU data for a index buffer
     * @name VulkanMapIndexBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HIndexBuffer] the buffer to map
     * @param buffer_access [type: dmGraphics::BufferAccess] how to access the buffer
     * @return void* [type: void*] the mapped data buffer
     */
    void* VulkanMapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access);
    /*#
     * Unmap a index buffer
     * @name VulkanUnmapIndexBuffer
     * @param context [type: dmGraphics::HContext] the vulkan context
     * @param buffer [type: dmGraphics::HIndexBuffer] the buffer
     * @return flag [type: bool] unused
     */
    bool VulkanUnmapIndexBuffer(HContext context, HIndexBuffer buffer);
}

#endif // DMSDK_GRAPHICS_VULKAN_H
