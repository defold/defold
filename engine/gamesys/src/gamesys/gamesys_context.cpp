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

#include <dlib/log.h>
#include <gameobject/gameobject.h>
#include <render/render_frame.h>
#include <render/render_graphics_service.h>

#include "gamesys_context_private.h"

namespace dmGameSystem
{
    static void ServiceResourceGraphics(void* context)
    {
        // Loading may update on a worker; the renderer checks ownership before servicing.
        dmRender::UpdateGraphicsService((dmRender::HRenderContext)context);
    }

    static dmResource::Result BeginFactoryMutation(void* user_context)
    {
        HContext context = (HContext)user_context;
        if (!dmRender::IsRenderExecutionOwner(dmRender::GetRenderExecution(context->m_Params.m_RenderContext), dmRender::EXECUTION_OWNER_GAME))
        {
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        return BeginResourceMutation(context);
    }

    static void EndFactoryMutation(void* context)
    {
        EndResourceMutation((HContext)context);
    }

    HContext NewContext(const ContextParams& params)
    {
        // Component migration will size this reserve for captured workloads. Fixed pass
        // storage is added from command capacity; constant snapshots grow separately.
        const dmRender::RenderFrameLimits component_limits = { 256 * 1024, 1024, 1024 };
        dmRender::RenderFrameLimits limits;
        if (dmRender::CalculateRenderFrameLimits(params.m_RenderContext, component_limits, &limits) != dmRender::RENDER_FRAME_OK)
        {
            return 0;
        }
        // Legacy inline path (current production): built-in components still submit
        // from live game data before frame consumption. Snapshot-only callers select
        // consumer preparation through internal setup instead.
        return NewContext(params, dmRender::GetRenderExecution(params.m_RenderContext), limits, dmRender::RENDER_FRAME_PREPARATION_LEGACY);
    }

    HContext NewContext(const ContextParams& params, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits, dmRender::RenderFramePreparation preparation)
    {
        Context* context = new Context();
        context->m_Params = params;
        context->m_GraphicsService = params.m_RenderContext ? dmRender::GetGraphicsService(params.m_RenderContext) : 0;
        context->m_ComponentRenderContext = NewComponentRenderContext(params.m_Register, execution, limits, params.m_RenderContext, preparation);
        if (!context->m_ComponentRenderContext)
        {
            delete context;
            return 0;
        }

        // Graphics-free tests have no renderer to service. Both contexts outlive the
        // factory, so progress and mutation checks also work after Finalize.
        if (params.m_RenderContext)
        {
            dmResource::SetFactoryService(params.m_Factory, ServiceResourceGraphics, params.m_RenderContext);
            dmResource::SetFactoryMutationCallbacks(params.m_Factory, BeginFactoryMutation, EndFactoryMutation, context);
        }

        return context;
    }

    dmGameObject::Result RegisterComponentType(HContext context, const dmGameObject::ComponentType& type, const ComponentRenderType* render_type)
    {
        return RegisterComponentType(context->m_ComponentRenderContext, type, render_type);
    }

    static bool CheckRenderResult(dmRender::RenderFrameResult result, const char* operation)
    {
        if (result == dmRender::RENDER_FRAME_OK)
            return true;
        dmLogError("Unable to %s game-system frame (%d)", operation, result);
        return false;
    }

    bool BeginFrame(HContext context)
    {
        return CheckRenderResult(BeginComponentRenderFrame(context->m_ComponentRenderContext), "begin");
    }

    dmRender::HGraphicsService GetGraphicsService(HContext context)
    {
        return context->m_GraphicsService;
    }

    bool PrepareFrame(HContext context)
    {
        if (context->m_AsyncResourceMutationPending)
        {
            dmRender::UpdateGraphicsService(context->m_Params.m_RenderContext);
        }

        // Completion callbacks may have queued another mutation. Defer rather than
        // treating ordinary backpressure as a frame failure or waiting on this thread.
        return !context->m_AsyncResourceMutationPending;
    }

    dmResource::Result BeginResourceMutation(HContext context)
    {
        if (context->m_AsyncResourceMutationPending)
        {
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        // Preserve synchronous setters in legacy inline callbacks, including render
        // scripts and PostUpdate. Captured components must opt into the barrier even
        // before their render-list preparation moves to the consumer.
        if (!context->m_ComponentRenderContext || !RequiresComponentRenderResourceBarrier(context->m_ComponentRenderContext))
        {
            return dmResource::RESULT_OK;
        }

        return BeginRenderResourceMutation(context);
    }

    void EndResourceMutation(HContext context)
    {
        if (context->m_ComponentRenderContext && RequiresComponentRenderResourceBarrier(context->m_ComponentRenderContext))
        {
            EndRenderResourceMutation(context);
        }
    }

    dmResource::Result BeginRenderResourceMutation(HContext context)
    {
        if (context->m_AsyncResourceMutationPending || context->m_GraphicsFramePending)
        {
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        dmRender::RenderFrameResult result = BeginComponentRenderResourceMutation(context->m_ComponentRenderContext);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            dmLogError("Cannot begin resource mutation while frame capture or retirement is active (%d).", result);
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        return dmResource::RESULT_OK;
    }

    void EndRenderResourceMutation(HContext context)
    {
        EndComponentRenderResourceMutation(context->m_ComponentRenderContext);
    }

    bool IsResourceMutationPending(HContext context)
    {
        return context->m_AsyncResourceMutationPending;
    }

    void SetGraphicsFramePending(HContext context, bool pending)
    {
        context->m_GraphicsFramePending = pending;
    }

    void BeginRender(HContext context, float time, float dt)
    {
        if (GetComponentRenderFramePreparation(context->m_ComponentRenderContext) == dmRender::RENDER_FRAME_PREPARATION_LEGACY)
        {
            dmRender::BeginFrame(context->m_Params.m_RenderContext, time, dt);
        }
        else
        {
            dmRender::BeginRenderFrameRecording(context->m_Params.m_RenderContext, time, dt);
        }
    }

    bool Render(HContext context, dmGameObject::HCollection collection)
    {
        if (context->m_Params.m_RenderContext && GetComponentRenderFramePreparation(context->m_ComponentRenderContext) == dmRender::RENDER_FRAME_PREPARATION_LEGACY)
        {
            // Keep the legacy list reset after PreRender, immediately before component
            // traversal. Snapshot lists are instead prepared on the consumer.
            dmRender::RenderListBegin(context->m_Params.m_RenderContext);
        }

        if (RenderComponents(context->m_ComponentRenderContext, collection))
            return true;
        dmLogError("Component rendering or render-input capture failed.");
        return false;
    }

    bool SubmitRender(HContext context, dmRender::HRenderScriptInstance script, float dt, uint32_t clear_color)
    {
        HComponentRenderContext components = context->m_ComponentRenderContext;
        dmRender::HRenderContext render_context = context->m_Params.m_RenderContext;
        dmRender::HRenderFrameStorage frame = GetComponentRenderFrame(components);
        // Graphics-free lifecycle tests submit component inputs without render passes.
        if (render_context)
        {
            if (!frame)
            {
                return CheckRenderResult(dmRender::RENDER_FRAME_INVALID_STATE, "record");
            }

            dmRender::RenderFrameInfo info = dmRender::GetRenderFrameInfo(render_context);
            dmRender::RenderFrameResult result = dmRender::RecordRenderFrameInfo(frame, info);
            if (result == dmRender::RENDER_FRAME_OK)
            {
                if (script)
                {
                    result = dmRender::RecordRenderScriptInstance(script, dt, frame);
                }
                else
                {
                    result = dmRender::RecordDefaultRenderCommands(render_context, frame, info, clear_color);
                }
            }
            if (result != dmRender::RENDER_FRAME_OK)
            {
                // Do not publish a valid-looking prefix after Lua/allocation failure.
                // Submit observes this sticky error and releases the entire snapshot.
                dmRender::FailRenderFrame(frame, result);
            }
        }
        return CheckRenderResult(SubmitComponentRenderFrame(components), "submit");
    }

    void EndRender(HContext context)
    {
        if (GetComponentRenderFramePreparation(context->m_ComponentRenderContext) == dmRender::RENDER_FRAME_PREPARATION_LEGACY)
        {
            // Legacy inline path: keep cleanup after PostUpdate. Captured-frame
            // consumers do this in EndRenderFrameConsumption after their last pass.
            dmRender::ClearRenderObjects(context->m_Params.m_RenderContext);
        }
    }

    bool PostUpdate(HContext context, dmGameObject::HCollection collection)
    {
        // Keep dependencies alive through drawing, then allow destructive game cleanup.
        EndComponentRenderFrame(context->m_ComponentRenderContext);
        if (collection)
            dmGameObject::PostUpdate(collection);
        dmGameObject::PostUpdate(context->m_Params.m_Register);
        return CheckRenderResult(ServiceComponentRenderContext(context->m_ComponentRenderContext), "finish");
    }

    bool Finalize(HContext context)
    {
        // Collection deletion may have queued more work. Keep admission open for
        // resource and extension cleanup that follows gamesys finalization.
        dmRender::DrainGraphicsService(context->m_Params.m_RenderContext);
        if (!context->m_ComponentRenderContext)
        {
            return true; // Resource callbacks may still use gamesys after finalization.
        }

        dmRender::RenderFrameResult result = ShutdownComponentRenderContext(context->m_ComponentRenderContext);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            dmLogError("Unable to finalize game system (%d)", result);
            return false;
        }

        DeleteComponentRenderContext(context->m_ComponentRenderContext);
        context->m_ComponentRenderContext = 0;
        return true;
    }

    void DeleteContext(HContext context)
    {
        // Resource types (notably modelc) retain pointers into this allocation.
        // It outlives the renderer bridge and is freed only after the factory.
        delete context;
    }
}
