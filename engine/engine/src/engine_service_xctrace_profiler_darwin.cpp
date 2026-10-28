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

#include "engine_service_private.h"

#include <dlib/array.h>
#include <dlib/mutex.h>
#include <dlib/profile.h>
#include <dlib/sys.h>
#include <dlib/thread.h>

#include <os/log.h>
#include <os/signpost.h>
#include <string.h>

namespace dmEngineService
{
    struct NativeProperty
    {
        const char*          m_Name;
        ProfilePropertyValue m_Value;
        ProfilePropertyValue m_Default;
        ProfilePropertyType  m_Type;
        ProfileIdx           m_Parent;
        uint32_t             m_Flags;
    };

    struct NativeThread
    {
        NativeThread*            m_Next;
        dmArray<os_signpost_id_t> m_Scopes;
        os_signpost_id_t          m_Frame;
    };

    struct XcTraceProfiler
    {
        os_log_t         m_Log;
        dmMutex::HMutex  m_Mutex;
        dmThread::TlsKey m_ThreadKey;
        // Thread states are owned by the session, including states of exited threads.
        NativeThread*  m_Threads;
        uint32_t       m_PropertyCount;
        NativeProperty m_Properties[PROFILER_MAX_NUM_PROPERTIES];
    };

    static ProfileListener g_NativeListener;
    static bool            g_NativeRegistered = false;

    static NativeThread*   GetThread(XcTraceProfiler* xctrace)
    {
        NativeThread* thread = (NativeThread*)dmThread::GetTlsValue(xctrace->m_ThreadKey);
        if (!thread)
        {
            thread = new NativeThread;
            thread->m_Frame = OS_SIGNPOST_ID_NULL;
            thread->m_Scopes.SetCapacity(64);
            dmThread::SetTlsValue(xctrace->m_ThreadKey, thread);
            DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex);
            thread->m_Next = xctrace->m_Threads;
            xctrace->m_Threads = thread;
        }
        return thread;
    }

    static ProfileResult NativeScopeBegin(void* context, const char* name, uint64_t name_hash)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        NativeThread*    thread = GetThread(xctrace);
        if (thread->m_Scopes.Full())
            thread->m_Scopes.OffsetCapacity(64);
        os_signpost_id_t id = os_signpost_id_generate(xctrace->m_Log);
        // Keep disabled begins on the stack if recording starts inside a scope.
        thread->m_Scopes.Push(id);
        if (id != OS_SIGNPOST_ID_NULL)
            os_signpost_interval_begin(xctrace->m_Log, id, "Scope", "%{public}s", name);
        return PROFILE_RESULT_OK;
    }

    static ProfileResult NativeScopeEnd(void* context, const char* name, uint64_t name_hash)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        NativeThread*    thread = (NativeThread*)dmThread::GetTlsValue(xctrace->m_ThreadKey);
        // Registration may happen while a scope is already open.
        if (thread && !thread->m_Scopes.Empty())
        {
            os_signpost_id_t id = thread->m_Scopes.Back();
            thread->m_Scopes.Pop();
            if (id != OS_SIGNPOST_ID_NULL)
                os_signpost_interval_end(xctrace->m_Log, id, "Scope");
        }
        return PROFILE_RESULT_OK;
    }

    static ProfileResult NativeThreadName(void* context, const char* name)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        os_signpost_event_emit(xctrace->m_Log, OS_SIGNPOST_ID_EXCLUSIVE, "Thread Name", "%{public}s", name);
        return PROFILE_RESULT_OK;
    }

    static ProfileResult NativeLogText(void* context, const char* text)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        os_signpost_event_emit(xctrace->m_Log, OS_SIGNPOST_ID_EXCLUSIVE, "Log", "%{public}s", text);
        return PROFILE_RESULT_OK;
    }

    static void NativeCreateProperty(XcTraceProfiler* xctrace, const char* name, ProfileIdx idx, ProfileIdx parent, uint32_t flags, ProfilePropertyType type, ProfilePropertyValue value)
    {
        NativeProperty* property = &xctrace->m_Properties[idx];
        // Names have the lifetime of the profiler's property registry.
        property->m_Name = name;
        property->m_Type = type;
        property->m_Flags = flags;
        property->m_Parent = parent;
        property->m_Default = property->m_Value = value;
        if (xctrace->m_PropertyCount <= idx)
            xctrace->m_PropertyCount = idx + 1;
    }

    static void NativeCreateGroup(void* context, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex);
        ProfilePropertyValue value = {};
        NativeCreateProperty(xctrace, name, idx, parent, 0, PROFILE_PROPERTY_TYPE_GROUP, value);
    }

