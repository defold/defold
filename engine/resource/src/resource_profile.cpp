
#include <dlib/dlib.h>
#include <dlib/array.h>
#include <dlib/mutex.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include "resource.h"
#include "resource_private.h"

namespace dmResource
{
    static const uint32_t g_MaxSnapshots = 20;
    static const uint32_t g_StringCacheGrowSize = 4096;
    static const char* g_HttpResponseTag = "/resources";

    struct SnapshotData
    {
        dmhash_t        m_ResourceId;       // Used to x-reference to the resource
        uint32_t        m_Size;             // The size of the object, in bytes
        uint32_t        m_ReferenceCount;   // Reference count of the object
        uint16_t        m_TypeStringIndex;  // Index to Snapshot m_TypeStrings array
        dmArray<dmhash_t> m_SubResourceIds; // Array of resource id's for subcomponents, used for detail information and x-referencing
    };

    struct Snapshot
    {
        dmhash_t        m_Tag;              // Id of the snapshot
        dmArray<SnapshotData> m_Data;
        dmArray<char>   m_NameStrings;      // Array of null terminated name strings, same order as items in m_Data
        dmArray<char>   m_TypeStringData;   // Array of null terminated type strings, indexed by m_TypeStrings (ptr)
        dmArray<char*>  m_TypeStrings;
    };

    static struct Profiler
    {
        Profiler() : m_Factory(0)
        {
            m_Mutex = dmMutex::New();
        }

        ~Profiler()
        {
            ProfilerReset();
            dmMutex::Delete(m_Mutex);
        }

        dmArray<dmhash_t>       m_SubResourceCache;
        dmArray<const char*>    m_NameStringCache;
        uint32_t                m_NameStringCacheDataSize;
        dmArray<Snapshot*>      m_Snapshots;
        dmMutex::Mutex          m_Mutex;
        SResourceFactory*       m_Factory;
    } g_Profiler;

    static dmProfile::ProfileExtensionResult HttpRequestCallback(void* context, const dmHttpServer::Request* request);

    void ProfilerInitialize(const HFactory factory)
    {
        if (!dLib::IsDebugMode())
            return;
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        assert(g_Profiler.m_Factory == 0);  // We support only one factory to be registered (this is by engine design, only one factory is existing at any one time)
        if(!dmProfile::RegisterProfileExtension(&g_Profiler, g_HttpResponseTag, HttpRequestCallback))
        {
            return;
        }
        g_Profiler.m_Factory = factory;
        g_Profiler.m_Snapshots.SetCapacity(g_MaxSnapshots);
        g_Profiler.m_NameStringCache.SetCapacity(g_StringCacheGrowSize);
        g_Profiler.m_SubResourceCache.SetCapacity(32);
    }

    void ProfilerFinalize(const HFactory factory)
    {
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        if(g_Profiler.m_Factory)
        {
            assert(g_Profiler.m_Factory == factory);
            dmProfile::UnregisterProfileExtension(&g_Profiler);
            ProfilerReset();
            g_Profiler.m_Factory = 0;
        }
    }

    static inline void PreparseResourceCallback(Snapshot* snapshot, const dmhash_t* key, SResourceDescriptor* value)
    {
        if(g_Profiler.m_NameStringCache.Full())
        {
            g_Profiler.m_NameStringCache.OffsetCapacity(g_StringCacheGrowSize);
        }
        const char* str = dmHashReverseSafe64(value->m_NameHash);
        g_Profiler.m_NameStringCache.Push(str);
        g_Profiler.m_NameStringCacheDataSize +=  strlen(str)+1;
    }

    static inline void ParseResourceCallback(Snapshot* snapshot, const dmhash_t* key, SResourceDescriptor* value)
    {
        uint32_t index = snapshot->m_Data.Size();
        snapshot->m_Data.SetSize(index+1);
        SnapshotData& data = snapshot->m_Data[index];
        new (&data.m_SubResourceIds) dmArray<dmhash_t>();
        data.m_ResourceId = value->m_NameHash;
        data.m_Size = value->m_ResourceSize;
        data.m_ReferenceCount = value->m_ReferenceCount;
        data.m_TypeStringIndex = ((uintptr_t)value->m_ResourceType - (uintptr_t)g_Profiler.m_Factory->m_ResourceTypes)/sizeof(SResourceType);
        SResourceType *type = (SResourceType*) value->m_ResourceType;
        if(type->m_GetInfoFunction)
        {
            ResourceGetInfoParams info;
            info.m_Factory = g_Profiler.m_Factory;
            info.m_Resource = value;
            info.m_SubResourceIds = &data.m_SubResourceIds;
            if(type->m_GetInfoFunction(info)!=dmResource::RESULT_OK)
            {
                data.m_SubResourceIds.SetCapacity(0);
            }
        }
    }

