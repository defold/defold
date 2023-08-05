// Copyright 2020-2023 The Defold Foundation
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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#else
#include <alloca.h>
#endif

#ifdef __linux__
#include <limits.h>
#elif defined (__MACH__)
#include <sys/param.h>
#endif

#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <dlib/message.h>
#include <dlib/mutex.h>
#include <dlib/path.h>
#include <dlib/profile.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <dlib/uri.h>

#include "resource.h"
#include "resource_manifest.h"
#include "resource_mounts.h"
#include "resource_private.h"
#include "resource_util.h"
#include <resource/resource_ddf.h>

#include "providers/provider.h"         // dmResourceProviderArchive::Result
#include "providers/provider_archive.h" // dmResourceProviderArchive::LoadManifest

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

DM_PROPERTY_U32(rmtp_Resource, 0, FrameReset, "# resources");

namespace dmResource
{
const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

#define RESOURCE_SOCKET_NAME "@resource"

const char* BUNDLE_MANIFEST_FILENAME            = "game.dmanifest";
const char* BUNDLE_INDEX_FILENAME               = "game.arci";
const char* BUNDLE_DATA_FILENAME                = "game.arcd";
const char* BUNDLE_PUBLIC_KEY_FILENAME          = "game.public.der";


const char* MAX_RESOURCES_KEY = "resource.max_resources";

struct ResourceReloadedCallbackPair
{
    ResourceReloadedCallback    m_Callback;
    void*                       m_UserData;
};

struct SResourceFactory
{
    // TODO: Arg... budget. Two hash-maps. Really necessary?
    dmHashTable64<SResourceDescriptor>*          m_Resources;
    dmHashTable<uintptr_t, uint64_t>*            m_ResourceToHash;
    // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
    // Used for reloading of resources
    dmHashTable64<const char*>*                  m_ResourceHashToFilename;
    // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
    dmArray<ResourceReloadedCallbackPair>*       m_ResourceReloadedCallbacks;
    SResourceType                                m_ResourceTypes[MAX_RESOURCE_TYPES];
    uint32_t                                     m_ResourceTypesCount;

    // Guard for anything that touches anything that could be shared
    // with GetRaw (used for async threaded loading). Liveupdate, HttpClient, m_Buffer
    // m_BuiltinsManifest, m_Manifest
    dmMutex::HMutex                              m_LoadMutex;

    // dmResource::Get recursion depth
    uint32_t                                     m_RecursionDepth;
    // List of resources currently in dmResource::Get call-stack
    dmArray<const char*>                         m_GetResourceStack;

    dmMessage::HSocket                           m_Socket;

    dmURI::Parts                                 m_UriParts;

    const char*                                  m_PublicKeyPath; // path to game.public.der

    dmArray<char>                                m_Buffer;

    // Resource manifest
    dmResourceMounts::HContext                   m_Mounts;
    dmResourceProvider::HArchive                 m_BuiltinMount;
    dmResourceProvider::HArchive                 m_BaseArchiveMount;
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
    memset(params, 0, sizeof(NewFactoryParams));
    params->m_MaxResources = 1024;
    params->m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;

