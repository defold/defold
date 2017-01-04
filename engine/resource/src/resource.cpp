#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __linux__
#include <limits.h>
#elif defined (__MACH__)
#include <sys/param.h>
#endif

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/http_client.h>
#include <dlib/http_cache.h>
#include <dlib/http_cache_verify.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <dlib/uri.h>
#include <dlib/path.h>
#include <dlib/profile.h>
#include <dlib/message.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <dlib/mutex.h>

#include "resource.h"
#include "resource_archive.h"
#include "resource_ddf.h"
#include "resource_private.h"

/*
 * TODO:
 *
 *  - Resources could be loaded twice if canonical path is different for equivalent files.
 *    We could use realpath or similar function but we want to avoid file accesses when converting
 *    a canonical path to hash value. This functionality is used in the GetDescriptor function.
 *
 *  - If GetCanonicalPath exceeds RESOURCE_PATH_MAX PATH_TOO_LONG should be returned.
 *
 *  - Handle out of resources. Eg Hashtables full.
 */

extern dmDDF::Descriptor dmLiveUpdateDDF_ManifestFile_DESCRIPTOR;

namespace dmResource
{
const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

#define RESOURCE_SOCKET_NAME "@resource"

const char* MAX_RESOURCES_KEY = "resource.max_resources";

const static uint32_t MANIFEST_MAGIC_NUMBER = 0x43cb6d06;
const static uint32_t MANIFEST_VERSION = 0x01;

struct Manifest
{
    Manifest()
    {
        memset(this, 0, sizeof(Manifest));
    }

    dmResourceArchive::HArchiveIndexContainer    m_ArchiveIndex;
    dmLiveUpdateDDF::ManifestFile*      m_DDF;
};

struct ResourceReloadedCallbackPair
{
    ResourceReloadedCallback    m_Callback;
    void*                       m_UserData;
};

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
    // with GetRaw (used for async threaded loading). HttpClient, m_Buffer
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

    Manifest*                                    m_Manifest;

    // -----------------------------
    // TODO replace these with manifest instance
    // Builtin resource archive
    dmResourceArchive::HArchive                  m_BuiltinsArchive;

    // Resource archive
    dmResourceArchive::HArchive                  m_Archive;
    void*                                        m_ArchiveMountInfo;
    void*                                        m_ArchiveMountInfo2;
};

SResourceType* FindResourceType(SResourceFactory* factory, const char* extension)
{
    for (uint32_t i = 0; i < factory->m_ResourceTypesCount; ++i)
    {
        SResourceType* rt = &factory->m_ResourceTypes[i];
        if (strcmp(extension, rt->m_Extension) == 0)
        {
            return rt;
        }
    }
    return 0;
}

// TODO: Test this...
static void GetCanonicalPath(const char* base_dir, const char* relative_dir, char* buf)
{
    DM_SNPRINTF(buf, RESOURCE_PATH_MAX, "%s/%s", base_dir, relative_dir);

    char* source = buf;
    char* dest = buf;
    char last_c = 0;
    while (*source != 0)
    {
        char c = *source;
        if (c != '/' || (c == '/' && last_c != '/'))
            *dest++ = c;

        last_c = c;
        ++source;
    }
    *dest = '\0';
}

void GetCanonicalPath(HFactory factory, const char* relative_dir, char* buf)
{
    return GetCanonicalPath(factory->m_UriParts.m_Path, relative_dir, buf);
}

Result CheckSuppliedResourcePath(const char* name)
{
    if (name[0] == 0)
    {
        dmLogError("Empty resource path");
        return RESULT_RESOURCE_NOT_FOUND;
    }
    if (name[0] != '/')
    {
        dmLogError("Resource path is not absolute (%s)", name);
        return RESULT_RESOURCE_NOT_FOUND;
    }
    return RESULT_OK;
}

void SetDefaultNewFactoryParams(struct NewFactoryParams* params)
{
    params->m_MaxResources = 1024;
    params->m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
    params->m_BuiltinsArchive = 0;
    params->m_BuiltinsArchiveSize = 0;
}

static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
{
    SResourceFactory* factory = (SResourceFactory*) user_data;
    factory->m_HttpStatus = status_code;

    if (strcmp(key, "Content-Length") == 0)
    {
        factory->m_HttpContentLength = strtol(value, 0, 10);
        if (factory->m_HttpContentLength < 0) {
            dmLogError("Content-Length negative (%d)", factory->m_HttpContentLength);
        } else {
            if (factory->m_HttpBuffer->Capacity() < factory->m_HttpContentLength) {
                factory->m_HttpBuffer->SetCapacity(factory->m_HttpContentLength);
            }
            factory->m_HttpBuffer->SetSize(0);
        }
    }
}