    void ProfilerSnapshot(dmhash_t tag)
    {
        if ((!dLib::IsDebugMode()) || (g_Profiler.m_Factory == 0))
            return;

        // create snapshots and add to array of existing of snapshots
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        if(g_Profiler.m_Snapshots.Full())
        {
            dmLogError("Unable to snapshot resources, buffer full. Max %d instances.", g_Profiler.m_Snapshots.Capacity());
            return;
        }

        g_Profiler.m_Snapshots.Push(new Snapshot);
        Snapshot& snapshot = *g_Profiler.m_Snapshots.Back();

        // tag
        snapshot.m_Tag = tag;

        // preparse to setup buffer sizes, typestrings and namestring cache/size. Also lock resource loading during snapshot
        SResourceFactory& factory = *g_Profiler.m_Factory;
        dmMutex::Lock(factory.m_LoadMutex);
        g_Profiler.m_NameStringCacheDataSize = 0;
        uint32_t typestrings_size = 0;
        uint32_t typestrings_count = factory.m_ResourceTypesCount;
        for(uint32_t i = 0; i < typestrings_count; ++i)
        {
            typestrings_size += strlen(factory.m_ResourceTypes[i].m_Extension)+1;
        }
        factory.m_Resources->Iterate(PreparseResourceCallback, &snapshot);
        snapshot.m_Data.SetCapacity(g_Profiler.m_NameStringCache.Size());
        snapshot.m_Data.SetSize(0);

        // parse factories and setup typestrings
        if(snapshot.m_TypeStrings.Capacity() < typestrings_count)
        {
            snapshot.m_TypeStrings.SetCapacity(typestrings_count);
        }
        snapshot.m_TypeStrings.SetSize(typestrings_count);
        if(snapshot.m_TypeStringData.Capacity() < typestrings_size)
        {
            snapshot.m_TypeStringData.SetCapacity(typestrings_size);
        }
        snapshot.m_TypeStringData.SetSize(typestrings_size);
        char* string_dst = snapshot.m_TypeStringData.Begin();
        for(uint32_t i = 0; i < typestrings_count; ++i)
        {
            snapshot.m_TypeStrings[i] = string_dst;
            const char* string_src = factory.m_ResourceTypes[i].m_Extension;
            while(*string_src) *(string_dst++) = *(string_src++);
            *(string_dst++) = 0;
        }

        // parse factories and setup snapshot data
        factory.m_Resources->Iterate(ParseResourceCallback, &snapshot);

        // transform string list into contious array
        dmArray<const char*>& string_cache = g_Profiler.m_NameStringCache;
        snapshot.m_NameStrings.SetCapacity(g_Profiler.m_NameStringCacheDataSize);
        snapshot.m_NameStrings.SetSize(g_Profiler.m_NameStringCacheDataSize);
        string_dst = snapshot.m_NameStrings.Begin();
        for(uint32_t i = 0; i < string_cache.Size(); ++i)
        {
            const char* string_src = string_cache[i];
            while(*string_src) *(string_dst++) = *(string_src++);
            *(string_dst++) = 0;
        }
        assert(string_dst == snapshot.m_NameStrings.End());
        g_Profiler.m_NameStringCache.SetSize(0);

        dmMutex::Unlock(g_Profiler.m_Factory->m_LoadMutex);
    }

    void ProfilerReset()
    {
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        for(uint32_t i = 0; i < g_Profiler.m_Snapshots.Size(); ++i)
        {
            Snapshot& snapshot = *g_Profiler.m_Snapshots[i];
            for(uint32_t j = 0; j < snapshot.m_Data.Size(); ++j)
            {
                snapshot.m_Data[j].m_SubResourceIds.~dmArray<dmhash_t>();
            }
            delete g_Profiler.m_Snapshots[i];
        }
        g_Profiler.m_Snapshots.SetSize(0);
    }

