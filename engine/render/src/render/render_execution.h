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

#ifndef DM_RENDER_EXECUTION_H
#define DM_RENDER_EXECUTION_H

#include <stdint.h>

namespace dmRender
{
    struct RenderExecutionContext;
    typedef RenderExecutionContext* HRenderExecution;

    // Engine-internal contract, not an extension SDK or a rendering architecture selector.
    enum ExecutionMode
    {
        EXECUTION_MODE_INLINE,
        EXECUTION_MODE_THREADED,
        EXECUTION_MODE_INVALID,
    };

    // Explains why mode selection chose inline/threaded execution or rejected the mode.
    // Used for startup diagnostics and capability-validation results.
    enum ExecutionReason
    {
        EXECUTION_REASON_INLINE,
        EXECUTION_REASON_BUILD_DISABLED,
        EXECUTION_REASON_THREADED,
        EXECUTION_REASON_THREADS_UNSUPPORTED,
        EXECUTION_REASON_EXECUTOR_UNAVAILABLE,
        EXECUTION_REASON_INVALID_MODE,
    };

    // Logical roles checked by ownership assertions. Inline assigns both to one thread;
    // threaded execution separates game work from rendering, including graphics calls.
    enum ExecutionOwner
    {
        EXECUTION_OWNER_GAME,
        EXECUTION_OWNER_RENDER,
    };

    // Build, platform and executor availability used to validate a requested mode.
    // These capabilities describe engine support, not project configuration.
    struct ExecutionCapabilities
    {
        uint8_t m_BuildEnabled : 1;
        uint8_t m_ThreadsSupported : 1;
        uint8_t m_ExecutorAvailable : 1;
    };

    // Resolved execution mode and its diagnostic reason, returned on success or failure.
    struct ExecutionSelection
    {
        ExecutionMode   m_ExecutionMode;
        ExecutionReason m_ExecutionReason;
    };

    ExecutionCapabilities GetExecutionCapabilities(bool executor_available);
    // The caller selects an explicit mode; configuration parsing belongs outside dmRender.
    // An excluded build always resolves valid modes to inline. Unsupported threading
    // in an enabled build fails, never silently changes owners.
    ExecutionSelection ResolveExecutionMode(ExecutionMode mode, const ExecutionCapabilities& capabilities);
    const char*        GetExecutionModeName(ExecutionMode mode);
    const char*        GetExecutionReasonString(ExecutionReason reason);

    // Resolve once, before extension/graphics/bootstrap initialization. No thread is created.
    // The caller becomes the game owner; inline also binds rendering to this caller.
    // executor_available is supplied by engine integration, NOT by project configuration.
    // It stays false until a caller can provide the required executor and migrated paths.
    // Returns null for an invalid selection. The selection is returned on success and failure.
    HRenderExecution NewRenderExecution(ExecutionMode mode, bool executor_available, ExecutionSelection* selection);
    // Immutable after construction; used to select inline or synchronized frame handoff.
    ExecutionMode GetRenderExecutionMode(HRenderExecution execution);

    // Threaded executor startup only: bind exactly one distinct render owner, including graphics.
    // Native: render thread. Web: browser main, with NewRenderExecution on the game pthread.
    // The executor must signal successful binding before allowing frame/owner work.
    // Binding does not transfer a native graphics context; backend ownership must be
    // established separately before executing graphics callbacks.
    // Returns false for inline, game-owner binding, or any second binding attempt.
    bool BindRenderExecutionOwner(HRenderExecution execution);
    // Called by the render owner after draining, before its thread exits or is joined.
    // This does not allow rebinding or runtime mode changes.
    void ReleaseRenderExecutionOwner(HRenderExecution execution);
    bool IsRenderExecutionOwner(HRenderExecution execution, ExecutionOwner owner);
    // Game owner only, after the executor has stopped/joined all other users.
    void DeleteRenderExecution(HRenderExecution execution);

    // Executor operation ownership (frame storage/handoff is introduced separately):
    // - Game: build/extract and publish immutable frame inputs; retire CPU references.
    // - Render: consume published inputs; cull, sort, batch, generate geometry and issue
    //   graphics calls. Backend-managed upload jobs retain their own rules; this contract
    //   does not make arbitrary dmGraphics calls safe on workers.
    // - Pump: service available work without waiting on the game owner on browser main.
    // - Stop/drain: stop admission, finish accepted readers, then release owner bindings.
    // Inline executes these stages sequentially with the same data/lifetime contract.
    // CPU frame retirement is not GPU completion. No borrowed mutable game/Lua pointers
    // may cross publication, and no slot may be reused while a CPU reader still uses it.
}

#endif // DM_RENDER_EXECUTION_H