static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
{
    SResourceFactory* factory = (SResourceFactory*) user_data;
    (void) status_code;

    if (!content_data && content_data_size)
    {
        factory->m_HttpBuffer->SetSize(0);
        return;
    }

    // We must set http-status here. For direct cached result HttpHeader is not called.
    factory->m_HttpStatus = status_code;

    if (factory->m_HttpBuffer->Remaining() < content_data_size) {
        uint32_t diff = content_data_size - factory->m_HttpBuffer->Remaining();
        // NOTE: Resizing the the array can be inefficient but sometimes we don't know the actual size, i.e. when "Content-Size" isn't set
        factory->m_HttpBuffer->OffsetCapacity(diff + 1024 * 1024);
    }

    factory->m_HttpBuffer->PushArray((const char*) content_data, content_data_size);
    factory->m_HttpTotalBytesStreamed += content_data_size;
}

Result LoadManifest(const char* path, const char* location, HFactory factory)
{
    dmLogInfo("Loading the manifest! path: %s, strlen(path): %lu", path, strlen(path));

    uint32_t manifest_file_size = 0;
    uint32_t dummy_file_size = 0;
    dmSys::ResourceSize(path, &manifest_file_size);
    uint8_t* manifest_file_data = 0x0;
    assert(dmMemory::RESULT_OK == dmMemory::AlignedMalloc((void**)&manifest_file_data, 16, manifest_file_size));
    dmSys::Result res = dmSys::LoadResource(path, manifest_file_data, manifest_file_size, &dummy_file_size);

    if (res != dmSys::RESULT_OK)
    {
        dmLogError("Failed to load manifest resource, result = %i", res);
        dmMemory::AlignedFree(manifest_file_data);
        return RESULT_IO_ERROR;
    }

    // Read from manifest resource
    dmDDF::Result ddf_res = dmDDF::LoadMessage(manifest_file_data, manifest_file_size, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, (void**)(&factory->m_Manifest->m_DDF));
    dmMemory::AlignedFree(manifest_file_data);
    if (ddf_res != dmDDF::RESULT_OK)
    {
        dmLogError("Failed to load manifest message, result = %i", ddf_res);
        return RESULT_IO_ERROR;
    }

    // Validate file version
    dmLiveUpdateDDF::ManifestHeader header = factory->m_Manifest->m_DDF->m_Data.m_Header;
    if (header.m_MagicNumber != MANIFEST_MAGIC_NUMBER)
    {
        dmLogError("Magic number mismatch");
        return RESULT_FORMAT_ERROR;
    }

    if (header.m_Version != MANIFEST_VERSION)
    {
        dmLogError("Version mismatch, engine version: %i,  manifest version: %i", dmResourceArchive::VERSION, header.m_Version);
        return RESULT_FORMAT_ERROR;
    }

    // Construct path to archive index file. Assumes same path as manifest file.
    char archiveIndexPath[DMPATH_MAX_PATH];
    memset(&archiveIndexPath[0], '\0', DMPATH_MAX_PATH);
    uint32_t manifest_str_len = 14; // 'game.dmanifest' is 14 characters
    if (strlen(path) > manifest_str_len)
    {
        dmStrlCpy(archiveIndexPath, path, strlen(path) - manifest_str_len);
        dmStrlCat(archiveIndexPath, "/game.arci", DMPATH_MAX_PATH);
    }
    else
    {
        dmStrlCat(archiveIndexPath, "game.arci", DMPATH_MAX_PATH);
    }

    dmLogInfo("Constructed archive index file path: %s THE END", archiveIndexPath);

    Result r = MountArchiveInternal(archiveIndexPath, &factory->m_Manifest->m_ArchiveIndex, &factory->m_ArchiveMountInfo2);
    if (r != RESULT_OK)
    {
        return r;
    }

    return RESULT_OK;
}

