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

    /**
     * Engine-owned mode and thread-ownership context. Creates no threads or frames.
     * The context must outlive its frame queues and all callers.
     */
    typedef RenderExecutionContext* HRenderExecution;

    /**
     * Engine-internal execution mode, not an extension SDK or rendering architecture selector.
     */
    enum ExecutionMode
    {
        EXECUTION_MODE_INLINE,
        EXECUTION_MODE_THREADED,
        EXECUTION_MODE_INVALID,
    };

    /**
     * Mode-selection result for startup diagnostics and capability validation.
     * INLINE, BUILD_DISABLED and THREADED are successful selections; the other results
     * reject the request. Kept separate from ExecutionMode because multiple results
     * can select the same mode.
     */
    enum ExecutionResult
    {
        EXECUTION_RESULT_INLINE,
        EXECUTION_RESULT_BUILD_DISABLED,
        EXECUTION_RESULT_THREADED,
        EXECUTION_RESULT_THREADS_UNSUPPORTED,
        EXECUTION_RESULT_EXECUTOR_UNAVAILABLE,
        EXECUTION_RESULT_INVALID_MODE,
    };

    /**
     * Logical roles checked by ownership assertions. Inline assigns both to one thread;
     * threaded execution separates game work from rendering, including graphics calls.
     */
    enum ExecutionOwner
    {
        EXECUTION_OWNER_GAME,
        EXECUTION_OWNER_RENDER,
    };

    /**
     * Build, platform and executor availability used to validate a requested mode.
     * These capabilities describe engine support, not project configuration.
     */
    struct ExecutionCapabilities
    {
        uint8_t m_BuildEnabled : 1;
        uint8_t m_ThreadsSupported : 1;
        uint8_t m_ExecutorAvailable : 1;
    };

    /**
     * Resolved execution mode and selection result, returned on success or failure.
     */
    struct ExecutionSelection
    {
        ExecutionMode   m_ExecutionMode;
        ExecutionResult m_ExecutionResult;
    };

    /**
     * Report this build's capabilities without changing the dlib worker-pool policy.
     * @param executor_available Whether engine integration provides an executor and migrated paths
     * @return Build, platform and supplied executor capabilities
     */
    ExecutionCapabilities GetExecutionCapabilities(bool executor_available);

    /**
     * Validate an explicit mode; configuration parsing belongs outside dmRender.
     * A build without threaded rendering resolves either valid request to inline.
     * Unsupported threading in an enabled build fails, never silently changes owners.
     * @param mode Requested execution mode
     * @param capabilities Available build, platform and executor capabilities
     * @return Selected mode and result; failures select EXECUTION_MODE_INVALID
     */
    ExecutionSelection ResolveExecutionMode(ExecutionMode mode, const ExecutionCapabilities& capabilities);

    /**
     * Get a diagnostic name for an execution mode.
     * @param mode Execution mode
     * @return Static string; unknown values return "invalid"
     */
    const char* GetExecutionModeName(ExecutionMode mode);

    /**
     * Get a diagnostic description of a mode-selection result.
     * @param result Mode-selection result
     * @return Static string; unknown values return "invalid execution mode"
     */
    const char* GetExecutionResultString(ExecutionResult result);

    /**
     * Resolve and create a context before extension, graphics and bootstrap initialization.
     * The caller becomes the game owner; inline also assigns rendering to this caller.
     * No thread is created. Executor availability comes from engine integration, not
     * project configuration, and remains false until migrated paths can be executed.
     * @param mode Requested execution mode
     * @param executor_available Whether engine integration provides the required executor
     * @param selection Required output, written on success and failure
     * @return New context, or null if mode selection fails
     */
    HRenderExecution NewRenderExecution(ExecutionMode mode, bool executor_available, ExecutionSelection* selection);

    /**
     * Get the immutable mode used to select inline or synchronized frame handoff.
     * @param execution Valid execution context
     * @return Selected execution mode
     */
    ExecutionMode GetRenderExecutionMode(HRenderExecution execution);

    /**
     * Bind the calling thread as the distinct render owner during threaded executor startup.
     * Native uses a render thread; web uses browser main, with the execution context created
     * on the game pthread. Signal successful binding before allowing frame/owner work. Binding does not
     * transfer a graphics context; backend ownership must be established separately.
     * @param execution Valid execution context
     * @return True on the first distinct binding; false for inline, game-owner or later attempts
     */
    bool BindRenderExecutionOwner(HRenderExecution execution);

    /**
     * Release the threaded render owner's binding after draining, before it exits or is joined.
     * Call only from the bound render owner. Does not allow rebinding or runtime mode changes.
     * @param execution Valid threaded execution context
     */
    void ReleaseRenderExecutionOwner(HRenderExecution execution);

    /**
     * Check the caller's logical ownership without comparing platform thread handles.
     * @param execution Valid execution context
     * @param owner Logical role to check
     * @return True if the caller owns the role; inline assigns both roles to the game owner
     */
    bool IsRenderExecutionOwner(HRenderExecution execution, ExecutionOwner owner);

    /**
     * Delete the context on its game owner after all other users have stopped or been joined.
     * Any threaded render owner must already have released its binding.
     * @param execution Valid execution context to delete
     */
    void DeleteRenderExecution(HRenderExecution execution);

    /**
     * Executor operation ownership:
     * - Game: build/extract and publish immutable frame inputs; retire CPU references.
     * - Render: consume published inputs; cull, sort, batch, generate geometry and issue
     *   graphics calls. Backend-managed upload jobs retain their own rules; this contract
     *   does not make arbitrary dmGraphics calls safe on workers.
     * - Pump: service available work without waiting on the game owner on browser main.
     * - Stop/drain: stop admission, finish accepted readers, then release owner bindings.
     * Inline executes these stages sequentially with the same data/lifetime contract.
     * CPU frame retirement is not GPU completion. No borrowed mutable game/Lua pointers
     * may cross publication, and no slot may be reused while a CPU reader still uses it.
     */
}

#endif // DM_RENDER_EXECUTION_H
