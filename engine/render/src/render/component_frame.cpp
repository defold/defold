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

#include "component_frame.h"
#include "render_private.h"
#include "font/font_renderer.h"
#include <graphics/graphics.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <assert.h>

namespace dmRender
{
    struct ComponentPass
    {
        dmVMath::Matrix4 m_View, m_Projection;
        dmVMath::Vector3 m_Ambient;
        HMaterial m_Material;
        HNamedConstantBuffer m_Constants;
        uint32_t m_ObjectBegin, m_ObjectCount, m_UploadEnd;
        uint32_t m_TextureBegin, m_TextureCount, m_LightBegin, m_LightCount, m_LightCapacity;
    };

    struct ComponentFrame
    {
        enum { MAX_COMMANDS = 256, MAX_PASSES = 64, MAX_OBJECTS = 16384,
               MAX_BINDINGS = 4096, MAX_LIGHTS = 4096, MAX_CONSTANT_BYTES = 4 * 1024 * 1024 };
        dmArray<Command> m_Commands;
        dmArray<ComponentPass> m_Passes;
        dmArray<RenderObject> m_Objects;
        dmArray<TextureBinding> m_Textures;
        dmArray<LightSTD140> m_Lights;
        dmArray<HNamedConstantBuffer> m_Constants;
        dmGraphics::VertexUploadBatch m_Uploads;
        uint32_t m_ConstantCursor;
        uint64_t m_ConstantCapacity;
        float m_Time, m_Dt;
        bool m_Failed;
        ComponentFrame() : m_ConstantCursor(0), m_ConstantCapacity(0), m_Time(0), m_Dt(0), m_Failed(false) {}
        ~ComponentFrame()
        {
            for (uint32_t i = 0; i < m_Constants.Size(); ++i) DeleteNamedConstantBuffer(m_Constants[i]);
        }
    };

    template <typename T> static bool Reserve(dmArray<T>& array, uint32_t count, uint32_t limit)
    {
        if (count > limit) return false;
        if (count > array.Capacity())
            array.SetCapacity(dmMath::Min(limit, dmMath::Max(count, dmMath::Max<uint32_t>(16, array.Capacity() * 2))));
        return true;
    }

    HComponentFrame NewComponentFrame() { return new ComponentFrame; }
    void DeleteComponentFrame(HComponentFrame frame) { delete frame; }
    void EnableComponentFrames(HRenderContext context, bool enabled) { context->m_ComponentFramesEnabled = enabled; }
    bool AreComponentFramesEnabled(HRenderContext context) { return context->m_ComponentFramesEnabled; }

    static void AppendCommand(ComponentFrame* frame, const Command& command)
    {
        if (!Reserve(frame->m_Commands, frame->m_Commands.Size() + 1, ComponentFrame::MAX_COMMANDS))
            frame->m_Failed = true;
        else frame->m_Commands.Push(command);
    }

    static HNamedConstantBuffer CopyConstants(ComponentFrame* frame, HNamedConstantBuffer source)
    {
        if (!source) return 0;
        // Include retained storage plus a conservative replacement allocation.
        uint64_t incoming = GetNamedConstantBufferCapacity(source) * 2 + GetNamedConstantBufferCapacity(0);
        if (frame->m_ConstantCapacity + incoming > ComponentFrame::MAX_CONSTANT_BYTES ||
            frame->m_ConstantCursor == ComponentFrame::MAX_OBJECTS + ComponentFrame::MAX_PASSES)
        { frame->m_Failed = true; return 0; }
        if (frame->m_ConstantCursor == frame->m_Constants.Size())
        {
            Reserve(frame->m_Constants, frame->m_Constants.Size() + 1, ComponentFrame::MAX_OBJECTS + ComponentFrame::MAX_PASSES);
            HNamedConstantBuffer buffer = NewNamedConstantBuffer();
            frame->m_Constants.Push(buffer);
            frame->m_ConstantCapacity += GetNamedConstantBufferCapacity(buffer);
        }
        HNamedConstantBuffer buffer = frame->m_Constants[frame->m_ConstantCursor++];
        frame->m_ConstantCapacity -= GetNamedConstantBufferCapacity(buffer);
        CopyNamedConstantBuffer(buffer, source);
        frame->m_ConstantCapacity += GetNamedConstantBufferCapacity(buffer);
        return buffer;
    }

