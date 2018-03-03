#ifndef RESOURCE_PRIVATE_H
#define RESOURCE_PRIVATE_H

#include <ddf/ddf.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/uri.h>
#include <dlib/http_client.h>
#include "resource_archive.h"
#include "resource.h"

// Internal API that preloader needs to use.

namespace dmResource
{
    // This is both for the total resource path, ie m_UriParts.X concatenated with relative path
    const uint32_t RESOURCE_PATH_MAX = 1024;

    const uint32_t MAX_RESOURCE_TYPES = 128;

    struct SResourceType
    {
        SResourceType()
        {
            memset(this, 0, sizeof(*this));
        }
        const char*         m_Extension;
        void*               m_Context;
        FResourcePreload    m_PreloadFunction;
        FResourceCreate     m_CreateFunction;
        FResourcePostCreate m_PostCreateFunction;
        FResourceDestroy    m_DestroyFunction;
        FResourceRecreate   m_RecreateFunction;
        FResourceDuplicate  m_DuplicateFunction;
        FResourceGetInfo    m_GetInfoFunction;
    };

    struct ResourceReloadedCallbackPair
    {
        ResourceReloadedCallback    m_Callback;
        void*                       m_UserData;
    };

    typedef dmArray<char> LoadBufferType;

    struct SResourceFactory
    {
        // TODO: Arg... budget. Two hash-maps. Really necessary?
        dmHashTable<uint64_t, SResourceDescriptor>*  m_Resources;
        dmHashTable<uintptr_t, uint64_t>*            m_ResourceToHash;
        // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
        // Used for reloading of resources
        dmHashTable<uint64_t, const char*>*          m_ResourceHashToFilename;
        // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
        dmArray<ResourceReloadedCallbackPair>*       m_ResourceReloadedCallbacks;
        SResourceType                                m_ResourceTypes[MAX_RESOURCE_TYPES];
        uint32_t                                     m_ResourceTypesCount;

        // Guard for anything that touches anything that could be shared
        // with GetRaw (used for async threaded loading). Liveupdate, HttpClient, m_Buffer
        // m_BuiltinsArchive and m_Archive
        dmMutex::Mutex                               m_LoadMutex;

        // dmResource::Get recursion depth
        uint32_t                                     m_RecursionDepth;
        // List of resources currently in dmResource::Get call-stack
        dmArray<const char*>                         m_GetResourceStack;

        dmMessage::HSocket                           m_Socket;

        dmURI::Parts                                 m_UriParts;
        dmHttpClient::HClient                        m_HttpClient;
        dmHttpCache::HCache                          m_HttpCache;
        LoadBufferType*                              m_HttpBuffer;

        dmArray<char>                                m_Buffer;

        // HTTP related state
        // Total number bytes loaded in current GET-request
        int32_t                                      m_HttpContentLength;
        uint32_t                                     m_HttpTotalBytesStreamed;
        int                                          m_HttpStatus;
        Result                                       m_HttpFactoryResult;

        // Manifest and archive for builtin resources
        dmLiveUpdateDDF::ManifestFile*               m_BuiltinsManifest;
        dmResourceArchive::HArchiveIndexContainer    m_BuiltinsArchiveContainer;

        // Resource manifest
        Manifest*                                    m_Manifest;
        void*                                        m_ArchiveMountInfo;

        // Shared resources
        uint32_t                                    m_NonSharedCount; // a running number, helping id the potentially non shared assets
    };

    struct SResourceDescriptor;

    Result CheckSuppliedResourcePath(const char* name);

    // load with default internal buffer and its management, returns buffer ptr in 'buffer'
    Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size);
    // load with own buffer
    Result DoLoadResource(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer);

    Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, SResourceDescriptor* descriptor);
    void GetCanonicalPath(HFactory factory, const char* relative_dir, char* buf);

    SResourceDescriptor* GetByHash(HFactory factory, uint64_t canonical_path_hash);
    SResourceType* FindResourceType(SResourceFactory* factory, const char* extension);
    uint32_t GetRefCount(HFactory factory, void* resource);
    uint32_t GetRefCount(HFactory factory, dmhash_t identifier);

    void ProfilerInitialize(const HFactory factory);
    void ProfilerFinalize(const HFactory factory);

    // Profiler snapshot iteration, for tests
    struct IterateProfilerSnapshotData
    {
        dmhash_t    m_ResourceId;
        dmhash_t    m_Tag;
        const char* m_TypeName;
        dmArray<dmhash_t>* m_SubResourceIds;
        uint32_t    m_Size;
        uint32_t    m_ReferenceCount;
        uint32_t    m_SnapshotIndex;
    };
    // Iterate profiler snapshot data items. Set call_back function to 0 to get total items
    uint32_t IterateProfilerSnapshot(void* context, void (*call_back)(void* context, const IterateProfilerSnapshotData* data));

    struct PreloadRequest;
    struct PreloadHintInfo
    {
        HPreloader m_Preloader;
        int32_t m_Parent;
    };
}

#endif
