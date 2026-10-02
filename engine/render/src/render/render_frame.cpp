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

#include <assert.h>
#include <string.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include "render_frame.h"

namespace dmRender
{
    static bool ParseSelector(const char* value, uint32_t maximum, uint32_t* result)
    {
        if (!value) { *result = 0; return true; }
        if (value[0] < '0' || value[0] > '0' + maximum || value[1]) return false;
        *result = value[0] - '0';
        return true;
    }

    bool ResolvePocCondition(const char* pipeline, const char* threaded, const char* sprite,
                             const char* graphics, PocCondition* result, const char** error)
    {
        result->m_Pipeline = POC_LEGACY;
        result->m_Threaded = false;
        *error = 0;
        uint32_t s, g, t;
        if (!ParseSelector(sprite, 2, &s) || !ParseSelector(graphics, 2, &g) || !ParseSelector(threaded, 1, &t))
            *error = "PoC selectors require 0/1 (threaded) or 0/1/2 (legacy aliases)";
        else if ((pipeline || threaded) && (sprite || graphics))
            *error = "Do not combine named PoC selectors with explicit legacy aliases, including zero";
        else if (s && g)
            *error = "Component and graphics pipelines cannot be combined";
        else if (pipeline || threaded)
        {
            const char* name = pipeline ? pipeline : "legacy";
            if (!strcmp(name, "legacy")) result->m_Pipeline = POC_LEGACY;
            else if (!strcmp(name, "component")) result->m_Pipeline = POC_COMPONENT;
            else if (!strcmp(name, "graphics")) result->m_Pipeline = POC_GRAPHICS;
            else if (!strcmp(name, "renderframe")) result->m_Pipeline = POC_RENDERFRAME;
            else *error = "Unknown render.poc_pipeline; expected legacy/component/graphics/renderframe";
            result->m_Threaded = t != 0;
            if (result->m_Pipeline == POC_LEGACY && t) *error = "The legacy pipeline has no threaded execution policy";
        }
        else if (s || g)
        {
            result->m_Pipeline = s ? POC_COMPONENT : POC_GRAPHICS;
            result->m_Threaded = (s ? s : g) == 2;
        }
        return *error == 0;
    }

    const char* GetPocConditionName(const PocCondition& condition)
    {
        switch (condition.m_Pipeline)
        {
            case POC_COMPONENT: return condition.m_Threaded ? "snapshot-threaded" : "snapshot-inline";
            case POC_GRAPHICS: return condition.m_Threaded ? "graphics-threaded" : "graphics-inline";
            case POC_RENDERFRAME: return condition.m_Threaded ? "renderframe-threaded" : "renderframe-inline";
            default: return "existing";
        }
    }

    RenderFrame::RenderFrame() : m_Factory(0), m_Id(0), m_Tick(0), m_GrowthPeak(sizeof(RenderFrame)),
        m_SurfaceGeneration(0), m_Width(0), m_Height(0), m_Limit(RENDER_FRAME_LIMIT), m_ConsumerMask(0),
        m_Time(0), m_Dt(0), m_State(FRAME_FREE), m_Overflow(false)
    {
        memset(m_Payloads, 0, sizeof(m_Payloads));
    }

    RenderFrame::~RenderFrame()
    {
        assert(m_State != FRAME_READING);
        RetireRenderFrame(this);
        for (uint32_t i = 0; i < RENDER_FRAME_CONSUMERS; ++i)
            if (m_Payloads[i].m_Data) m_Payloads[i].m_Delete(m_Payloads[i].m_Data);
    }

    RenderFrameConsumers::RenderFrameConsumers() : m_Mask(0) { memset(m_Items, 0, sizeof(m_Items)); }

    uint64_t GetRenderFrameCapacity(const RenderFrame& frame)
    {
        uint64_t bytes = sizeof(RenderFrame) + GetCapturedCommandsCapacity(frame.m_Commands) - sizeof(frame.m_Commands) + frame.m_Data.Capacity() +
            (uint64_t)frame.m_Entries.Capacity() * sizeof(RenderFrameEntry) +
            (uint64_t)frame.m_Dependencies.Capacity() * sizeof(RenderFrameDependency) + frame.m_DependencyMap.Capacity() * 32ULL;
        for (uint32_t i = 0; i < RENDER_FRAME_CONSUMERS; ++i) bytes += frame.m_Payloads[i].m_Capacity;
        return bytes;
    }

    bool AdmitRenderFrameAllocation(RenderFrameBuilder* builder, uint64_t extra, uint64_t replaced)
    {
        RenderFrame& frame = *builder->m_Frame;
        assert(frame.m_State == FRAME_BUILDING);
        uint64_t peak = GetRenderFrameCapacity(frame) + extra + replaced;
        frame.m_GrowthPeak = dmMath::Max(frame.m_GrowthPeak, peak);
        if (peak > frame.m_Limit) frame.m_Overflow = true;
        return !frame.m_Overflow;
    }

    void RetireRenderFrame(RenderFrame* frame)
    {
        assert(frame->m_State != FRAME_READING);
        for (uint32_t i = 0; i < frame->m_Dependencies.Size(); ++i)
            dmResource::Release(frame->m_Factory, frame->m_Dependencies[i].m_Resource);
        frame->m_Dependencies.SetSize(0);
        frame->m_DependencyMap.Clear();
        frame->m_Data.SetSize(0);
        frame->m_Entries.SetSize(0);
        frame->m_Commands.m_Count = 0;
        frame->m_ConsumerMask = 0;
        frame->m_State = FRAME_RETIRED;
    }