HFactory NewFactory(NewFactoryParams* params, const char* uri)
{
    dmMessage::HSocket socket = 0;

    dmMessage::Result mr = dmMessage::NewSocket(RESOURCE_SOCKET_NAME, &socket);
    if (mr != dmMessage::RESULT_OK)
    {
        dmLogFatal("Unable to create resource socket: %s (%d)", RESOURCE_SOCKET_NAME, mr);
        return 0;
    }

    SResourceFactory* factory = new SResourceFactory;
    memset(factory, 0, sizeof(*factory));
    factory->m_Socket = socket;

    dmURI::Result uri_result = dmURI::Parse(uri, &factory->m_UriParts);
    if (uri_result != dmURI::RESULT_OK)
    {
        dmLogError("Unable to parse uri: %s", uri);
        dmMessage::DeleteSocket(socket);
        delete factory;
        return 0;
    }

    factory->m_HttpBuffer = 0;
    factory->m_HttpClient = 0;
    factory->m_HttpCache = 0;
    if (strcmp(factory->m_UriParts.m_Scheme, "http") == 0 || strcmp(factory->m_UriParts.m_Scheme, "https") == 0)
    {
        factory->m_HttpCache = 0;
        if (params->m_Flags & RESOURCE_FACTORY_FLAGS_HTTP_CACHE)
        {
            dmHttpCache::NewParams cache_params;
            char path[1024];
            dmSys::Result sys_result = dmSys::GetApplicationSupportPath("defold", path, sizeof(path));
            if (sys_result == dmSys::RESULT_OK)
            {
                // NOTE: The other http-service cache is called /http-cache
                dmStrlCat(path, "/cache", sizeof(path));
                cache_params.m_Path = path;
                dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &factory->m_HttpCache);
                if (cache_r != dmHttpCache::RESULT_OK)
                {
                    dmLogWarning("Unable to open http cache (%d)", cache_r);
                }
                else
                {
                    dmHttpCacheVerify::Result verify_r = dmHttpCacheVerify::VerifyCache(factory->m_HttpCache, &factory->m_UriParts, 60 * 60 * 24 * 5); // 5 days
                    // Http-cache batch verification might be unsupported
                    // We currently does not have support for batch validation in the editor http-server
                    // Batch validation was introduced when we had remote branch and latency problems
                    if (verify_r != dmHttpCacheVerify::RESULT_OK && verify_r != dmHttpCacheVerify::RESULT_UNSUPPORTED)
                    {
                        dmLogWarning("Cache validation failed (%d)", verify_r);
                    }

                    dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
                }
            }
            else
            {
                dmLogWarning("Unable to locate application support path (%d)", sys_result);
            }
        }

        dmHttpClient::NewParams http_params;
        http_params.m_HttpHeader = &HttpHeader;
        http_params.m_HttpContent = &HttpContent;
        http_params.m_Userdata = factory;
        http_params.m_HttpCache = factory->m_HttpCache;
        factory->m_HttpClient = dmHttpClient::New(&http_params, factory->m_UriParts.m_Hostname, factory->m_UriParts.m_Port, strcmp(factory->m_UriParts.m_Scheme, "https") == 0);
        if (!factory->m_HttpClient)
        {
            dmLogError("Invalid URI: %s", uri);
            dmMessage::DeleteSocket(socket);
            delete factory;
            return 0;
        }
    }
    else if (strcmp(factory->m_UriParts.m_Scheme, "file") == 0)
    {
        // Ok
    }
    /*else if (strcmp(factory->m_UriParts.m_Scheme, "arc") == 0)
    {
        Result r = MountArchiveInternal(factory->m_UriParts.m_Path, &factory->m_Archive, &factory->m_ArchiveMountInfo);
        if (r != RESULT_OK)
        {
            dmLogError("Unable to load archive: %s", factory->m_UriParts.m_Path);
            dmMessage::DeleteSocket(socket);
            delete factory;
            return 0;
        }
    }*/
    else if (strcmp(factory->m_UriParts.m_Scheme, "dmanif") == 0)
    {
        factory->m_Manifest = new Manifest();
        factory->m_Manifest->m_DDF = new dmLiveUpdateDDF::ManifestFile();

        //dmLogInfo("path: %s, location: %s", factory->m_UriParts.m_Path, factory->m_UriParts.m_Location);

        Result r = LoadManifest(factory->m_UriParts.m_Path, factory->m_UriParts.m_Location, factory);
        if (r != RESULT_OK)
        {
            dmLogError("Unable to load manifest: %s", factory->m_UriParts.m_Path);
            dmMessage::DeleteSocket(socket);
            delete factory->m_Manifest->m_DDF;
            delete factory->m_Manifest;
            delete factory;
            return 0;
        }
    }
    else
    {
        dmLogError("Invalid URI: %s", uri);
        dmMessage::DeleteSocket(socket);
        delete factory;
        return 0;
    }

    factory->m_ResourceTypesCount = 0;

    const uint32_t table_size = dmMath::Max(1u, (3 * params->m_MaxResources) / 4);
    factory->m_Resources = new dmHashTable<uint64_t, SResourceDescriptor>();
    factory->m_Resources->SetCapacity(table_size, params->m_MaxResources);

    factory->m_ResourceToHash = new dmHashTable<uintptr_t, uint64_t>();
    factory->m_ResourceToHash->SetCapacity(table_size, params->m_MaxResources);

    if (params->m_Flags & RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT)
    {
        factory->m_ResourceHashToFilename = new dmHashTable<uint64_t, const char*>();
        factory->m_ResourceHashToFilename->SetCapacity(table_size, params->m_MaxResources);

        factory->m_ResourceReloadedCallbacks = new dmArray<ResourceReloadedCallbackPair>();
        factory->m_ResourceReloadedCallbacks->SetCapacity(256);
    }
    else
    {
        factory->m_ResourceHashToFilename = 0;
        factory->m_ResourceReloadedCallbacks = 0;
    }

    // if (params->m_BuiltinsArchive)
    // {
    //     dmResourceArchive::WrapArchiveBuffer(params->m_BuiltinsArchive, params->m_BuiltinsArchiveSize, &factory->m_BuiltinsArchive);
    // }
    // else
    // {
        factory->m_BuiltinsArchive = 0;
    //}

    factory->m_LoadMutex = dmMutex::New();
    return factory;
}