    void BeginComponentFrameCapture(HRenderContext context, HComponentFrame frame)
    {
        assert(!context->m_ComponentFrameCapture);
        frame->m_Commands.SetSize(0); frame->m_Passes.SetSize(0); frame->m_Objects.SetSize(0);
        frame->m_Textures.SetSize(0); frame->m_Lights.SetSize(0);
        frame->m_ConstantCursor = 0;
        frame->m_Failed = false;
        frame->m_Time = context->m_Time; frame->m_Dt = context->m_Dt;
        context->m_ComponentFrameCapture = frame;
        int32_t x, y; uint32_t w, h;
        dmGraphics::GetViewport(context->m_GraphicsContext, &x, &y, &w, &h);
        AppendCommand(frame, Command(COMMAND_TYPE_SET_VIEWPORT, x, y, w, h));
        dmGraphics::BeginVertexUploadCapture(&frame->m_Uploads, true);
    }

    bool EndComponentFrameCapture(HRenderContext context)
    {
        ComponentFrame* frame = context->m_ComponentFrameCapture;
        assert(frame);
        if (!dmGraphics::EndVertexUploadCapture()) frame->m_Failed = true;
        context->m_ComponentFrameCapture = 0;
        // Text layout references and live RenderObject pointers stay on producer.
        ClearTextEntries(context);
        ClearRenderObjects(context);
        if (frame->m_Failed) dmLogError("Component frame capture rejected: unsupported command or frame capacity exceeded");
        return !frame->m_Failed;
    }

    Result CaptureComponentDraw(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constants)
    {
        ComponentFrame* frame = context->m_ComponentFrameCapture;
        if (frame->m_Failed) return RESULT_OUT_OF_RESOURCES;
        const LightSTD140* lights; uint32_t light_count; dmVMath::Vector3 ambient;
        GetComponentFrameLights(context, &lights, &light_count, &ambient);
        if (!Reserve(frame->m_Passes, frame->m_Passes.Size() + 1, ComponentFrame::MAX_PASSES) ||
            !Reserve(frame->m_Objects, frame->m_Objects.Size() + context->m_RenderObjects.Size(), ComponentFrame::MAX_OBJECTS) ||
            !Reserve(frame->m_Textures, frame->m_Textures.Size() + context->m_TextureBindTable.Size(), ComponentFrame::MAX_BINDINGS) ||
            !Reserve(frame->m_Lights, frame->m_Lights.Size() + light_count, ComponentFrame::MAX_LIGHTS))
        { frame->m_Failed = true; return RESULT_OUT_OF_RESOURCES; }
        ComponentPass pass;
        pass.m_View = context->m_View; pass.m_Projection = context->m_Projection;
        pass.m_Material = context->m_Material; pass.m_Constants = CopyConstants(frame, constants);
        pass.m_ObjectBegin = frame->m_Objects.Size();
        for (uint32_t i = 0; i < context->m_RenderObjects.Size(); ++i)
        {
            RenderObject object = *context->m_RenderObjects[i];
            if (!object.m_VertexCount) continue;
            if (predicate)
            {
                MaterialTagList tags;
                GetMaterialTagList(context, GetMaterialTagListKey(object.m_Material), &tags);
                if (!MatchMaterialTags(tags.m_Count, tags.m_Tags, predicate->m_TagCount, predicate->m_Tags)) continue;
            }
            object.m_ConstantBuffer = CopyConstants(frame, object.m_ConstantBuffer);
            frame->m_Objects.Push(object);
        }
        pass.m_ObjectCount = frame->m_Objects.Size() - pass.m_ObjectBegin;
        pass.m_UploadEnd = frame->m_Uploads.m_Uploads.Size();
        pass.m_TextureBegin = frame->m_Textures.Size(); pass.m_TextureCount = context->m_TextureBindTable.Size();
        frame->m_Textures.PushArray(context->m_TextureBindTable.Begin(), pass.m_TextureCount);
        pass.m_LightCapacity = context->m_MaxLightCount;
        pass.m_LightBegin = frame->m_Lights.Size(); pass.m_LightCount = light_count; pass.m_Ambient = ambient;
        frame->m_Lights.PushArray(lights, light_count);
        AppendCommand(frame, Command(COMMAND_TYPE_DRAW, frame->m_Passes.Size()));
        frame->m_Passes.Push(pass);
        dmGraphics::ProtectCapturedGraphicsResources();
        return frame->m_Failed ? RESULT_OUT_OF_RESOURCES : RESULT_OK;
    }