    void BeginRenderFrame(RenderFrameBuilder* builder, RenderFrame* frame, dmResource::HFactory factory,
                          uint64_t id, uint64_t tick, uint32_t width, uint32_t height, uint32_t generation)
    {
        assert(frame->m_State == FRAME_FREE || frame->m_State == FRAME_RETIRED);
        RetireRenderFrame(frame);
        frame->m_Factory = factory;
        frame->m_Id = id;
        frame->m_Tick = tick;
        frame->m_Width = width;
        frame->m_Height = height;
        frame->m_SurfaceGeneration = generation;
        frame->m_Overflow = false;
        frame->m_State = FRAME_BUILDING;
        builder->m_Frame = frame;
    }

    template <typename T> static bool Grow(RenderFrameBuilder* builder, dmArray<T>& array, uint32_t needed)
    {
        if (needed <= array.Capacity()) return true;
        uint64_t capacity = dmMath::Max((uint64_t)needed, dmMath::Max((uint64_t)16, (uint64_t)array.Capacity() * 2));
        if (!AdmitRenderFrameAllocation(builder, (capacity - array.Capacity()) * sizeof(T), (uint64_t)array.Capacity() * sizeof(T))) return false;
        array.SetCapacity((uint32_t)capacity);
        return true;
    }

    uint32_t AllocateRenderFrameData(RenderFrameBuilder* builder, uint32_t bytes)
    {
        RenderFrame& frame = *builder->m_Frame;
        uint64_t offset = ((uint64_t)frame.m_Data.Size() + 15) & ~15ULL;
        uint64_t end = offset + bytes;
        if (end > frame.m_Limit) { frame.m_Overflow = true; return 0xffffffffu; }
        if (!Grow(builder, frame.m_Data, (uint32_t)end)) return 0xffffffffu;
        frame.m_Data.SetSize((uint32_t)end);
        return (uint32_t)offset;
    }

    bool AddRenderFrameEntry(RenderFrameBuilder* builder, const RenderFrameEntry& entry)
    {
        RenderFrame& frame = *builder->m_Frame;
        if (entry.m_Consumer >= RENDER_FRAME_CONSUMERS || !GetRenderFrameData(frame, entry.m_Payload, entry.m_PayloadBytes))
        { frame.m_Overflow = true; return false; }
        if (!Grow(builder, frame.m_Entries, frame.m_Entries.Size() + 1)) return false;
        frame.m_Entries.Push(entry);
        frame.m_ConsumerMask |= 1u << entry.m_Consumer;
        return true;
    }

    bool RetainRenderFrameResource(RenderFrameBuilder* builder, void* resource)
    {
        RenderFrame& frame = *builder->m_Frame;
        if (!resource || frame.m_DependencyMap.Get((uintptr_t)resource)) return true;
        if (!Grow(builder, frame.m_Dependencies, frame.m_Dependencies.Size() + 1)) return false;
        if (frame.m_DependencyMap.Full())
        {
            uint32_t capacity = dmMath::Max(16u, frame.m_DependencyMap.Capacity() * 2);
            if (!AdmitRenderFrameAllocation(builder, (capacity - frame.m_DependencyMap.Capacity()) * 32ULL, frame.m_DependencyMap.Capacity() * 32ULL)) return false;
            frame.m_DependencyMap.SetCapacity(dmMath::Max(1u, capacity / 3), capacity);
        }
        RenderFrameDependency dependency = { resource, dmResource::GetVersion(frame.m_Factory, resource) };
        dmResource::IncRef(frame.m_Factory, resource);
        frame.m_DependencyMap.Put((uintptr_t)resource, frame.m_Dependencies.Size());
        frame.m_Dependencies.Push(dependency);
        return true;
    }

    const void* GetRenderFrameData(const RenderFrame& frame, uint32_t offset, uint32_t bytes)
    {
        if (!bytes || (offset & 15) || (uint64_t)offset + bytes > frame.m_Data.Size()) return 0;
        return frame.m_Data.Begin() + offset;
    }

    bool RegisterRenderFrameConsumer(RenderFrameConsumers* consumers, uint32_t id, void* renderer, RenderFrameSubmit submit)
    {
        if (id >= RENDER_FRAME_CONSUMERS || !submit || (consumers->m_Mask & (1u << id))) return false;
        consumers->m_Items[id].m_Renderer = renderer;
        consumers->m_Items[id].m_Submit = submit;
        consumers->m_Mask |= 1u << id;
        return true;
    }

    bool SealRenderFrame(RenderFrameBuilder* builder, const RenderFrameConsumers& consumers)
    {
        RenderFrame& frame = *builder->m_Frame;
        assert(frame.m_State == FRAME_BUILDING);
        if (frame.m_Overflow || GetRenderFrameCapacity(frame) > frame.m_Limit || (frame.m_ConsumerMask & ~consumers.m_Mask)) return false;
        for (uint32_t i = 0; i < frame.m_Dependencies.Size(); ++i)
            if (dmResource::GetVersion(frame.m_Factory, frame.m_Dependencies[i].m_Resource) != frame.m_Dependencies[i].m_Generation) return false;
        for (uint32_t i = 0; i < frame.m_Entries.Size(); ++i)
            if (!GetRenderFrameData(frame, frame.m_Entries[i].m_Payload, frame.m_Entries[i].m_PayloadBytes)) return false;
        frame.m_State = FRAME_READY;
        return true;
    }

    void SubmitRenderFrame(HRenderContext context, const RenderFrame& frame, const RenderFrameConsumers& consumers)
    {
        assert(frame.m_State == FRAME_READING);
        for (uint32_t i = 0; i < RENDER_FRAME_CONSUMERS; ++i)
            if (frame.m_ConsumerMask & (1u << i)) consumers.m_Items[i].m_Submit(consumers.m_Items[i].m_Renderer, context, frame);
    }
}