    params->m_ArchiveManifest.m_Data = 0;
    params->m_ArchiveManifest.m_Size = 0;
    params->m_ArchiveIndex.m_Data = 0;
    params->m_ArchiveIndex.m_Size = 0;
    params->m_ArchiveData.m_Data = 0;
    params->m_ArchiveData.m_Size = 0;
}

static Result AddBuiltinMount(HFactory factory, NewFactoryParams* params)
{
    dmResourceProvider::HArchiveInternal internal = 0;
    dmResourceProvider::Result result;
    result = dmResourceProviderArchive::CreateArchive( (uint8_t*)params->m_ArchiveManifest.m_Data, params->m_ArchiveManifest.m_Size,
                                                       (uint8_t*)params->m_ArchiveIndex.m_Data, params->m_ArchiveIndex.m_Size,
                                                       (uint8_t*)params->m_ArchiveData.m_Data, params->m_ArchiveData.m_Size,
                                                       &internal);
    if (dmResourceProvider::RESULT_OK != result)
    {
        internal = 0;
        dmLogError("Failed to create in-memory archive from builtin project: %d", result);
        return RESULT_INVAL;
    }

    dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    result = dmResourceProvider::CreateMount(loader, internal, &factory->m_BuiltinMount);

    if (dmResourceProvider::RESULT_OK != result)
    {
        factory->m_BuiltinMount = 0;
        dmLogError("Failed to mount builtin archive: %d", result);
        return RESULT_NOT_LOADED;
    }

    dmResourceMounts::AddMount(factory->m_Mounts, "_builtin", factory->m_BuiltinMount, -5, false);
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

    factory->m_Mounts = 0;

    // Mount the base archive, regardless of the liveupdate.mounts
    struct SchemeMountTypePair
    {
        const char* m_Scheme;
        const char* m_ProviderType;
        bool        m_CheckPublicKey;
    } type_pairs[] = {
        {"http", "http", false},
        {"https", "http", false},
        {"archive", "archive", true},
        {"dmanif", "archive", true},
        {"file", "file", true},
#if defined(__NX__)
        {"data", "file", false},
        {"host", "file", false},
#endif
    };

    int num_mounted = 0;
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(type_pairs); ++i)
    {
        if (strcmp(factory->m_UriParts.m_Scheme, type_pairs[i].m_Scheme) != 0)
            continue;

        dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64(type_pairs[i].m_ProviderType));
        if (!loader)
            continue;
        if (dmResourceProvider::CanMountUri(loader, &factory->m_UriParts))
        {
            dmResourceProvider::HArchive archive;
            dmResourceProvider::Result provider_result = dmResourceProvider::CreateMount(loader, &factory->m_UriParts, 0, &archive);
            if (dmResourceProvider::RESULT_OK != provider_result)
            {
                dmLogError("Failed to mount base archive: %d for mount %s://%s%s", provider_result, factory->m_UriParts.m_Scheme, factory->m_UriParts.m_Location, factory->m_UriParts.m_Path);
                continue;
            }
            ++num_mounted;

            // Create the mounts context with the base archive
            if (!factory->m_Mounts)
            {
                factory->m_Mounts = dmResourceMounts::Create(archive);
            }

            dmResourceMounts::AddMount(factory->m_Mounts, "_base", archive, -10, false);

            if (strcmp("archive", type_pairs[i].m_ProviderType) == 0)
            {
                // We want access to this later on (mostly for liveupdate, that wants the app ID to use as a folder name for liveupdate content)
                factory->m_BaseArchiveMount = archive;
            }

            if (type_pairs[i].m_CheckPublicKey)
            {
                size_t manifest_path_len = strlen(factory->m_UriParts.m_Path);
                char* app_path = (char*)alloca(manifest_path_len+1);
                dmStrlCpy(app_path, factory->m_UriParts.m_Path, manifest_path_len+1);
                char* app_path_end = strrchr(app_path, '/');
                if (app_path_end)
                    *app_path_end = 0;
                else
                    app_path[0] = 0; // it only contained a filename

                char public_key_path[DMPATH_MAX_PATH];
                dmPath::Concat(app_path, BUNDLE_PUBLIC_KEY_FILENAME, public_key_path, DMPATH_MAX_PATH);

                if (dmSys::ResourceExists(public_key_path))
                {
                    factory->m_PublicKeyPath = strdup(public_key_path);
                }
            }
            break;
        }
    }

    if (!num_mounted)
    {
        dmLogWarning("No resource loaders mounted that could match uri %s", uri);
        DeleteFactory(factory);
        dmMessage::DeleteSocket(socket);
        return 0;
    }

    if (factory->m_BaseArchiveMount)
    {
        dmResource::HManifest manifest;
        if (dmResourceProvider::RESULT_OK == dmResourceProvider::GetManifest(factory->m_BaseArchiveMount, &manifest))
        {
            char app_support_path[DMPATH_MAX_PATH];
            if (RESULT_OK == dmResource::GetApplicationSupportPath(manifest, app_support_path, sizeof(app_support_path)))
            {
                dmResourceMounts::LoadMounts(factory->m_Mounts, app_support_path);
            }
        }
    }

    dmLogDebug("Created resource factory with uri %s\n", uri);

    factory->m_ResourceTypesCount = 0;

    const uint32_t table_size = dmMath::Max(1u, (3 * params->m_MaxResources) / 4);
    factory->m_Resources = new dmHashTable64<SResourceDescriptor>();
    factory->m_Resources->SetCapacity(table_size, params->m_MaxResources);

    factory->m_ResourceToHash = new dmHashTable<uintptr_t, uint64_t>();
    factory->m_ResourceToHash->SetCapacity(table_size, params->m_MaxResources);

    if (params->m_Flags & RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT)
    {
        factory->m_ResourceHashToFilename = new dmHashTable64<const char*>();
        factory->m_ResourceHashToFilename->SetCapacity(table_size, params->m_MaxResources);

        factory->m_ResourceReloadedCallbacks = new dmArray<ResourceReloadedCallbackPair>();
        factory->m_ResourceReloadedCallbacks->SetCapacity(256);
    }
    else
    {
        factory->m_ResourceHashToFilename = 0;
        factory->m_ResourceReloadedCallbacks = 0;
    }

    factory->m_BuiltinMount = 0;
    if (params->m_ArchiveManifest.m_Size && params->m_ArchiveIndex.m_Size && params->m_ArchiveData.m_Size)
    {
        AddBuiltinMount(factory, params);
    }

    factory->m_LoadMutex = dmMutex::New();
    return factory;
}

