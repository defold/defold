
#include <dlib/dlib.h>
#include <dlib/array.h>
#include <dlib/mutex.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include "gameobject.h"
#include "gameobject_private.h"

namespace dmGameObject
{
    static const uint32_t g_MaxSnapshots = 20;
    static const uint32_t g_StringCacheGrowSize = 1024;
    static const char* g_HttpResponseTag = "/gameobjects";

    enum SnapshotDataTypeFlags
    {
        SNAPSHOT_DATA_TYPE_GO_BONE      = (1 << 0),
        SNAPSHOT_DATA_TYPE_GO_GENERATED = (1 << 1)
    };

    struct SnapshotData
    {
        dmhash_t    m_ResourceId;       // Used to x-reference to the resource
        dmhash_t    m_Id;               // The name of the object
        uint32_t    m_Parent;           // Parent index to snapshot item array
        uint32_t    m_Flags;            // SnapshotDataTypeFlags or 0
        uint16_t    m_TypeStringIndex;  // Index to Snapshot m_TypeStrings array
    };

    struct Snapshot
    {
        dmhash_t        m_Tag;              // Id of the snapshot
        dmArray<struct SnapshotData> m_Data;
        dmArray<char>   m_NameStrings;      // Array of null terminated name strings, same order as items in m_Data
        dmArray<char>   m_TypeStringData;   // Array of null terminated type strings, indexed by m_TypeStrings (ptr)
        dmArray<char*>  m_TypeStrings;
        uint32_t        m_CollectionTypeStringIndex;    // Index of collection type string in m_TypeStrings
        uint32_t        m_GameObjectTypeStringIndex;    // Index of go type string in m_TypeStrings
    };

    static struct Profiler
    {
        Profiler() : m_Registry(0)
        {
            m_Mutex = dmMutex::New();
        }

        ~Profiler()
        {
            ProfilerReset();
            dmMutex::Delete(m_Mutex);
        }

        dmArray<const char*>    m_NameStringCache;
        uint32_t                m_NameStringCacheDataSize;
        dmArray<Snapshot*>      m_Snapshots;
        dmMutex::Mutex          m_Mutex;
        Register*               m_Registry;
    } g_Profiler;

    static dmProfile::ProfileExtensionResult HttpRequestCallback(void* context, const dmHttpServer::Request* request);

    void ProfilerInitialize(const HRegister regist)
    {
        if (!dLib::IsDebugMode())
            return;
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        assert(g_Profiler.m_Registry == 0); // We support only one registry to be registered (this is by engine design, only one register is existing at any one time)
        if(!dmProfile::RegisterProfileExtension(&g_Profiler, g_HttpResponseTag, HttpRequestCallback))
        {
            return;
        }
        g_Profiler.m_Registry = regist;
        g_Profiler.m_Snapshots.SetCapacity(g_MaxSnapshots);
        g_Profiler.m_NameStringCache.SetCapacity(g_StringCacheGrowSize);
    }

    void ProfilerFinalize(const HRegister regist)
    {
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        if(g_Profiler.m_Registry)
        {
            assert(g_Profiler.m_Registry == regist);
            dmProfile::UnregisterProfileExtension(&g_Profiler);
            ProfilerReset();
            g_Profiler.m_Registry = 0;
        }
    }

    static inline void StringCacheAdd(const char* str)
    {
        if(g_Profiler.m_NameStringCache.Full())
        {
            g_Profiler.m_NameStringCache.OffsetCapacity(g_StringCacheGrowSize);
        }
        g_Profiler.m_NameStringCache.Push(str);
        int32_t str_len = strlen(str)+1;
        g_Profiler.m_NameStringCacheDataSize +=  str_len;
    }

    static inline void PreParseInstances(const Collection* collection, const Instance* instance)
    {
        if(instance->m_ToBeDeleted)
            return;
        StringCacheAdd(dmHashReverseSafe64(instance->m_Identifier));
        Prototype* prototype = instance->m_Prototype;

        for (uint32_t k = 0; k < prototype->m_Components.Size(); ++k)
        {
            Prototype::Component* component = &prototype->m_Components[k];
            StringCacheAdd(dmHashReverseSafe64(component->m_Id));
        }
        uint32_t childIndex = instance->m_FirstChildIndex;
        while (childIndex != INVALID_INSTANCE_INDEX)
        {
            Instance* child = collection->m_Instances[childIndex];
            assert(child->m_Parent == instance->m_Index);
            childIndex = child->m_SiblingIndex;
            PreParseInstances(collection, child);
        }
    }

