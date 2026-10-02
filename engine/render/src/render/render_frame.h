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

#ifndef DM_RENDER_FRAME_H
#define DM_RENDER_FRAME_H

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <resource/resource.h>
#include "render.h"
#include "render_command.h"

namespace dmRender
{
    enum PocPipeline { POC_LEGACY, POC_COMPONENT, POC_GRAPHICS, POC_RENDERFRAME };
    struct PocCondition { PocPipeline m_Pipeline; bool m_Threaded; };
    bool ResolvePocCondition(const char* pipeline, const char* threaded, const char* sprite_alias,
                             const char* graphics_alias, PocCondition* result, const char** error);
    const char* GetPocConditionName(const PocCondition& condition);

    enum RenderFrameState { FRAME_FREE, FRAME_BUILDING, FRAME_READY, FRAME_READING, FRAME_RETIRED };
    enum { RENDER_FRAME_SCHEMA = 1, RENDER_FRAME_CONSUMERS = 16, RENDER_FRAME_LIMIT = 32 * 1024 * 1024 };
    // Bounds use squared radius. Payload offsets resolve only after sealing, so
    // arena growth cannot leave stale pointers in entries or consumer records.
    struct RenderFrameEntry
    {
        dmVMath::Vector4 m_Bounds;
        uint32_t m_Consumer;
        uint32_t m_Payload;
        uint32_t m_PayloadBytes;
        uint32_t m_BatchKey;
        uint32_t m_TagListKey;
        uint32_t m_Order;
        uint32_t m_MajorOrder;
    };
    struct RenderFrameDependency
    {
        void* m_Resource;
        uint32_t m_Generation;
    };
    struct RenderFrame;
    typedef void (*RenderFramePayloadRelease)(void* payload);
    struct RenderFramePayload
    {
        void* m_Data;
        RenderFramePayloadRelease m_Delete;
        uint64_t m_Capacity;
        uint64_t m_GrowthPeak;
    };
    // The owner contains allocation bookkeeping. Consumers receive only a const
    // view; factory operations and retirement always run on the producer.
    struct RenderFrame
    {
        RenderFrame();
        ~RenderFrame();
        dmArray<uint8_t> m_Data;
        dmArray<RenderFrameEntry> m_Entries;
        dmArray<RenderFrameDependency> m_Dependencies;
        dmHashTable64<uint32_t> m_DependencyMap;
        RenderFramePayload m_Payloads[RENDER_FRAME_CONSUMERS];
        CapturedCommands m_Commands;
        dmResource::HFactory m_Factory;
        uint64_t m_Id;
        uint64_t m_Tick;
        uint64_t m_GrowthPeak;
        uint32_t m_SurfaceGeneration;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Limit;
        uint32_t m_ConsumerMask;
        float m_Time;
        float m_Dt;
        RenderFrameState m_State;
        bool m_Overflow;
    private:
        RenderFrame(const RenderFrame&);
        RenderFrame& operator=(const RenderFrame&);
    };
    struct RenderFrameBuilder { RenderFrame* m_Frame; };
    typedef void (*RenderFrameSubmit)(void* renderer, HRenderContext context, const RenderFrame& frame);
    struct RenderFrameConsumer
    {
        void* m_Renderer; // Renderer-owned scratch only, never a component world.
        RenderFrameSubmit m_Submit;
    };
    struct RenderFrameConsumers
    {
        RenderFrameConsumers();
        RenderFrameConsumer m_Items[RENDER_FRAME_CONSUMERS];
        uint32_t m_Mask;
    };

    void BeginRenderFrame(RenderFrameBuilder* builder, RenderFrame* frame, dmResource::HFactory factory,
                          uint64_t id, uint64_t tick, uint32_t width, uint32_t height, uint32_t generation);
    void RetireRenderFrame(RenderFrame* frame);
    uint64_t GetRenderFrameCapacity(const RenderFrame& frame);
    bool AdmitRenderFrameAllocation(RenderFrameBuilder* builder, uint64_t extra, uint64_t replaced);
    // Returns UINT32_MAX on failure. Every allocation is 16-byte aligned.
    uint32_t AllocateRenderFrameData(RenderFrameBuilder* builder, uint32_t bytes);
    bool AddRenderFrameEntry(RenderFrameBuilder* builder, const RenderFrameEntry& entry);
    bool RetainRenderFrameResource(RenderFrameBuilder* builder, void* resource);
    bool SealRenderFrame(RenderFrameBuilder* builder, const RenderFrameConsumers& consumers);
    bool RegisterRenderFrameConsumer(RenderFrameConsumers* consumers, uint32_t id, void* renderer, RenderFrameSubmit submit);
    void SubmitRenderFrame(HRenderContext context, const RenderFrame& frame, const RenderFrameConsumers& consumers);
    const void* GetRenderFrameData(const RenderFrame& frame, uint32_t offset, uint32_t bytes);
}
#endif