void DeleteFactory(HFactory factory)
{
    if (factory->m_Socket)
    {
        dmMessage::DeleteSocket(factory->m_Socket);
    }
    if (factory->m_HttpClient)
    {
        dmHttpClient::Delete(factory->m_HttpClient);
    }
    if (factory->m_HttpCache)
    {
        dmHttpCache::Close(factory->m_HttpCache);
    }
    if (factory->m_Archive)
    {
        UnmountArchiveInternal(factory->m_Archive, factory->m_ArchiveMountInfo);
    }
    if (factory->m_LoadMutex)
    {
        dmMutex::Delete(factory->m_LoadMutex);
    }
    if (factory->m_Manifest)
    {
        
        if (factory->m_Manifest->m_DDF)
        {
            dmDDF::FreeMessage(factory->m_Manifest->m_DDF);
        }

        if (factory->m_Manifest->m_ArchiveIndex)
        {
            UnmountArchiveInternal(factory->m_Manifest->m_ArchiveIndex, factory->m_ArchiveMountInfo2);
        }
        delete factory->m_Manifest;
    }
    delete factory->m_Resources;
    delete factory->m_ResourceToHash;
    if (factory->m_ResourceHashToFilename)
        delete factory->m_ResourceHashToFilename;
    if (factory->m_ResourceReloadedCallbacks)
        delete factory->m_ResourceReloadedCallbacks;
    delete factory;
}

static void Dispatch(dmMessage::Message* message, void* user_ptr)
{
    HFactory factory = (HFactory) user_ptr;

    if (message->m_Descriptor)
    {
        dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
        if (descriptor == dmResourceDDF::Reload::m_DDFDescriptor)
        {
            dmResourceDDF::Reload* reload_resource = (dmResourceDDF::Reload*) message->m_Data;
            const char* resource = (const char*) ((uintptr_t) reload_resource + (uintptr_t) reload_resource->m_Resource);

            SResourceDescriptor* resource_descriptor;
            ReloadResource(factory, resource, &resource_descriptor);

        }
        else
        {
            dmLogError("Unknown message '%s' sent to socket '%s'.\n", descriptor->m_Name, RESOURCE_SOCKET_NAME);
        }
    }
    else
    {
        dmLogError("Only system messages can be sent to the '%s' socket.\n", RESOURCE_SOCKET_NAME);
    }
}

void UpdateFactory(HFactory factory)
{
    dmMessage::Dispatch(factory->m_Socket, &Dispatch, factory);
}

Result RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           FResourcePreload preload_function,
                           FResourceCreate create_function,
                           FResourceDestroy destroy_function,
                           FResourceRecreate recreate_function)
{
    if (factory->m_ResourceTypesCount == MAX_RESOURCE_TYPES)
        return RESULT_OUT_OF_RESOURCES;

    // Dots not allowed in extension
    if (strrchr(extension, '.') != 0)
        return RESULT_INVAL;

    if (create_function == 0 || destroy_function == 0)
        return RESULT_INVAL;

    if (FindResourceType(factory, extension) != 0)
        return RESULT_ALREADY_REGISTERED;

    SResourceType resource_type;
    resource_type.m_Extension = extension;
    resource_type.m_Context = context;
    resource_type.m_PreloadFunction = preload_function;
    resource_type.m_CreateFunction = create_function;
    resource_type.m_DestroyFunction = destroy_function;
    resource_type.m_RecreateFunction = recreate_function;

    factory->m_ResourceTypes[factory->m_ResourceTypesCount++] = resource_type;

    return RESULT_OK;
}