    uint32_t IterateProfilerSnapshot(void* context, void (*call_back)(void* context,  const IterateProfilerSnapshotData* data))
    {
        if(g_Profiler.m_Snapshots.Empty())
            return 0;
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        uint32_t total_items = 0;
        for(uint32_t i = 0; i < g_Profiler.m_Snapshots.Size(); ++i)
        {
            Snapshot& snapshot = *g_Profiler.m_Snapshots[i];
            total_items += snapshot.m_Data.Size();
            if(call_back)
            {
                const char* name_ptr = snapshot.m_NameStrings.Begin();
                for(uint32_t j = 0; j < snapshot.m_Data.Size(); ++j)
                {
                    SnapshotData& data = snapshot.m_Data[j];
                    IterateProfilerSnapshotData callback_data;
                    callback_data.m_Tag = snapshot.m_Tag;
                    callback_data.m_TypeName = snapshot.m_TypeStrings[data.m_TypeStringIndex];
                    callback_data.m_Size = data.m_Size;
                    callback_data.m_ReferenceCount = data.m_ReferenceCount;
                    callback_data.m_SnapshotIndex = i;
                    callback_data.m_ResourceId = data.m_ResourceId;
                    callback_data.m_SubResourceIds = &data.m_SubResourceIds;
                    call_back(context, &callback_data);
                    name_ptr += strlen(name_ptr) + 1;
                }
            }
        }
        return total_items;
    }

#define SEND_LOG_RETURN(data, size) \
    r = dmHttpServer::Send(request, data, size); \
    if (r != dmHttpServer::RESULT_OK)\
    {\
        dmLogWarning("Unexpected http-server when transmitting profile data (%d)", r);\
        return dmProfile::PROFILE_EXTENSION_RESULT_TRANSMISSION_FAILED;\
    }

    static dmProfile::ProfileExtensionResult HttpRequestCallback(void* context, const dmHttpServer::Request* request)
    {
        static dmhash_t def_tag = dmHashString64("Default");
        ProfilerSnapshot(def_tag);

        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        uint32_t count;
        uint16_t str_len;
        dmHttpServer::Result r;

        // type header
        SEND_LOG_RETURN("RESS", 4);

        // send snapshot count
        count = g_Profiler.m_Snapshots.Size();
        SEND_LOG_RETURN(&count, sizeof(count))

        // send snapshot(s)
        for(uint32_t i = 0; i < g_Profiler.m_Snapshots.Size(); ++i)
        {
            Snapshot& snapshot = *g_Profiler.m_Snapshots[i];

            // send snapshot index
            SEND_LOG_RETURN(&i, 4);

            // send snapshot tag string
            const char* tag_str = dmHashReverseSafe64(snapshot.m_Tag);
            str_len = strlen(tag_str) + 1;
            SEND_LOG_RETURN(&str_len, 2);
            SEND_LOG_RETURN(tag_str, str_len);

            // send namestrings
            count = snapshot.m_Data.Size();
            SEND_LOG_RETURN(&count, 4)
            const char* str = snapshot.m_NameStrings.Begin();
            for(uint32_t j = 0; j < count; ++j)
            {
                str_len = strlen(str);
                SEND_LOG_RETURN(&str_len, 2);
                SEND_LOG_RETURN(str, str_len);
                str += str_len+1;
            }

            // send typestrings
            count = snapshot.m_TypeStrings.Size();
            SEND_LOG_RETURN(&count, 4)
            str = snapshot.m_TypeStringData.Begin();
            for(uint32_t j = 0; j < count; ++j)
            {
                str_len = strlen(str);
                SEND_LOG_RETURN(&str_len, 2);
                SEND_LOG_RETURN(str, str_len);
                str += str_len+1;
            }

            // send snapshot data
            count = snapshot.m_Data.Size();
            SEND_LOG_RETURN(&count, 4)
            for(uint32_t j = 0; j < count; ++j)
            {
                SnapshotData& data = snapshot.m_Data[j];
                SEND_LOG_RETURN(&data.m_ResourceId, 8);
                SEND_LOG_RETURN(&data.m_Size, 4);
                SEND_LOG_RETURN(&data.m_ReferenceCount, 4);
                SEND_LOG_RETURN(&data.m_TypeStringIndex, 2);


                // send unique subresources only
                if(data.m_SubResourceIds.Size())
                {
                    dmArray<dmhash_t>& src = g_Profiler.m_SubResourceCache;
                    src.SetSize(0);
                    if(src.Capacity() < data.m_SubResourceIds.Size())
                    {
                        src.SetCapacity(data.m_SubResourceIds.Size());
                    }
                    for(uint32_t k = 0; k < data.m_SubResourceIds.Size(); ++k)
                    {
                        uint32_t l = 0;
                        uint32_t c = src.Size();
                        for(; l < c; ++l)
                            if(src[l] == data.m_SubResourceIds[k])
                                break;
                        if(l == c)
                            src.Push(data.m_SubResourceIds[k]);
                    }
                    uint32_t sc = src.Size();
                    SEND_LOG_RETURN(&sc, 4)
                    for(uint32_t k = 0; k < sc; ++k)
                    {
                        SEND_LOG_RETURN(src.Begin()+k, 8);
                    }
                }
                else
                {
                    uint32_t sc = 0;
                    SEND_LOG_RETURN(&sc, 4);
                }
            }

        }

        ProfilerReset();
        return dmProfile::PROFILE_EXTENSION_RESULT_OK;
    }

}
