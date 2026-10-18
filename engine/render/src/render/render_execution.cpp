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

#include <dlib/mutex.h>
#include <dlib/thread.h>

#include "render_execution.h"

namespace dmRender
{
    // Engine-owned execution/ownership context, not the renderer or its scene data.
    // Records the selected mode and uses per-context TLS to identify the game and
    // render owners so call sites can assert that work runs on the correct thread.
    // Inline mode assigns both roles to the caller; threaded mode allows one distinct
    // render owner to bind. Graphics operations belong to that render owner as well.
    // Created during engine startup and destroyed by the game owner after all other
    // users have stopped and any separate render owner has released its binding.
    //
    // This context creates no threads, schedules no jobs and stores no frames.
    // The separate frame queue and its executors build around this ownership contract:
    // publish immutable frame inputs, consume them inline or on the render owner,
    // signal completion and drain accepted work before shutdown.
    struct RenderExecutionContext
    {
        ExecutionMode    m_Mode;
        dmThread::TlsKey m_OwnerKey;
        dmMutex::HMutex  m_BindMutex;
        bool             m_RenderBound;
        bool             m_RenderAttached;
        char             m_GameToken;
        char             m_RenderToken;
    };

    ExecutionCapabilities GetExecutionCapabilities(bool executor_available)
    {
        ExecutionCapabilities capabilities;
#if defined(DM_THREADED_RENDERING) && DM_THREADED_RENDERING
        capabilities.m_BuildEnabled = true;
#else
        capabilities.m_BuildEnabled = false;
#endif
        // Do not change dlib's worker-pool policy as a side effect of render execution.
#if !defined(DM_USE_SINGLE_THREAD) && (defined(DM_HAS_THREADS) || defined(__EMSCRIPTEN_PTHREADS__))
        capabilities.m_ThreadsSupported = true;
#else
        capabilities.m_ThreadsSupported = false;
#endif
        capabilities.m_ExecutorAvailable = executor_available;
        return capabilities;
    }

    ExecutionSelection ResolveExecutionMode(ExecutionMode mode, const ExecutionCapabilities& capabilities)
    {
        ExecutionSelection selection;
        selection.m_ExecutionMode = EXECUTION_MODE_INLINE;
        if (mode != EXECUTION_MODE_INLINE && mode != EXECUTION_MODE_THREADED)
        {
            selection.m_ExecutionMode = EXECUTION_MODE_INVALID;
            selection.m_ExecutionReason = EXECUTION_REASON_INVALID_MODE;
        }
        else if (!capabilities.m_BuildEnabled)
        {
            selection.m_ExecutionReason = EXECUTION_REASON_BUILD_DISABLED;
        }
        else if (mode == EXECUTION_MODE_INLINE)
        {
            selection.m_ExecutionReason = EXECUTION_REASON_INLINE;
        }
        else if (!capabilities.m_ThreadsSupported)
        {
            selection.m_ExecutionMode = EXECUTION_MODE_INVALID;
            selection.m_ExecutionReason = EXECUTION_REASON_THREADS_UNSUPPORTED;
        }
        else if (!capabilities.m_ExecutorAvailable)
        {
            selection.m_ExecutionMode = EXECUTION_MODE_INVALID;
            selection.m_ExecutionReason = EXECUTION_REASON_EXECUTOR_UNAVAILABLE;
        }
        else
        {
            selection.m_ExecutionMode = EXECUTION_MODE_THREADED;
            selection.m_ExecutionReason = EXECUTION_REASON_THREADED;
        }
        return selection;
    }

    const char* GetExecutionModeName(ExecutionMode mode)
    {
        switch (mode)
        {
            case EXECUTION_MODE_INLINE:   return "inline";
            case EXECUTION_MODE_THREADED: return "threaded";
            default:                      return "invalid";
        }
    }

    const char* GetExecutionReasonString(ExecutionReason reason)
    {
        switch (reason)
        {
            case EXECUTION_REASON_INLINE:               return "inline mode";
            case EXECUTION_REASON_BUILD_DISABLED:       return "threaded rendering excluded by build";
            case EXECUTION_REASON_THREADED:             return "threaded mode";
            case EXECUTION_REASON_THREADS_UNSUPPORTED:  return "platform thread support unavailable";
            case EXECUTION_REASON_EXECUTOR_UNAVAILABLE: return "threaded executor unavailable";
            default:                                    return "invalid execution mode";
        }
    }

    HRenderExecution NewRenderExecution(ExecutionMode mode, bool executor_available, ExecutionSelection* selection)
    {
        *selection = ResolveExecutionMode(mode, GetExecutionCapabilities(executor_available));
        if (selection->m_ExecutionMode == EXECUTION_MODE_INVALID)
            return 0;

        RenderExecutionContext* execution = new RenderExecutionContext;
        execution->m_Mode = selection->m_ExecutionMode;
        execution->m_OwnerKey = dmThread::AllocTls();
        execution->m_BindMutex = dmMutex::New();
        execution->m_RenderBound = selection->m_ExecutionMode == EXECUTION_MODE_INLINE;
        execution->m_RenderAttached = false;
        // TLS membership is portable; Win32 GetCurrentThread returns a pseudo-handle
        // that must not be compared with a stored value to determine ownership.
        dmThread::SetTlsValue(execution->m_OwnerKey, &execution->m_GameToken);
        return execution;
    }

    ExecutionMode GetRenderExecutionMode(HRenderExecution execution)
    {
        return execution->m_Mode;
    }

    bool BindRenderExecutionOwner(HRenderExecution execution)
    {
        DM_MUTEX_SCOPED_LOCK(execution->m_BindMutex);
        if (execution->m_RenderBound || IsRenderExecutionOwner(execution, EXECUTION_OWNER_GAME))
            return false;

        dmThread::SetTlsValue(execution->m_OwnerKey, &execution->m_RenderToken);
        execution->m_RenderBound = true;
        execution->m_RenderAttached = true;
        return true;
    }

    void ReleaseRenderExecutionOwner(HRenderExecution execution)
    {
        assert(execution->m_Mode == EXECUTION_MODE_THREADED);
        assert(IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER));
        DM_MUTEX_SCOPED_LOCK(execution->m_BindMutex);
        dmThread::SetTlsValue(execution->m_OwnerKey, 0);
        execution->m_RenderAttached = false;
    }

    bool IsRenderExecutionOwner(HRenderExecution execution, ExecutionOwner owner)
    {
        void* token = dmThread::GetTlsValue(execution->m_OwnerKey);
        if (owner == EXECUTION_OWNER_GAME)
            return token == &execution->m_GameToken;
        if (owner == EXECUTION_OWNER_RENDER)
            return token == &execution->m_RenderToken || (execution->m_Mode == EXECUTION_MODE_INLINE && token == &execution->m_GameToken);
        return false;
    }

    void DeleteRenderExecution(HRenderExecution execution)
    {
        assert(IsRenderExecutionOwner(execution, EXECUTION_OWNER_GAME));
        assert(!execution->m_RenderAttached);
        dmThread::SetTlsValue(execution->m_OwnerKey, 0);
        dmThread::FreeTls(execution->m_OwnerKey);
        dmMutex::Delete(execution->m_BindMutex);
        delete execution;
    }
}