static void ResourceIteratorCallback(void*, const dmhash_t* id, SResourceDescriptor* resource)
{
    dmLogError("Resource: %s  ref count: %u", dmHashReverseSafe64(*id), resource->m_ReferenceCount);
}

void DeleteFactory(HFactory factory)
{
    if (factory->m_Socket)
    {
        dmMessage::DeleteSocket(factory->m_Socket);
    }
    if (factory->m_LoadMutex)
    {
        dmMutex::Delete(factory->m_LoadMutex);
    }

    ReleaseBuiltinsArchive(factory);

    if (factory->m_Mounts)
        dmResourceMounts::Destroy(factory->m_Mounts);

    if (factory->m_Resources && !factory->m_Resources->Empty())
    {
        dmLogError("Leaked resources:");
        factory->m_Resources->Iterate<>(&ResourceIteratorCallback, (void*)0);
    }

    free((void*)factory->m_PublicKeyPath);
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
            // NOTE use offsets instead of reading via ddf message
            // Message: | offset to string-offsets | count | string offset | string offset | ... | string #1 | string #2 | ... |
            dmResourceDDF::Reload* reload_resources = (dmResourceDDF::Reload*) message->m_Data;
            uint32_t count = reload_resources->m_Resources.m_Count;
            uint8_t* str_offset_cursor = (uint8_t*)((uintptr_t)reload_resources + *(uint32_t*)reload_resources);
            for (uint32_t i = 0; i < count; ++i)
            {
                const char* resource = (const char *) (uintptr_t)reload_resources + *(str_offset_cursor + i * sizeof(uint64_t));
                SResourceDescriptor* resource_descriptor;
                ReloadResource(factory, resource, &resource_descriptor);
            }
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
    DM_PROFILE(__FUNCTION__);
    dmMessage::Dispatch(factory->m_Socket, &Dispatch, factory);
    DM_PROPERTY_ADD_U32(rmtp_Resource, factory->m_Resources->Size());
}

Result RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           FResourcePreload preload_function,
                           FResourceCreate create_function,
                           FResourcePostCreate post_create_function,
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
    resource_type.m_ExtensionHash = dmHashString64(extension);
    resource_type.m_Extension = extension;
    resource_type.m_Context = context;
    resource_type.m_PreloadFunction = preload_function;
    resource_type.m_CreateFunction = create_function;
    resource_type.m_PostCreateFunction = post_create_function;
    resource_type.m_DestroyFunction = destroy_function;
    resource_type.m_RecreateFunction = recreate_function;

    factory->m_ResourceTypes[factory->m_ResourceTypesCount++] = resource_type;

    return RESULT_OK;
}

struct SResourceDependencyCallback
{
    FGetDependency  m_Callback;
    void*           m_CallbackContext;
};

static void ResourceDependencyCallback(void* _context, const dmResourceMounts::SGetDependenciesResult* result)
{
    SResourceDependencyCallback* context = (SResourceDependencyCallback*)_context;
    // Opportunity to check status in the resources (e.g. ref count to see if it's loaded)

    dmResource::SGetDependenciesResult out;
    out.m_UrlHash           = result->m_UrlHash;
    out.m_HashDigest        = result->m_HashDigest;
    out.m_HashDigestLength  = result->m_HashDigestLength;
    out.m_Missing           = result->m_Missing;

    context->m_Callback(context->m_CallbackContext, &out);
}

