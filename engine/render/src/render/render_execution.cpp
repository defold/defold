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

#include <dlib/mutex.h>
#include <dlib/thread.h>

#include "render_execution.h"

namespace dmRender
{
    struct RenderExecution
    {
        ExecutionPolicy  m_Policy;
        dmThread::TlsKey m_OwnerKey;
        dmMutex::HMutex  m_BindMutex;
        bool             m_RenderBound;
        bool             m_RenderAttached;
        char             m_GameToken;
        char             m_RenderToken;
    };

    ExecutionRequest ParseExecutionRequest(const char* value)
    {
        if (!value)
            return EXECUTION_REQUEST_DEFAULT;
        if (strcmp(value, "0") == 0)
            return EXECUTION_REQUEST_INLINE;
        if (strcmp(value, "1") == 0)
            return EXECUTION_REQUEST_THREADED;
        return EXECUTION_REQUEST_INVALID;
    }

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

    ExecutionSelection ResolveExecutionPolicy(ExecutionRequest request, const ExecutionCapabilities& capabilities)
    {
        ExecutionSelection selection;
        selection.m_Policy = EXECUTION_POLICY_INLINE;
        if (request != EXECUTION_REQUEST_DEFAULT && request != EXECUTION_REQUEST_INLINE && request != EXECUTION_REQUEST_THREADED)
        {
            selection.m_Policy = EXECUTION_POLICY_INVALID;
            selection.m_Reason = EXECUTION_REASON_INVALID_REQUEST;
        }
        else if (!capabilities.m_BuildEnabled)
        {
            selection.m_Reason = EXECUTION_REASON_BUILD_DISABLED;
        }
        else if (request == EXECUTION_REQUEST_DEFAULT)
        {
            // The default changes only in the final, qualified release PR.
            selection.m_Reason = EXECUTION_REASON_DEFAULT_INLINE;
        }
        else if (request == EXECUTION_REQUEST_INLINE)
        {
            selection.m_Reason = EXECUTION_REASON_EXPLICIT_INLINE;
        }
        else if (!capabilities.m_ThreadsSupported)
        {
            selection.m_Policy = EXECUTION_POLICY_INVALID;
            selection.m_Reason = EXECUTION_REASON_THREADS_UNSUPPORTED;
        }
        else if (!capabilities.m_ExecutorAvailable)
        {
            selection.m_Policy = EXECUTION_POLICY_INVALID;
            selection.m_Reason = EXECUTION_REASON_EXECUTOR_UNAVAILABLE;
        }
        else
        {
            selection.m_Policy = EXECUTION_POLICY_THREADED;
            selection.m_Reason = EXECUTION_REASON_THREADED;
        }
        return selection;
    }

    const char* GetExecutionPolicyName(ExecutionPolicy policy)
    {
        switch (policy)
        {
            case EXECUTION_POLICY_INLINE:   return "inline";
            case EXECUTION_POLICY_THREADED: return "threaded";
            default:                        return "invalid";
        }
    }

    const char* GetExecutionReasonString(ExecutionReason reason)
    {
        switch (reason)
        {
            case EXECUTION_REASON_DEFAULT_INLINE:       return "development default";
            case EXECUTION_REASON_EXPLICIT_INLINE:      return "explicit inline request";
            case EXECUTION_REASON_BUILD_DISABLED:       return "threaded rendering excluded by build";
            case EXECUTION_REASON_THREADED:             return "explicit threaded request";
            case EXECUTION_REASON_THREADS_UNSUPPORTED:  return "platform thread support unavailable";
            case EXECUTION_REASON_EXECUTOR_UNAVAILABLE: return "threaded executor unavailable";
            default:                                    return "invalid render.internal_threaded value (expected 0 or 1)";
        }
    }

    HRenderExecution NewRenderExecution(ExecutionRequest request, bool executor_available, ExecutionSelection* selection)
    {
        *selection = ResolveExecutionPolicy(request, GetExecutionCapabilities(executor_available));
        if (selection->m_Policy == EXECUTION_POLICY_INVALID)
            return 0;

        RenderExecution* execution = new RenderExecution;
        execution->m_Policy = selection->m_Policy;
        execution->m_OwnerKey = dmThread::AllocTls();
        execution->m_BindMutex = dmMutex::New();
        execution->m_RenderBound = selection->m_Policy == EXECUTION_POLICY_INLINE;
        execution->m_RenderAttached = false;
        // TLS membership is portable; Win32 GetCurrentThread returns a pseudo-handle
        // that must not be compared with a stored value to determine ownership.
        dmThread::SetTlsValue(execution->m_OwnerKey, &execution->m_GameToken);
        return execution;
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
        assert(execution->m_Policy == EXECUTION_POLICY_THREADED);
        AssertRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER);
        DM_MUTEX_SCOPED_LOCK(execution->m_BindMutex);
        dmThread::SetTlsValue(execution->m_OwnerKey, 0);
        execution->m_RenderAttached = false;
    }

    bool IsRenderExecutionOwner(HRenderExecution execution, ExecutionOwner owner)
    {
        void* token = dmThread::GetTlsValue(execution->m_OwnerKey);
        if (owner == EXECUTION_OWNER_GAME)
            return token == &execution->m_GameToken;
        if (owner == EXECUTION_OWNER_RENDER || owner == EXECUTION_OWNER_GRAPHICS)
            return token == &execution->m_RenderToken || (execution->m_Policy == EXECUTION_POLICY_INLINE && token == &execution->m_GameToken);
        return false;
    }

    void AssertRenderExecutionOwner(HRenderExecution execution, ExecutionOwner owner)
    {
        assert(IsRenderExecutionOwner(execution, owner));
    }

    void DeleteRenderExecution(HRenderExecution execution)
    {
        AssertRenderExecutionOwner(execution, EXECUTION_OWNER_GAME);
        assert(!execution->m_RenderAttached);
        dmThread::SetTlsValue(execution->m_OwnerKey, 0);
        dmThread::FreeTls(execution->m_OwnerKey);
        dmMutex::Delete(execution->m_BindMutex);
        delete execution;
    }
}