Result LoadFromManifest(HFactory factory, const Manifest* manifest, const char* path, uint32_t* resource_size, LoadBufferType* buffer)
{
    // Get resource hash from path_hash
    dmLogInfo("Loading from manifest! path: %s", path);
    uint32_t entry_count = manifest->m_DDF->m_Data.m_Resources.m_Count;
    dmLiveUpdateDDF::ResourceEntry* entries = manifest->m_DDF->m_Data.m_Resources.m_Data;

    uint32_t first = 0;
    uint32_t last = entry_count-1;
    uint64_t path_hash = dmHashString64(path);

    while (first <= last)
    {
        int mid = first + (last - first) / 2;
        uint64_t h = entries[mid].m_UrlHash;

        if (h == path_hash)
        {
            dmResourceArchive::EntryData ed;
            dmResourceArchive::Result res = dmResourceArchive::FindEntry2(manifest->m_ArchiveIndex, entries[mid].m_Hash.m_Data.m_Data, &ed);
            if (res == dmResourceArchive::RESULT_OK)
            {
                uint32_t file_size = ed.m_ResourceSize;
                if (buffer->Capacity() < file_size)
                {
                    buffer->SetCapacity(file_size);
                }

                buffer->SetSize(0);
                dmResourceArchive::Read2(manifest->m_ArchiveIndex, &ed, buffer->Begin());
                buffer->SetSize(file_size);
                *resource_size = file_size;

                return RESULT_OK;
            }
            else if (res == dmResourceArchive::RESULT_NOT_FOUND)
            {
                // Resource was found in manifest, but not in archive
                return RESULT_RESOURCE_NOT_FOUND;
            }
            else
            {
                return RESULT_IO_ERROR;
            }
        }
        else if (h > path_hash)
        {
            last = mid-1;
        }
        else if (h < path_hash)
        {
            first = mid+1;
        }
    }

    return RESULT_RESOURCE_NOT_FOUND;
}

// Assumes m_LoadMutex is already held
static Result LoadFromArchive(HFactory factory, dmResourceArchive::HArchive archive, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    dmResourceArchive::EntryInfo entry_info;
    dmResourceArchive::Result r = dmResourceArchive::FindEntry(archive, original_name, &entry_info);
    if (r == dmResourceArchive::RESULT_OK)
    {
        uint32_t file_size = entry_info.m_Size;
        if (buffer->Capacity() < file_size) {
            buffer->SetCapacity(file_size);
        }

        buffer->SetSize(0);
        dmResourceArchive::Read(archive, &entry_info, buffer->Begin());
        buffer->SetSize(file_size);
        *resource_size = file_size;

        return RESULT_OK;
    }
    else if (r == dmResourceArchive::RESULT_NOT_FOUND)
    {
        return RESULT_RESOURCE_NOT_FOUND;
    }
    else
    {
        return RESULT_IO_ERROR;
    }
}