    static inline uint32_t AddData(Snapshot& snapshot, dmhash_t resource_id, dmhash_t id, uint32_t parent, uint32_t flags, uint16_t type)
    {
        uint32_t index = snapshot.m_Data.Size();
        snapshot.m_Data.SetSize(index+1);
        SnapshotData& data = snapshot.m_Data[index];
        data.m_ResourceId = resource_id;
        data.m_Id = id;
        data.m_Parent = parent;
        data.m_Flags = flags;
        data.m_TypeStringIndex = type;
        return index;
    }

    static void ParseInstances(const Collection* collection, const Instance* instance, uint32_t parent, Snapshot& snapshot)
    {
        if(instance->m_ToBeDeleted)
            return;
        dmhash_t resource_id = 0;
        dmResource::GetPath(collection->m_Factory, instance->m_Prototype, &resource_id);
        uint32_t flags = (instance->m_Bone ? SNAPSHOT_DATA_TYPE_GO_BONE : 0) | (instance->m_Generated ? SNAPSHOT_DATA_TYPE_GO_GENERATED : 0);
        parent = AddData(snapshot, resource_id, instance->m_Identifier, parent, flags, snapshot.m_GameObjectTypeStringIndex);

        // components
        Prototype* prototype = instance->m_Prototype;
        for (uint32_t k = 0; k < prototype->m_Components.Size(); ++k)
        {
            Prototype::Component* component = &prototype->m_Components[k];
            AddData(snapshot, component->m_ResourceId, component->m_Id, parent, 0, component->m_TypeIndex);
        }

        // parse child hierarchy recursively.
        uint32_t childIndex = instance->m_FirstChildIndex;
        while (childIndex != INVALID_INSTANCE_INDEX)
        {
            Instance* child = collection->m_Instances[childIndex];
            assert(child->m_Parent == instance->m_Index);
            childIndex = child->m_SiblingIndex;
            ParseInstances(collection, child, parent, snapshot);
        }
    }

