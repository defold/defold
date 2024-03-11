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

#ifndef DM_GRAPHICS_VULKAN_H
#define DM_GRAPHICS_VULKAN_H

namespace dmGraphics
{
	static const uint32_t MAX_SUBPASSES             = 4;
    static const uint32_t MAX_SUBPASS_DEPENDENCIES  = 4;
    static const uint8_t  SUBPASS_EXTERNAL 		    = -1;
    static const uint8_t  SUBPASS_ATTACHMENT_UNUSED = -1;

    enum BarrierStageFlags
    {
        STAGE_FLAG_QUEUE_BEGIN                = 1,
        STAGE_FLAG_QUEUE_END                  = 2,
        STAGE_FLAG_FRAGMENT_SHADER            = 4,
        STAGE_FLAG_EARLY_FRAGMENT_SHADER_TEST = 8,
    };

    enum BarrierAccessFlags
    {
        ACCESS_FLAG_READ   = 1,
        ACCESS_FLAG_WRITE  = 2,
        ACCESS_FLAG_SHADER = 4,
    };

    struct RenderPassDependency
    {
        uint8_t m_Src;
        uint8_t m_Dst;
    };

    struct RenderPassDescriptor
    {
        uint8_t* m_ColorAttachmentIndices;
        uint8_t  m_ColorAttachmentIndicesCount;
        uint8_t* m_DepthStencilAttachmentIndex;

        uint8_t* m_InputAttachmentIndices;
        uint8_t  m_InputAttachmentIndicesCount;
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

    HContext VulkanGetContext();
	void     VulkanCopyBufferToTexture(HContext context, HVertexBuffer buffer, HTexture texture, const TextureParams& params);
    void     VulkanSetRenderTargetAttachments(HContext context, HRenderTarget render_target, const SetRenderTargetAttachmentsParams& params);
    void     VulkanSetConstantBuffer(HContext context, HVertexBuffer buffer, uint32_t buffer_offset, HUniformLocation base_location);
    HTexture VulkanGetActiveSwapChainTexture(HContext context);
    void     VulkanDrawElementsInstanced(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance, Type type, HIndexBuffer index_buffer);
    void     VulkanSetVertexDeclarationStepFunction(HContext context, HVertexDeclaration vertex_declaration, VertexStepFunction step_function);
    void     VulkanDrawBaseInstance(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count, uint32_t base_instance);
    void     VulkanCreateRenderPass(HContext context, HRenderTarget render_target, const CreateRenderPassParams& params);
    void     VulkanNextRenderPass(HContext context, HRenderTarget render_target);
    void     VulkanSetFrameInFlightCount(HContext, uint8_t num_frames_in_flight);
    void     VulkanSetPipelineState(HContext context, PipelineState ps);
    void     VulkanClearTexture(HContext context, HTexture texture, float values[4]);
    void     VulkanMemorybarrier(HContext context, HTexture _texture, uint32_t src_stage_flags, uint32_t dst_stage_flags, uint32_t src_access_flags, uint32_t dst_access_flags);
    void     VulkanGetUniformBinding(HContext context, HProgram program, uint32_t index, uint32_t* set, uint32_t* binding, uint32_t* member_index);
}

#endif