    bool CaptureComponentCommands(HRenderContext context, Command* commands, uint32_t count, bool release)
    {
        ComponentFrame* frame = context->m_ComponentFrameCapture;
        for (uint32_t i = 0; i < count && !frame->m_Failed; ++i)
        {
            const Command& c = commands[i];
            switch (c.m_Type)
            {
                case COMMAND_TYPE_SET_VIEW: SetViewMatrix(context, *(dmVMath::Matrix4*)c.m_Operands[0]); break;
                case COMMAND_TYPE_SET_PROJECTION: SetProjectionMatrix(context, *(dmVMath::Matrix4*)c.m_Operands[0]); break;
                case COMMAND_TYPE_SET_RENDER_CAMERA:
                    context->m_CurrentRenderCamera = (HRenderCamera)c.m_Operands[0];
                    context->m_CurrentRenderCameraUseFrustum = c.m_Operands[1]; break;
                case COMMAND_TYPE_ENABLE_MATERIAL: context->m_Material = (HMaterial)c.m_Operands[0]; break;
                case COMMAND_TYPE_DISABLE_MATERIAL: context->m_Material = 0; break;
                case COMMAND_TYPE_ENABLE_TEXTURE:
                case COMMAND_TYPE_DISABLE_TEXTURE:
                {
                    dmGraphics::HTexture texture = c.m_Type == COMMAND_TYPE_ENABLE_TEXTURE ? c.m_Operands[2] : 0;
                    if (c.m_Operands[0]) SetTextureBindingByHash(context, c.m_Operands[0], texture);
                    else SetTextureBindingByUnit(context, c.m_Operands[1], texture);
                    break;
                }
                case COMMAND_TYPE_DRAW:
                    DrawRenderList(context, (HPredicate)c.m_Operands[0], (HNamedConstantBuffer)c.m_Operands[1],
                        (FrustumOptions*)c.m_Operands[2], (SortOrder)c.m_Operands[3]); break;
                case COMMAND_TYPE_DRAW_DEBUG3D: DrawDebug3d(context, (FrustumOptions*)c.m_Operands[0]); break;
                case COMMAND_TYPE_SET_VIEWPORT:
                    // Only viewport state is needed during geometry preparation
                    // (font SDF scale). Clears and draws are never executed here.
                    dmGraphics::SetViewport(context->m_GraphicsContext, c.m_Operands[0], c.m_Operands[1], c.m_Operands[2], c.m_Operands[3]);
                    AppendCommand(frame, c); break;
                case COMMAND_TYPE_SET_COMPUTE:
                case COMMAND_TYPE_DISPATCH_COMPUTE:
                case COMMAND_TYPE_MAX:
                    frame->m_Failed = true; break;
                case COMMAND_TYPE_SET_RENDER_TARGET:
                    if (c.m_Operands[0]) dmGraphics::ProtectCapturedGraphicsResources();
                    AppendCommand(frame, c); break;
                case COMMAND_TYPE_ENABLE_STATE:
                case COMMAND_TYPE_DISABLE_STATE:
                case COMMAND_TYPE_CLEAR:
                case COMMAND_TYPE_SET_BLEND_FUNC:
                case COMMAND_TYPE_SET_BLEND_FUNC_SEPARATE:
                case COMMAND_TYPE_SET_BLEND_EQUATION_SEPARATE:
                case COMMAND_TYPE_SET_COLOR_MASK:
                case COMMAND_TYPE_SET_DEPTH_MASK:
                case COMMAND_TYPE_SET_DEPTH_FUNC:
                case COMMAND_TYPE_SET_STENCIL_MASK:
                case COMMAND_TYPE_SET_STENCIL_FUNC:
                case COMMAND_TYPE_SET_STENCIL_OP:
                case COMMAND_TYPE_SET_CULL_FACE:
                case COMMAND_TYPE_SET_POLYGON_OFFSET:
                    AppendCommand(frame, c); break; // Pointer-free pass state.
                default: frame->m_Failed = true; break;
            }
        }
        if (release) ReleaseCommandOperands(commands, count);
        return !frame->m_Failed;
    }