dmResource::Result GetDependencies(const dmResource::HFactory factory, const SGetDependenciesParams* _params, FGetDependency callback, void* callback_context)
{
    SResourceDependencyCallback ctx;
    ctx.m_Callback = callback;
    ctx.m_CallbackContext = callback_context;

    dmResourceMounts::SGetDependenciesParams params;
    params.m_UrlHash        = _params->m_UrlHash;
    params.m_OnlyMissing    = _params->m_OnlyMissing;
    params.m_Recursive      = _params->m_Recursive;
    return dmResourceMounts::GetDependencies(factory->m_Mounts, &params, ResourceDependencyCallback, &ctx);
}

const char* GetPublicKeyPath(HFactory factory)
{
    return factory->m_PublicKeyPath;
}

dmResourceProvider::HArchive GetBaseArchive(HFactory factory)
{
    return factory->m_BaseArchiveMount;
}

// Assumes m_LoadMutex is already held
static Result DoLoadResourceLocked(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    DM_PROFILE(__FUNCTION__);

    char normalized_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(path, normalized_path); // normalize the path

    // Let's find the resource in the current mounts

    dmhash_t normalized_path_hash = dmHashString64(normalized_path);
    uint32_t file_size;
    dmResource::Result r = dmResourceMounts::GetResourceSize(factory->m_Mounts, normalized_path_hash, normalized_path, &file_size);
    if (r == dmResource::RESULT_OK)
    {
        if (buffer->Capacity() < file_size) {
            buffer->SetCapacity(file_size);
        }
        buffer->SetSize(0);

        r = dmResourceMounts::ReadResource(factory->m_Mounts, normalized_path_hash, normalized_path, (uint8_t*)buffer->Begin(), file_size);
        if (r == dmResource::RESULT_OK)
        {
            buffer->SetSize(file_size);
            *resource_size = file_size;
            return RESULT_OK;
        }
        return r;
    }
    return RESULT_RESOURCE_NOT_FOUND;
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
    {
        *buffer = factory->m_Buffer.Begin();
    }
    else
    {
        *buffer = 0;
    }
    return r;
}

const char* GetExtFromPath(const char* path)
{
    return strrchr(path, '.');
}

// Assumes m_LoadMutex is already held
static Result DoCreateResource(HFactory factory, SResourceType* resource_type, const char* name, const char* canonical_path,
    dmhash_t canonical_path_hash, void* buffer, uint32_t buffer_size, void** resource_out)
{
    // TODO: We should *NOT* allocate SResource dynamically...
    SResourceDescriptor tmp_resource;
    memset(&tmp_resource, 0, sizeof(tmp_resource));
    tmp_resource.m_NameHash       = canonical_path_hash;
    tmp_resource.m_ReferenceCount = 1;
    tmp_resource.m_ResourceType   = (void*) resource_type;

    void *preload_data = 0;
    Result create_error = RESULT_OK;

    if (resource_type->m_PreloadFunction)
    {
        ResourcePreloadParams params;
        params.m_Factory     = factory;
        params.m_Context     = resource_type->m_Context;
        params.m_Buffer      = buffer;
        params.m_BufferSize  = buffer_size;
        params.m_PreloadData = &preload_data;
        params.m_Filename    = name;
        params.m_HintInfo    = 0; // No hinting now
        create_error         = resource_type->m_PreloadFunction(params);
    }

    if (create_error == RESULT_OK)
    {
        tmp_resource.m_ResourceSizeOnDisc = buffer_size;
        tmp_resource.m_ResourceSize       = 0; // Not everything will report a size (but instead rely on the disc size, sinze it's close enough)

        ResourceCreateParams params;
        params.m_Factory     = factory;
        params.m_Context     = resource_type->m_Context;
        params.m_Buffer      = buffer;
        params.m_BufferSize  = buffer_size;
        params.m_PreloadData = preload_data;
        params.m_Resource    = &tmp_resource;
        params.m_Filename    = name;
        create_error         = resource_type->m_CreateFunction(params);
    }

    if (create_error == RESULT_OK && resource_type->m_PostCreateFunction)
    {
        ResourcePostCreateParams params;
        params.m_Factory     = factory;
        params.m_Context     = resource_type->m_Context;
        params.m_PreloadData = preload_data;
        params.m_Resource    = &tmp_resource;
        for(;;)
        {
            create_error = resource_type->m_PostCreateFunction(params);
            if(create_error != RESULT_PENDING)
                break;
            dmTime::Sleep(1000);
        }
    }

    // Restore to default buffer size
    factory->m_Buffer.SetSize(0);
    if (factory->m_Buffer.Capacity() != DEFAULT_BUFFER_SIZE) {
        factory->m_Buffer.SetCapacity(DEFAULT_BUFFER_SIZE);
    }

    if (create_error == RESULT_OK)
    {
        Result insert_error = InsertResource(factory, name, canonical_path_hash, &tmp_resource);
        if (insert_error == RESULT_OK)
        {
            *resource_out = tmp_resource.m_Resource;
            return RESULT_OK;
        }
        else
        {
            ResourceDestroyParams params;
            params.m_Factory  = factory;
            params.m_Context  = resource_type->m_Context;
            params.m_Resource = &tmp_resource;
            resource_type->m_DestroyFunction(params);
            return insert_error;
        }
    }
    else
    {
        dmLogWarning("Unable to create resource: %s: %s", canonical_path, ResultToString(create_error));
        return create_error;
    }
}