// Assumes m_LoadMutex is already held
Result DoLoadResourceLocked(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    DM_PROFILE(Resource, "LoadResource");

    // if (factory->m_BuiltinsArchive)
    // {
    //     if (LoadFromArchive(factory, factory->m_BuiltinsArchive, path, original_name, resource_size, buffer) == RESULT_OK)
    //     {
    //         return RESULT_OK;
    //     }
    // }

    // NOTE: No else if here. Fall through
    if (factory->m_HttpClient)
    {
        // Load over HTTP
        *resource_size = 0;
        factory->m_HttpBuffer = buffer;
        factory->m_HttpContentLength = -1;
        factory->m_HttpTotalBytesStreamed = 0;
        factory->m_HttpFactoryResult = RESULT_OK;
        factory->m_HttpStatus = -1;

        char uri[RESOURCE_PATH_MAX*2];
        dmURI::Encode(path, uri, sizeof(uri));

        dmHttpClient::Result http_result = dmHttpClient::Get(factory->m_HttpClient, uri);
        if (http_result != dmHttpClient::RESULT_OK)
        {
            if (factory->m_HttpStatus == 404)
            {
                return RESULT_RESOURCE_NOT_FOUND;
            }
            else
            {
                // 304 (NOT MODIFIED) is OK. 304 is returned when the resource is loaded from cache, ie ETag or similar match
                if (http_result == dmHttpClient::RESULT_NOT_200_OK && factory->m_HttpStatus != 304)
                {
                    dmLogWarning("Unexpected http status code: %d", factory->m_HttpStatus);
                    return RESULT_IO_ERROR;
                }
            }
        }

        if (factory->m_HttpFactoryResult != RESULT_OK)
            return factory->m_HttpFactoryResult;

        // Only check content-length if status != 304 (NOT MODIFIED)
        if (factory->m_HttpStatus != 304 && factory->m_HttpContentLength != -1 && factory->m_HttpContentLength != factory->m_HttpTotalBytesStreamed)
        {
            dmLogError("Expected content length differs from actually streamed for resource %s (%d != %d)", path, factory->m_HttpContentLength, factory->m_HttpTotalBytesStreamed);
        }

        *resource_size = factory->m_HttpTotalBytesStreamed;
        return RESULT_OK;
    }
    /*else if (factory->m_Archive)
    {
        Result r = LoadFromArchive(factory, factory->m_Archive, path, original_name, resource_size, buffer);
        return r;
    }*/
    else if (factory->m_Manifest)
    {
        Result r = LoadFromManifest(factory, factory->m_Manifest, original_name, resource_size, buffer);
        return r;
    }
    else
    {
        // Load over local file system
        uint32_t file_size;
        dmSys::Result r = dmSys::ResourceSize(path, &file_size);
        if (r != dmSys::RESULT_OK) {
            if (r == dmSys::RESULT_NOENT)
                return RESULT_RESOURCE_NOT_FOUND;
            else
                return RESULT_IO_ERROR;
        }

        if (buffer->Capacity() < file_size) {
            buffer->SetCapacity(file_size);
        }
        buffer->SetSize(0);

        r = dmSys::LoadResource(path, buffer->Begin(), file_size, &file_size);
        if (r == dmSys::RESULT_OK) {
            buffer->SetSize(file_size);
            *resource_size = file_size;
            return RESULT_OK;
        } else {
            if (r == dmSys::RESULT_NOENT)
                return RESULT_RESOURCE_NOT_FOUND;
            else
                return RESULT_IO_ERROR;
        }
    }
}

// Takes the lock.
Result DoLoadResource(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    // Called from async queue so we wrap around a lock
    dmMutex::ScopedLock lk(factory->m_LoadMutex);
    return DoLoadResourceLocked(factory, path, original_name, resource_size, buffer);
}

// Assumes m_LoadMutex is already held
Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size)
{
    if (factory->m_Buffer.Capacity() != DEFAULT_BUFFER_SIZE) {
        factory->m_Buffer.SetCapacity(DEFAULT_BUFFER_SIZE);
    }
    factory->m_Buffer.SetSize(0);

    Result r = DoLoadResourceLocked(factory, path, original_name, resource_size, &factory->m_Buffer);
    if (r == RESULT_OK)
        *buffer = factory->m_Buffer.Begin();
    else
        *buffer = 0;
    return r;
}

// Assumes m_LoadMutex is already held
static Result DoGet(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);

    DM_PROFILE(Resource, "Get");

    *resource = 0;

    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    // Try to get from already loaded resources
    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);
    if (rd)
    {
        assert(factory->m_ResourceToHash->Get((uintptr_t) rd->m_Resource));
        rd->m_ReferenceCount++;
        *resource = rd->m_Resource;
        return RESULT_OK;
    }

    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    // Resource not loaded previously, try and load the resource from archive
    const char* ext = strrchr(name, '.');
    if (ext)
    {
        ext++;

        SResourceType* resource_type = FindResourceType(factory, ext);
        if (resource_type == 0)
        {
            dmLogError("Unknown resource type: %s", ext);
            return RESULT_UNKNOWN_RESOURCE_TYPE;
        }

        void *buffer;
        uint32_t file_size;
        Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
        if (result != RESULT_OK) {
            if (result == RESULT_RESOURCE_NOT_FOUND) {
                dmLogWarning("Resource not found: %s", name);
            }
            return result;
        }

        assert(buffer == factory->m_Buffer.Begin());

        // TODO: We should *NOT* allocate SResource dynamically...
        SResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));
        tmp_resource.m_NameHash = canonical_path_hash;
        tmp_resource.m_ReferenceCount = 1;
        tmp_resource.m_ResourceType = (void*) resource_type;

        void *preload_data = 0;
        Result create_error = RESULT_OK;

        if (resource_type->m_PreloadFunction)
        {
            ResourcePreloadParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Buffer = buffer;
            params.m_BufferSize = file_size;
            params.m_PreloadData = &preload_data;
            params.m_Filename = name;
            params.m_HintInfo = 0; // No hinting now
            create_error = resource_type->m_PreloadFunction(params);
        }

        if (create_error == RESULT_OK)
        {
            ResourceCreateParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Buffer = buffer;
            params.m_BufferSize = file_size;
            params.m_PreloadData = preload_data;
            params.m_Resource = &tmp_resource;
            params.m_Filename = name;
            create_error = resource_type->m_CreateFunction(params);
        }

        // Restore to default buffer size
        if (factory->m_Buffer.Capacity() != DEFAULT_BUFFER_SIZE) {
            factory->m_Buffer.SetCapacity(DEFAULT_BUFFER_SIZE);
        }
        factory->m_Buffer.SetSize(0);

        if (create_error == RESULT_OK)
        {
            Result insert_error = InsertResource(factory, name, canonical_path_hash, &tmp_resource);
            if (insert_error == RESULT_OK)
            {
                *resource = tmp_resource.m_Resource;
                return RESULT_OK;
            }
            else
            {
                ResourceDestroyParams params;
                params.m_Factory = factory;
                params.m_Context = resource_type->m_Context;
                params.m_Resource = &tmp_resource;
                resource_type->m_DestroyFunction(params);
                return insert_error;
            }
        }
        else
        {
            dmLogWarning("Unable to create resource: %s", canonical_path);
            return create_error;
        }
    }
    else
    {
        dmLogWarning("Unable to load resource: '%s'. Missing file extension.", name);
        return RESULT_MISSING_FILE_EXTENSION;
    }
}