#define NATIVE_PROPERTY(TYPE, CTYPE, ENUM_TYPE) \
    static void NativeCreate##TYPE(void* context, const char* name, const char* desc, CTYPE value, uint32_t flags, ProfileIdx idx, ProfileIdx parent) \
    { \
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context; \
        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex); \
        ProfilePropertyValue v = {}; \
        v.m_##TYPE = value; \
        NativeCreateProperty(xctrace, name, idx, parent, flags, ENUM_TYPE, v); \
    } \
    static void NativeSet##TYPE(void* context, ProfileIdx idx, CTYPE value) \
    { \
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context; \
        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex); \
        xctrace->m_Properties[idx].m_Value.m_##TYPE = value; \
    }

    NATIVE_PROPERTY(Bool, int, PROFILE_PROPERTY_TYPE_BOOL)
    NATIVE_PROPERTY(S32, int32_t, PROFILE_PROPERTY_TYPE_S32)
    NATIVE_PROPERTY(U32, uint32_t, PROFILE_PROPERTY_TYPE_U32)
    NATIVE_PROPERTY(F32, float, PROFILE_PROPERTY_TYPE_F32)
    NATIVE_PROPERTY(S64, int64_t, PROFILE_PROPERTY_TYPE_S64)
    NATIVE_PROPERTY(U64, uint64_t, PROFILE_PROPERTY_TYPE_U64)
    NATIVE_PROPERTY(F64, double, PROFILE_PROPERTY_TYPE_F64)
#undef NATIVE_PROPERTY

#define NATIVE_ADD(TYPE, CTYPE) \
    static void NativeAdd##TYPE(void* context, ProfileIdx idx, CTYPE value) \
    { \
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context; \
        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex); \
        xctrace->m_Properties[idx].m_Value.m_##TYPE += value; \
    }
    NATIVE_ADD(S32, int32_t)
    NATIVE_ADD(U32, uint32_t)
    NATIVE_ADD(F32, float)
    NATIVE_ADD(S64, int64_t)
    NATIVE_ADD(U64, uint64_t)
    NATIVE_ADD(F64, double)