    void ProfilerSnapshot(dmhash_t tag)
    {
        if (!dLib::IsDebugMode())
            return;

        // create snapshots and add to array of existing of snapshots
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        if(g_Profiler.m_Snapshots.Full())
        {
            dmLogError("Unable to snapshot collections, buffer full. Max %d instances.", g_Profiler.m_Snapshots.Capacity());
            return;
        }
        g_Profiler.m_Snapshots.Push(new Snapshot);
        Snapshot& snapshot = *g_Profiler.m_Snapshots.Back();

        // tag
        snapshot.m_Tag = tag;

        // preparse to setup buffer sizes and namestring cache
        Register& regist = *g_Profiler.m_Registry;
        dmMutex::Lock(regist.m_Mutex);
        g_Profiler.m_NameStringCacheDataSize = 0;
        uint32_t typestrings_size = 0;
        uint32_t typestrings_count = regist.m_ComponentTypeCount;
        for(uint32_t i = 0; i < typestrings_count; ++i)
        {
            typestrings_size += strlen(regist.m_ComponentTypes[i].m_Name)+1;
        }
        for(uint32_t i = 0; i < regist.m_Collections.Size(); ++i)
        {
            const Collection* collection = regist.m_Collections[i];
            dmMutex::Lock(collection->m_Mutex);
            if(!collection->m_ToBeDeleted)
            {
                StringCacheAdd(dmHashReverseSafe64(collection->m_NameHash));
                const dmArray<uint16_t>& root_level = collection->m_LevelIndices[0];
                for (uint32_t j = 0; j < root_level.Size(); ++j)
                {
                    PreParseInstances(collection, collection->m_Instances[root_level[j]]);
                }
            }
        }
        snapshot.m_Data.SetCapacity(g_Profiler.m_NameStringCache.Size());
        snapshot.m_Data.SetSize(0);

        // Add internal types (not components)
        const char* collection_type_string = "collectionc";
        snapshot.m_CollectionTypeStringIndex = typestrings_count++;
        typestrings_size += strlen(collection_type_string)+1;
        const char* gameobject_type_string = "goc";
        snapshot.m_GameObjectTypeStringIndex = typestrings_count++;
        typestrings_size += strlen(gameobject_type_string)+1;

        // parse registry and setup component typestrings
        if(snapshot.m_TypeStrings.Capacity() < typestrings_count)
        {
            snapshot.m_TypeStrings.SetCapacity(typestrings_count);
        }
        snapshot.m_TypeStrings.SetSize(0);
        if(snapshot.m_TypeStringData.Capacity() < typestrings_size)
        {
            snapshot.m_TypeStringData.SetCapacity(typestrings_size);
        }
        snapshot.m_TypeStringData.SetSize(typestrings_size);
        char* string_dst = snapshot.m_TypeStringData.Begin();
        for(uint32_t i = 0; i < regist.m_ComponentTypeCount; ++i)
        {
            snapshot.m_TypeStrings.Push(string_dst);
            const char* string_src = regist.m_ComponentTypes[i].m_Name;
            while(*string_src) *(string_dst++) = *(string_src++);
            *(string_dst++) = 0;
        }

        // setup internal typestrings
        snapshot.m_TypeStrings.Push(string_dst);
        string_dst += dmStrlCpy(string_dst, collection_type_string, snapshot.m_TypeStringData.End() - string_dst) + 1;
        snapshot.m_TypeStrings.Push(string_dst);
        string_dst += dmStrlCpy(string_dst, gameobject_type_string, snapshot.m_TypeStringData.End() - string_dst) + 1;

        // parse collections and setup snapshot data
        for(uint32_t i = 0; i < regist.m_Collections.Size(); ++i)
        {
            const Collection* collection = regist.m_Collections[i];
            if(!collection->m_ToBeDeleted)
            {
                dmhash_t resource_id = 0;
                dmResource::GetPath(collection->m_Factory, collection, &resource_id);
                uint32_t parent = AddData(snapshot, resource_id, collection->m_NameHash, INVALID_INSTANCE_POOL_INDEX, 0, snapshot.m_CollectionTypeStringIndex);
                const dmArray<uint16_t>& root_level = collection->m_LevelIndices[0];
                for (uint32_t j = 0; j < root_level.Size(); ++j)
                {
                    ParseInstances(collection, collection->m_Instances[root_level[j]], parent, snapshot);
                }
            }
        }

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

        // release locks for registry, collections and profiler
        for(uint32_t i = 0; i < regist.m_Collections.Size(); ++i)
        {
            dmMutex::Unlock(regist.m_Collections[i]->m_Mutex);
        }
        dmMutex::Unlock(regist.m_Mutex);
    }

    void ProfilerReset()
    {
        DM_MUTEX_SCOPED_LOCK(g_Profiler.m_Mutex);
        for(uint32_t i = 0; i < g_Profiler.m_Snapshots.Size(); ++i)
        {
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
                    callback_data.m_ResourceId = data.m_ResourceId;
                    callback_data.m_Id = name_ptr;
                    callback_data.m_TypeName = snapshot.m_TypeStrings[data.m_TypeStringIndex];
                    callback_data.m_SnapshotIndex = i;
                    callback_data.m_ParentIndex = data.m_Parent;
                    callback_data.m_Flags = data.m_Flags;
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
        SEND_LOG_RETURN("GOBJ", 4);

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
            for(uint32_t j = 0; j < snapshot.m_TypeStrings.Size(); ++j)
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
                SEND_LOG_RETURN(&data.m_Id, 8);
                SEND_LOG_RETURN(&data.m_Parent, 4);
                SEND_LOG_RETURN(&data.m_Flags, 4);
                SEND_LOG_RETURN(&data.m_TypeStringIndex, 2);
            }
        }

        ProfilerReset();
        return dmProfile::PROFILE_EXTENSION_RESULT_OK;
    }

}