Result Get(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);
    *resource = 0;

    Result chk = CheckSuppliedResourcePath(name);
    if (chk != RESULT_OK)
        return chk;

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    dmArray<const char*>& stack = factory->m_GetResourceStack;
    if (factory->m_RecursionDepth == 0)
    {
        stack.SetSize(0);
    }

    ++factory->m_RecursionDepth;

    uint32_t n = stack.Size();
    for (uint32_t i=0;i<n;i++)
    {
        if (strcmp(stack[i], name) == 0)
        {
            dmLogError("Self referring resource detected");
            dmLogError("Reference chain:");
            for (uint32_t j = 0; j < n; ++j)
            {
                dmLogError("%d: %s", j, stack[j]);
            }
            dmLogError("%d: %s", n, name);
            --factory->m_RecursionDepth;
            return RESULT_RESOURCE_LOOP_ERROR;
        }
    }
    fflush(stdout);

    if (stack.Full())
    {
        stack.SetCapacity(stack.Capacity() + 16);
    }
    stack.Push(name);

    Result r = DoGet(factory, name, resource);
    stack.SetSize(stack.Size() - 1);
    --factory->m_RecursionDepth;
    return r;
}

SResourceDescriptor* GetByHash(HFactory factory, uint64_t canonical_path_hash)
{
    return factory->m_Resources->Get(canonical_path_hash);
}

Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, SResourceDescriptor* descriptor)
{
    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    assert(descriptor->m_Resource);
    assert(descriptor->m_ReferenceCount == 1);

    factory->m_Resources->Put(canonical_path_hash, *descriptor);
    factory->m_ResourceToHash->Put((uintptr_t) descriptor->m_Resource, canonical_path_hash);
    if (factory->m_ResourceHashToFilename)
    {
        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(factory, path, canonical_path);
        factory->m_ResourceHashToFilename->Put(canonical_path_hash, strdup(canonical_path));
    }

    return RESULT_OK;
}

Result GetRaw(HFactory factory, const char* name, void** resource, uint32_t* resource_size)
{
    DM_PROFILE(Resource, "GetRaw");

    assert(name);
    assert(resource);
    assert(resource_size);

    *resource = 0;
    *resource_size = 0;

    Result chk = CheckSuppliedResourcePath(name);
    if (chk != RESULT_OK)
        return chk;

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    void* buffer;
    uint32_t file_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
    if (result == RESULT_OK) {
        *resource = malloc(file_size);
        assert(buffer == factory->m_Buffer.Begin());
        memcpy(*resource, buffer, file_size);
        *resource_size = file_size;
    }
    return result;
}