#undef NATIVE_ADD

    static void NativeResetProperty(XcTraceProfiler* xctrace, ProfileIdx idx)
    {
        xctrace->m_Properties[idx].m_Value = xctrace->m_Properties[idx].m_Default;
        for (uint32_t i = idx + 1; i < xctrace->m_PropertyCount; ++i)
        {
            if (xctrace->m_Properties[i].m_Parent == idx)
                NativeResetProperty(xctrace, i);
        }
    }

    static void NativeResetProperty(void* context, ProfileIdx idx)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex);
        NativeResetProperty(xctrace, idx);
    }

    static void NativeCounter(os_log_t log, const NativeProperty* property)
    {
        switch (property->m_Type)
        {
            case PROFILE_PROPERTY_TYPE_BOOL:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %d", property->m_Name, property->m_Value.m_Bool);
                break;
            case PROFILE_PROPERTY_TYPE_S32:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %d", property->m_Name, property->m_Value.m_S32);
                break;
            case PROFILE_PROPERTY_TYPE_U32:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %u", property->m_Name, property->m_Value.m_U32);
                break;
            case PROFILE_PROPERTY_TYPE_S64:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %lld", property->m_Name, (long long)property->m_Value.m_S64);
                break;
            case PROFILE_PROPERTY_TYPE_U64:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %llu", property->m_Name, (unsigned long long)property->m_Value.m_U64);
                break;
            case PROFILE_PROPERTY_TYPE_F32:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %f", property->m_Name, property->m_Value.m_F32);
                break;
            case PROFILE_PROPERTY_TYPE_F64:
                os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Counter", "%{public}s = %f", property->m_Name, property->m_Value.m_F64);
                break;
            default:
                break;
        }
    }

    static ProfileResult NativeFrameBegin(void* context)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        NativeThread*    thread = GetThread(xctrace);
        thread->m_Frame = os_signpost_id_generate(xctrace->m_Log);
        if (thread->m_Frame != OS_SIGNPOST_ID_NULL)
            os_signpost_interval_begin(xctrace->m_Log, thread->m_Frame, "Frame");
        return PROFILE_RESULT_OK;
    }

    static ProfileResult NativeFrameEnd(void* context)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        NativeThread*    thread = GetThread(xctrace);
        if (thread->m_Frame != OS_SIGNPOST_ID_NULL)
            os_signpost_interval_end(xctrace->m_Log, thread->m_Frame, "Frame");
        thread->m_Frame = OS_SIGNPOST_ID_NULL;

        DM_MUTEX_SCOPED_LOCK(xctrace->m_Mutex);
        bool enabled = os_signpost_enabled(xctrace->m_Log);
        for (uint32_t i = 0; i < xctrace->m_PropertyCount; ++i)
        {
            NativeProperty* property = &xctrace->m_Properties[i];
            if (enabled)
                NativeCounter(xctrace->m_Log, property);
            if (property->m_Flags & PROFILE_PROPERTY_FRAME_RESET)
                property->m_Value = property->m_Default;
        }
        return PROFILE_RESULT_OK;
    }

    static void* CreateXcTraceProfiler()
    {
        XcTraceProfiler* xctrace = new XcTraceProfiler;
        memset(xctrace, 0, sizeof(*xctrace));
        // Emit from startup, before Instruments has enabled dynamic subsystems.
        xctrace->m_Log = os_log_create("com.defold.profiler", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
        xctrace->m_Mutex = dmMutex::New();
        xctrace->m_ThreadKey = dmThread::AllocTls();
        return xctrace;
    }

    static void DestroyXcTraceProfiler(void* context)
    {
        XcTraceProfiler* xctrace = (XcTraceProfiler*)context;
        NativeThread*    thread = xctrace->m_Threads;
        while (thread)
        {
            NativeThread* next = thread->m_Next;
            delete thread;
            thread = next;
        }
        dmThread::FreeTls(xctrace->m_ThreadKey);
        dmMutex::Delete(xctrace->m_Mutex);
        os_release(xctrace->m_Log);
        delete xctrace;
        g_NativeListener.m_Ctx = 0;
        ProfileUnregisterProfiler("ProfilerNative");
        g_NativeRegistered = false;
    }

    bool EnableXcTraceProfiler()
    {
        if (!ProfileIsInitialized())
            return false;
        if (g_NativeRegistered)
            return true;
        memset(&g_NativeListener, 0, sizeof(g_NativeListener));
        g_NativeListener.m_Create = CreateXcTraceProfiler;
        g_NativeListener.m_Destroy = DestroyXcTraceProfiler;
        g_NativeListener.m_ScopeBegin = NativeScopeBegin;
        g_NativeListener.m_ScopeEnd = NativeScopeEnd;
        g_NativeListener.m_SetThreadName = NativeThreadName;
        g_NativeListener.m_LogText = NativeLogText;
        g_NativeListener.m_FrameBegin = NativeFrameBegin;
        g_NativeListener.m_FrameEnd = NativeFrameEnd;
        g_NativeListener.m_CreatePropertyGroup = NativeCreateGroup;
#define NATIVE_CALLBACKS(TYPE) \
    g_NativeListener.m_CreateProperty##TYPE = NativeCreate##TYPE; \
    g_NativeListener.m_PropertySet##TYPE = NativeSet##TYPE;
        NATIVE_CALLBACKS(Bool)
        NATIVE_CALLBACKS(S32)
        NATIVE_CALLBACKS(U32)
        NATIVE_CALLBACKS(F32)
        NATIVE_CALLBACKS(S64)
        NATIVE_CALLBACKS(U64)
        NATIVE_CALLBACKS(F64)
#undef NATIVE_CALLBACKS
        g_NativeListener.m_PropertyAddS32 = NativeAddS32;
        g_NativeListener.m_PropertyAddU32 = NativeAddU32;
        g_NativeListener.m_PropertyAddF32 = NativeAddF32;
        g_NativeListener.m_PropertyAddS64 = NativeAddS64;
        g_NativeListener.m_PropertyAddU64 = NativeAddU64;
        g_NativeListener.m_PropertyAddF64 = NativeAddF64;
        g_NativeListener.m_PropertyReset = NativeResetProperty;
        ProfileRegisterProfiler("ProfilerNative", &g_NativeListener);
        g_NativeRegistered = true;
        return true;
    }

    void InitializeXcTraceProfiler()
    {
#if defined(DM_PROFILE_NATIVE_ENABLED)
        bool enabled = true;
#else
        bool enabled = false;
#endif
        const char* enable = dmSys::GetEnv("DM_PROFILE_NATIVE_ENABLE");
        if (enable)
            enabled = strcmp(enable, "1") == 0;
        if (enabled)
            EnableXcTraceProfiler();
    }
} // namespace dmEngineService