// Assumes m_LoadMutex is already held
static Result PrepareResourceCreation(HFactory factory, const char* canonical_path, dmhash_t canonical_path_hash, void** resource_out, SResourceType** resource_type_out)
{
    *resource_out = 0;

    // Try to get from already loaded resources
    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);
    if (rd)
    {
        assert(factory->m_ResourceToHash->Get((uintptr_t) rd->m_Resource));
        rd->m_ReferenceCount++;
        *resource_out = rd->m_Resource;
        return RESULT_OK;
    }

    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    const char* ext = GetExtFromPath(canonical_path);

    if (!ext)
    {
        dmLogWarning("Unable to load resource: '%s'. Missing file extension.", canonical_path);
        return RESULT_MISSING_FILE_EXTENSION;
    }

    // skip over '.'
    ext++;

    SResourceType* resource_type = FindResourceType(factory, ext);
    if (resource_type == 0)
    {
        dmLogError("Unknown resource type: %s", ext);
        return RESULT_UNKNOWN_RESOURCE_TYPE;
    }

    *resource_type_out = resource_type;

    return RESULT_OK;
}

// Assumes m_LoadMutex is already held
static Result CreateAndLoadResource(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);

    DM_PROFILE(__FUNCTION__);

    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(name, canonical_path);
    dmhash_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceType* resource_type;
    Result res = PrepareResourceCreation(factory, canonical_path, canonical_path_hash, resource, &resource_type);
    if (res != RESULT_OK)
    {
        return res;
    }
    else if (*resource != 0x0)
    {
        // Resource already created
        return RESULT_OK;
    }

    void* buffer         = 0;
    uint32_t buffer_size = 0;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &buffer_size);
    if (result != RESULT_OK)
    {
        return result;
    }
    assert(buffer == factory->m_Buffer.Begin());

    return DoCreateResource(factory, resource_type, name, canonical_path, canonical_path_hash, buffer, buffer_size, resource);
}

Result CreateResource(HFactory factory, const char* name, void* data, uint32_t data_size, void** resource)
{
    assert(name);
    assert(resource);

    DM_PROFILE(__FUNCTION__);

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(name, canonical_path);
    dmhash_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceType* resource_type;
    Result res = PrepareResourceCreation(factory, canonical_path, canonical_path_hash, resource, &resource_type);

    if (res != RESULT_OK)
    {
        return res;
    }
    else if (*resource != 0x0)
    {
        // Resource already created
        return RESULT_OK;
    }

    return DoCreateResource(factory, resource_type, name, canonical_path, canonical_path_hash, data, data_size, resource);
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

    if (stack.Full())
    {
        stack.SetCapacity(stack.Capacity() + 16);
    }
    stack.Push(name);
    Result r = CreateAndLoadResource(factory, name, resource);
    stack.SetSize(stack.Size() - 1);
    --factory->m_RecursionDepth;
    return r;
}

Result Get(HFactory factory, dmhash_t name, void** resource)
{
    dmResource::SResourceDescriptor* rd = dmResource::FindByHash(factory, name);
    if (!rd)
    {
        return RESULT_RESOURCE_NOT_FOUND;
    }
    dmResource::IncRef(factory, rd->m_Resource);
    *resource = rd->m_Resource;
    return RESULT_OK;
}