    HRenderContext NewComponentFrameConsumer(HRenderContext source)
    {
        // No Lua world, callbacks, text cache, camera registry or message socket.
        RenderContext* context = new RenderContext;
        context->m_GraphicsContext = source->m_GraphicsContext;
        context->m_UseAdjustedNDC = source->m_UseAdjustedNDC;
        context->m_View = dmVMath::Matrix4::identity();
        context->m_Projection = dmVMath::Matrix4::identity();
        context->m_ComponentFrameCapture = 0;
        context->m_ComponentFramesEnabled = false;
        context->m_UseCapturedLightBuffer = true;
        context->m_Material = 0;
        context->m_StencilBufferCleared = 0;
        context->m_LightUniformBuffer = 0;
        SetLightBufferCount(context, 0);
        return context;
    }

    void DeleteComponentFrameConsumer(HRenderContext context)
    {
        FinalizeLightData(context);
        delete context;
    }

    void ConsumeComponentFrame(HRenderContext context, HComponentFrame frame)
    {
        assert(!frame->m_Failed && !context->m_ComponentFrameCapture);
        BeginFrame(context, frame->m_Time, frame->m_Dt);
        uint32_t upload_begin = 0;
        for (uint32_t i = 0; i < frame->m_Commands.Size(); ++i)
        {
            Command& command = frame->m_Commands[i];
            if (command.m_Type != COMMAND_TYPE_DRAW)
            { ParseCommands(context, &command, 1, false); continue; }
            const ComponentPass& pass = frame->m_Passes[command.m_Operands[0]];
            dmGraphics::ReplayVertexUploads(&frame->m_Uploads, upload_begin, pass.m_UploadEnd);
            upload_begin = pass.m_UploadEnd;
            SetViewMatrix(context, pass.m_View); SetProjectionMatrix(context, pass.m_Projection);
            context->m_Material = pass.m_Material;
            context->m_TextureBindTable.SetSize(0);
            Reserve(context->m_TextureBindTable, pass.m_TextureCount, ComponentFrame::MAX_BINDINGS);
            context->m_TextureBindTable.PushArray(frame->m_Textures.Begin() + pass.m_TextureBegin, pass.m_TextureCount);
            // Shader blocks require the configured array capacity, even when
            // this frame submits fewer (or no) active light instances.
            if (context->m_MaxLightCount < pass.m_LightCapacity) SetLightBufferCount(context, pass.m_LightCapacity);
            context->m_LightBufferUploadScratch.SetSize(0);
            Reserve(context->m_LightBufferUploadScratch, pass.m_LightCount, ComponentFrame::MAX_LIGHTS);
            context->m_LightBufferUploadScratch.PushArray(frame->m_Lights.Begin() + pass.m_LightBegin, pass.m_LightCount);
            context->m_AmbientLight = pass.m_Ambient;
            context->m_LightBufferDirty = 1;
            context->m_RenderObjects.SetSize(0);
            Reserve(context->m_RenderObjects, pass.m_ObjectCount, ComponentFrame::MAX_OBJECTS);
            for (uint32_t j = 0; j < pass.m_ObjectCount; ++j)
                context->m_RenderObjects.Push(&frame->m_Objects[pass.m_ObjectBegin + j]);
            Draw(context, 0, pass.m_Constants);
        }
        dmGraphics::ReplayVertexUploads(&frame->m_Uploads, upload_begin, frame->m_Uploads.m_Uploads.Size());
        context->m_RenderObjects.SetSize(0);
    }

    uint64_t GetComponentFrameCapacity(HComponentFrame f)
    {
        return sizeof(*f) + f->m_Commands.Capacity() * sizeof(Command) + f->m_Passes.Capacity() * sizeof(ComponentPass) +
            f->m_Objects.Capacity() * sizeof(RenderObject) + f->m_Textures.Capacity() * sizeof(TextureBinding) +
            f->m_Lights.Capacity() * sizeof(LightSTD140) + f->m_Constants.Capacity() * sizeof(HNamedConstantBuffer) +
            f->m_ConstantCapacity + dmGraphics::GetVertexUploadCapacity(&f->m_Uploads) - sizeof(f->m_Uploads);
    }
    uint64_t GetComponentFrameUsedBytes(HComponentFrame f)
    {
        return sizeof(*f) + f->m_Commands.Size() * sizeof(Command) + f->m_Passes.Size() * sizeof(ComponentPass) +
            f->m_Objects.Size() * sizeof(RenderObject) + f->m_Textures.Size() * sizeof(TextureBinding) +
            f->m_Lights.Size() * sizeof(LightSTD140) + f->m_ConstantCapacity + f->m_Uploads.m_Data.Size() +
            f->m_Uploads.m_Uploads.Size() * sizeof(dmGraphics::VertexUpload);
    }
}
