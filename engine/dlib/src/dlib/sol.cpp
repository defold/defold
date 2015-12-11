#include "sol.h"

#include <sol/runtime.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <string.h>
#include <assert.h>

namespace dmSol
{
    struct HandleData
    {
        uint16_t version;
        void* ptr;
        void* type;
    };

    struct Proxy
    {
        void* m_Ptr;
    };

    static HandleData* g_Handles;
    static uint32_t g_MaxHandles;
    static dmArray<uint32_t> g_FreeHandles;
    static GarbageCollectionInfo g_GcStartInfo;

    void Initialize(uint32_t max_handles)
    {
        assert(max_handles < 0x10000);

        g_Handles = new HandleData[max_handles];
        g_MaxHandles = max_handles;
        memset(g_Handles, 0x00, max_handles * sizeof(HandleData));

        g_FreeHandles.SetCapacity(max_handles);
        for (uint32_t i=0;i!=max_handles;i++)
        {
            g_FreeHandles.Push(max_handles-1-i);
        }

        runtime_init();
        runtime_gc();
        g_GcStartInfo = runtime_gc_info();
    }

    void Finalize()
    {
        delete g_Handles;
        runtime_destroy();
    }

    void FinalizeWithCheck()
    {
        runtime_gc();
        GarbageCollectionInfo gc_stop = runtime_gc_info();
        if (gc_stop.live_count != g_GcStartInfo.live_count)
        {
            dmLogError("Live count on exit:%lld start:%lld", gc_stop.live_count, g_GcStartInfo.live_count);
        }
        assert(gc_stop.live_count == g_GcStartInfo.live_count);
        Finalize();
    }

    Handle MakeHandle(void *ptr, void* type)
    {
        assert(g_FreeHandles.Size() > 0);
        uint32_t idx = g_FreeHandles.Back();
        g_FreeHandles.SetSize(g_FreeHandles.Size()-1);

        g_Handles[idx].ptr = ptr;
        g_Handles[idx].type = type;
        g_Handles[idx].version++;

        return (idx << 16) | g_Handles[idx].version;
    }

    void* GetHandle(Handle handle, void* type)
    {
        uint32_t idx = (handle >> 16);
        assert(idx < g_MaxHandles);
        if (g_Handles[idx].version != (handle & 0xffff) || g_Handles[idx].type != type)
        {
            dmLogWarning("Trying to access stale handle.");
            return 0;
        }
        return g_Handles[idx].ptr;
    }

    bool IsHandleValid(Handle handle, void* type)
    {
        uint32_t idx = (handle >> 16);
        assert(idx < g_MaxHandles);
        return g_Handles[idx].version == (handle & 0xffff) && g_Handles[idx].type == type;
    }

    void FreeHandle(Handle handle)
    {
        uint32_t idx = (handle >> 16);
        assert(idx < g_MaxHandles);
        assert(g_Handles[idx].version == (handle & 0xffff));
        g_Handles[idx].version++;
        g_Handles[idx].ptr = 0;
        g_FreeHandles.Push(idx);
    }

    HProxy NewProxy(void* ptr)
    {
        Proxy* p = (Proxy*) runtime_alloc_array(sizeof(Proxy), 1);
        p->m_Ptr = ptr;
        return p;
    }

    void DeleteProxy(HProxy proxy)
    {
        proxy->m_Ptr = 0;
        runtime_unpin(proxy);
    }

    void* ProxyGet(HProxy proxy)
    {
        return proxy->m_Ptr;
    }

    const char* StrDup(const char *str)
    {
        if (!str)
        {
            return str;
        }
        uint32_t len = strlen(str);
        void* out = runtime_alloc_array(1, len + 1);
        memcpy(out, str, len + 1);
        return (const char*) out;
    }

    void SafePin(::Any a)
    {
        void* value = (void*)reflect_get_any_value(a);
        Type* type = reflect_get_any_type(a);
        if (value && type && type->referenced_type)
        {
            runtime_pin(value);
            return;
        }
    }

    void SafeUnpin(::Any a)
    {
        void* value = (void*)reflect_get_any_value(a);
        Type* type = reflect_get_any_type(a);
        if (value && type && type->referenced_type)
        {
            runtime_unpin(value);
            return;
        }
    }

    size_t SizeOf(::Any ptr)
    {
        Type* type = reflect_get_any_type(ptr);
        if (!type || !type->referenced_type)
        {
            dmLogWarning("SizeOf: type is not a pointer.");
            return 0;
        }

        StructType* struct_type = type->referenced_type->struct_type;
        if (!struct_type)
        {
            dmLogWarning("SizeOf: referenced type is not a struct");
        }

        uint32_t a = (struct_type->size % struct_type->alignment);
        if (!a)
        {
            return struct_type->size;
        }

        dmLogError("struct_type->size = %d   struct_type->alignment = %d", struct_type->size, struct_type->alignment);
        return struct_type->size + (struct_type->alignment - a);
    }
}