SResourceDescriptor* FindByHash(HFactory factory, uint64_t canonical_path_hash)
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
        GetCanonicalPath(path, canonical_path);
        factory->m_ResourceHashToFilename->Put(canonical_path_hash, strdup(canonical_path));
    }

    return RESULT_OK;
}

Result GetRaw(HFactory factory, const char* name, void** resource, uint32_t* resource_size)
{
    DM_PROFILE(__FUNCTION__);

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
    GetCanonicalPath(name, canonical_path);

    void* buffer;
    uint32_t buffer_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &buffer_size);

    if (result == RESULT_OK) {
        *resource = malloc(buffer_size);
        assert(buffer == factory->m_Buffer.Begin());
        memcpy(*resource, buffer, buffer_size);
        *resource_size = buffer_size;
    }
    return result;
}

static Result DoReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(name, canonical_path);

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
    uint32_t buffer_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &buffer_size);
    if (result != RESULT_OK)
    {
        return result;
    }

    assert(buffer == factory->m_Buffer.Begin());

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = 0;
    params.m_Buffer = buffer;
    params.m_BufferSize = buffer_size;
    params.m_Resource = rd;
    params.m_Filename = name;
    rd->m_PrevResource = 0;
    Result create_result = resource_type->m_RecreateFunction(params);
    if (create_result == RESULT_OK)
    {
        params.m_Resource->m_ResourceSizeOnDisc = buffer_size;
        if (factory->m_ResourceReloadedCallbacks)
        {
            for (uint32_t i = 0; i < factory->m_ResourceReloadedCallbacks->Size(); ++i)
            {
                ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
                ResourceReloadedParams reload_params;
                reload_params.m_UserData = pair.m_UserData;
                reload_params.m_Resource = rd;
                reload_params.m_Name = name;
                pair.m_Callback(reload_params);
            }
        }
        if (rd->m_PrevResource) {
            SResourceDescriptor tmp_resource = *rd;
            tmp_resource.m_Resource = rd->m_PrevResource;
            ResourceDestroyParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Resource = &tmp_resource;
            dmResource::Result res = resource_type->m_DestroyFunction(params);
            rd->m_PrevResource = 0x0;
            return res;
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

    return result;
}

Result SetResource(HFactory factory, uint64_t hashed_name, void* data, uint32_t datasize)
{
    DM_PROFILE(__FUNCTION__);

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    assert(data);

    SResourceDescriptor* rd = factory->m_Resources->Get(hashed_name);
    if (!rd) {
        return RESULT_RESOURCE_NOT_FOUND;
    }

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    assert(data);
    assert(datasize > 0);

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = 0;
    params.m_Buffer = data;
    params.m_BufferSize = datasize;
    params.m_Resource = rd;
    params.m_Filename = 0;
    params.m_NameHash = hashed_name;
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
                params.m_Name = 0;
                params.m_NameHash = hashed_name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }

    return RESULT_OK;
}

Result SetResource(HFactory factory, uint64_t hashed_name, void* message)
{
    DM_PROFILE(__FUNCTION__);

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    assert(message);

    SResourceDescriptor* rd = factory->m_Resources->Get(hashed_name);
    if (!rd) {
        return RESULT_RESOURCE_NOT_FOUND;
    }

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = message;
    params.m_Buffer = 0;
    params.m_BufferSize = 0;
    params.m_Resource = rd;
    params.m_Filename = 0;
    params.m_NameHash = hashed_name;
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
                params.m_Name = 0;
                params.m_NameHash = hashed_name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }

    return RESULT_OK;
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
    GetCanonicalPath(name, canonical_path);

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