static Result DoReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);

    if (out_descriptor)
        *out_descriptor = rd;

    if (rd == 0x0)
        return RESULT_RESOURCE_NOT_FOUND;

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    void* buffer;
    uint32_t file_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
    if (result != RESULT_OK)
        return result;

    assert(buffer == factory->m_Buffer.Begin());

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Buffer = buffer;
    params.m_BufferSize = file_size;
    params.m_Resource = rd;
    params.m_Filename = name;
    Result create_result = resource_type->m_RecreateFunction(params);
    if (create_result == RESULT_OK)
    {
        if (factory->m_ResourceReloadedCallbacks)
        {
            for (uint32_t i = 0; i < factory->m_ResourceReloadedCallbacks->Size(); ++i)
            {
                ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
                ResourceReloadedParams params;
                params.m_UserData = pair.m_UserData;
                params.m_Resource = rd;
                params.m_Name = name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }
}

Result ReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor)
{
    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    // Always verify cache for reloaded resources
    if (factory->m_HttpCache)
        dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);

    Result result = DoReloadResource(factory, name, out_descriptor);

    switch (result)
    {
        case RESULT_OK:
            dmLogInfo("%s was successfully reloaded.", name);
            break;
        case RESULT_OUT_OF_MEMORY:
            dmLogError("Not enough memory to reload %s.", name);
            break;
        case RESULT_FORMAT_ERROR:
        case RESULT_CONSTANT_ERROR:
            dmLogError("%s has invalid format and could not be reloaded.", name);
            break;
        case RESULT_RESOURCE_NOT_FOUND:
            dmLogError("%s could not be reloaded since it was never loaded before.", name);
            break;
        case RESULT_NOT_SUPPORTED:
            dmLogWarning("Reloading of resource type %s not supported.", ((SResourceType*)(*out_descriptor)->m_ResourceType)->m_Extension);
            break;
        default:
            dmLogWarning("%s could not be reloaded, unknown error: %d.", name, result);
            break;
    }

    if (factory->m_HttpCache)
        dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    return result;
}

Result GetType(HFactory factory, void* resource, ResourceType* type)
{
    assert(type);

    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    if (!resource_hash)
    {
        return RESULT_NOT_LOADED;
    }

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    *type = (ResourceType) rd->m_ResourceType;

    return RESULT_OK;
}

Result GetTypeFromExtension(HFactory factory, const char* extension, ResourceType* type)
{
    assert(type);

    SResourceType* resource_type = FindResourceType(factory, extension);
    if (resource_type)
    {
        *type = (ResourceType) resource_type;
        return RESULT_OK;
    }
    else
    {
        return RESULT_UNKNOWN_RESOURCE_TYPE;
    }
}

Result GetExtensionFromType(HFactory factory, ResourceType type, const char** extension)
{
    for (uint32_t i = 0; i < factory->m_ResourceTypesCount; ++i)
    {
        SResourceType* rt = &factory->m_ResourceTypes[i];

        if (((uintptr_t) rt) == type)
        {
            *extension = rt->m_Extension;
            return RESULT_OK;
        }
    }

    *extension = 0;
    return RESULT_UNKNOWN_RESOURCE_TYPE;
}

Result GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* tmp_descriptor = factory->m_Resources->Get(canonical_path_hash);
    if (tmp_descriptor)
    {
        *descriptor = *tmp_descriptor;
        return RESULT_OK;
    }
    else
    {
        return RESULT_NOT_LOADED;
    }
}

void IncRef(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    assert(resource_hash);

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    ++rd->m_ReferenceCount;
}

void Release(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    assert(resource_hash);

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    rd->m_ReferenceCount--;

    if (rd->m_ReferenceCount == 0)
    {
        SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;

        ResourceDestroyParams params;
        params.m_Factory = factory;
        params.m_Context = resource_type->m_Context;
        params.m_Resource = rd;
        resource_type->m_DestroyFunction(params);

        factory->m_ResourceToHash->Erase((uintptr_t) resource);
        factory->m_Resources->Erase(*resource_hash);
        if (factory->m_ResourceHashToFilename)
        {
            const char** s = factory->m_ResourceHashToFilename->Get(*resource_hash);
            factory->m_ResourceHashToFilename->Erase(*resource_hash);
            assert(s);
            free((void*) *s);
        }
    }
}

void RegisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data)
{
    if (factory->m_ResourceReloadedCallbacks)
    {
        if (factory->m_ResourceReloadedCallbacks->Full())
        {
            factory->m_ResourceReloadedCallbacks->SetCapacity(factory->m_ResourceReloadedCallbacks->Capacity() + 128);
        }
        ResourceReloadedCallbackPair pair;
        pair.m_Callback = callback;
        pair.m_UserData = user_data;
        factory->m_ResourceReloadedCallbacks->Push(pair);
    }
}

void UnregisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data)
{
    if (factory->m_ResourceReloadedCallbacks)
    {
        uint32_t i = 0;
        uint32_t size = factory->m_ResourceReloadedCallbacks->Size();
        while (i < size)
        {
            ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
            if (pair.m_Callback == callback && pair.m_UserData == user_data)
            {
                factory->m_ResourceReloadedCallbacks->EraseSwap(i);
                --size;
            }
            else
            {
                ++i;
            }
        }
    }
}

}