Result GetDescriptorWithExt(HFactory factory, uint64_t hashed_name, const uint64_t* exts, uint32_t ext_count, SResourceDescriptor* descriptor)
{
    SResourceDescriptor* tmp_descriptor = factory->m_Resources->Get(hashed_name);
    if (!tmp_descriptor) {
        return RESULT_NOT_LOADED;
    }

    SResourceType* type = (SResourceType*) tmp_descriptor->m_ResourceType;
    bool ext_match = ext_count == 0;
    if (!ext_match) {
        for (uint32_t i = 0; i < ext_count; ++i) {
            if (type->m_ExtensionHash == exts[i]) {
                ext_match = true;
                break;
            }
        }
    }
    if (ext_match) {
        *descriptor = *tmp_descriptor;
        return RESULT_OK;
    } else {
        return RESULT_INVALID_FILE_EXTENSION;
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

// For unit testing
uint32_t GetRefCount(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    if(!resource_hash)
        return 0;
    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    return rd->m_ReferenceCount;
}

uint32_t GetRefCount(HFactory factory, dmhash_t identifier)
{
    SResourceDescriptor* rd = factory->m_Resources->Get(identifier);
    if(!rd)
        return 0;
    return rd->m_ReferenceCount;
}

void Release(HFactory factory, void* resource)
{
    DM_PROFILE(__FUNCTION__);

    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    assert(resource_hash);

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    rd->m_ReferenceCount--;

    if (rd->m_ReferenceCount == 0)
    {
        SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;

        DM_PROFILE_DYN(resource_type->m_Extension, 0);

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

Result GetPath(HFactory factory, const void* resource, uint64_t* hash)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t)resource);
    if( resource_hash ) {
        *hash = *resource_hash;
        return RESULT_OK;
    }
    *hash = 0;
    return RESULT_RESOURCE_NOT_FOUND;
}

dmMutex::HMutex GetLoadMutex(const dmResource::HFactory factory)
{
    return factory->m_LoadMutex;
}

dmResourceMounts::HContext GetMountsContext(const dmResource::HFactory factory)
{
    return factory->m_Mounts;
}

void ReleaseBuiltinsArchive(HFactory factory)
{
    if (factory->m_BuiltinMount)
    {
        dmResourceMounts::RemoveMount(factory->m_Mounts, factory->m_BuiltinMount);
        dmResourceProvider::Unmount(factory->m_BuiltinMount);
        factory->m_BuiltinMount = 0;
    }
}

struct ResourceIteratorCallbackInfo
{
    FResourceIterator   m_Callback;
    void*               m_Context;
    bool                m_ShouldContinue;
};

static void ResourceIteratorCallback(ResourceIteratorCallbackInfo* callback, const dmhash_t* id, SResourceDescriptor* resource)
{
    IteratorResource info;
    info.m_Id           = resource->m_NameHash;
    info.m_SizeOnDisc   = resource->m_ResourceSizeOnDisc;
    info.m_Size         = resource->m_ResourceSize ? resource->m_ResourceSize : resource->m_ResourceSizeOnDisc; // default to the size on disc if no in memory size was specified
    info.m_RefCount     = resource->m_ReferenceCount;

    if (callback->m_ShouldContinue)
    {
        callback->m_ShouldContinue = callback->m_Callback(info, callback->m_Context);
    }
}

void IterateResources(HFactory factory, FResourceIterator callback, void* user_ctx)
{
    DM_MUTEX_SCOPED_LOCK(factory->m_LoadMutex);
    ResourceIteratorCallbackInfo callback_info = {callback, user_ctx, true};
    factory->m_Resources->Iterate<>(&ResourceIteratorCallback, &callback_info);
}

const char* ResultToString(Result r)
{
    #define DM_RESOURCE_RESULT_TO_STRING_CASE(x) case RESULT_##x: return #x;
    switch (r)
    {
        DM_RESOURCE_RESULT_TO_STRING_CASE(OK);
        DM_RESOURCE_RESULT_TO_STRING_CASE(INVALID_DATA);
        DM_RESOURCE_RESULT_TO_STRING_CASE(DDF_ERROR);
        DM_RESOURCE_RESULT_TO_STRING_CASE(RESOURCE_NOT_FOUND);
        DM_RESOURCE_RESULT_TO_STRING_CASE(MISSING_FILE_EXTENSION);
        DM_RESOURCE_RESULT_TO_STRING_CASE(ALREADY_REGISTERED);
        DM_RESOURCE_RESULT_TO_STRING_CASE(INVAL);
        DM_RESOURCE_RESULT_TO_STRING_CASE(UNKNOWN_RESOURCE_TYPE);
        DM_RESOURCE_RESULT_TO_STRING_CASE(OUT_OF_MEMORY);
        DM_RESOURCE_RESULT_TO_STRING_CASE(IO_ERROR);
        DM_RESOURCE_RESULT_TO_STRING_CASE(NOT_LOADED);
        DM_RESOURCE_RESULT_TO_STRING_CASE(OUT_OF_RESOURCES);
        DM_RESOURCE_RESULT_TO_STRING_CASE(STREAMBUFFER_TOO_SMALL);
        DM_RESOURCE_RESULT_TO_STRING_CASE(FORMAT_ERROR);
        DM_RESOURCE_RESULT_TO_STRING_CASE(CONSTANT_ERROR);
        DM_RESOURCE_RESULT_TO_STRING_CASE(NOT_SUPPORTED);
        DM_RESOURCE_RESULT_TO_STRING_CASE(RESOURCE_LOOP_ERROR);
        DM_RESOURCE_RESULT_TO_STRING_CASE(PENDING);
        DM_RESOURCE_RESULT_TO_STRING_CASE(VERSION_MISMATCH);
        DM_RESOURCE_RESULT_TO_STRING_CASE(SIGNATURE_MISMATCH);
        DM_RESOURCE_RESULT_TO_STRING_CASE(UNKNOWN_ERROR);
        default: break;
    }
    #undef DM_RESOURCE_RESULT_TO_STRING_CASE
    return "RESULT_UNDEFINED";
}


dmhash_t GetNameHash(HResourceDescriptor desc)
{
    return desc->m_NameHash;
}

void* GetResource(HResourceDescriptor desc)
{
    return desc->m_Resource;
}

void* GetPreviousResource(HResourceDescriptor desc)
{
    return desc->m_PrevResource;
}

void SetResource(HResourceDescriptor desc, void* resource)
{
    desc->m_Resource = resource;
}

void SetResourceSize(HResourceDescriptor desc, uint32_t size)
{
    desc->m_ResourceSize = size;
}


TypeCreatorDesc* g_ResourceTypeCreatorDescFirst = 0;

void RegisterTypeCreatorDesc(struct TypeCreatorDesc* desc,
                            uint32_t desc_size,
                            const char *name,
                            FResourceTypeRegister register_fn,
                            FResourceTypeRegister deregister_fn)
{
    DM_STATIC_ASSERT(dmResource::s_ResourceTypeCreatorDescBufferSize >= sizeof(struct TypeCreatorDesc), Invalid_Struct_Size);

    dmLogDebug("Registered resource type descriptor %s", name);
    desc->m_Name = name;
    desc->m_RegisterFn = register_fn;
    desc->m_DeregisterFn = deregister_fn;
    desc->m_Next = g_ResourceTypeCreatorDescFirst;
    g_ResourceTypeCreatorDescFirst = desc;
}

const TypeCreatorDesc* GetFirstTypeCreatorDesc()
{
    return g_ResourceTypeCreatorDescFirst;
}


// add already registered types
Result RegisterTypes(HFactory factory, dmHashTable64<void*>* contexts)
{
    const TypeCreatorDesc* desc = GetFirstTypeCreatorDesc();
    while (desc)
    {
        if (contexts->Full()) {
            uint32_t capacity = contexts->Capacity() + 8;
            contexts->SetCapacity(capacity/2, capacity);
        }

        ResourceTypeRegisterContext ctx;
        ctx.m_Factory = factory;
        ctx.m_Contexts = contexts;
        ctx.m_Name = desc->m_Name;
        ctx.m_NameHash = dmHashString64(desc->m_Name);
        Result result = desc->m_RegisterFn(ctx);
        if (result != RESULT_OK)
        {
            dmLogError("Failed to register type '%s': %s", desc->m_Name, ResultToString(result));
            return result;
        }

        dmLogDebug("Registered type '%s'", desc->m_Name);
        desc = desc->m_Next;
    }
    return RESULT_OK;
}

Result DeregisterTypes(HFactory factory, dmHashTable64<void*>* contexts)
{
    const TypeCreatorDesc* desc = GetFirstTypeCreatorDesc();
    while (desc)
    {
        if (desc->m_DeregisterFn)
        {
            ResourceTypeRegisterContext ctx;
            ctx.m_Factory = factory;
            ctx.m_Contexts = contexts;
            ctx.m_Name = desc->m_Name;
            ctx.m_NameHash = dmHashString64(desc->m_Name);

            Result result = desc->m_DeregisterFn(ctx);
            if (result != RESULT_OK)
            {
                dmLogError("Failed to deregister type '%s': %s", desc->m_Name, ResultToString(result));
                return result;
            }
            dmLogDebug("Deregistered type '%s'", desc->m_Name);
        }

        desc = desc->m_Next;
    }
    return RESULT_OK;
}

}
