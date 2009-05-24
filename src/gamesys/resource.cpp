#include <vector>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#ifdef __linux__
#include <limits.h>
#elif defined (__MACH__)
#include <sys/param.h>
#endif

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>

#include "resource.h"

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

namespace Resource
{

struct SResourceType
{
    SResourceType()
    {
        memset(this, 0, sizeof(*this));
    }
    const char*      m_Extension;
    void*            m_Context;
    FResourceCreate  m_CreateFunction;
    FResourceDestroy m_DestroyFunction;
};

// This is both used for m_ResourcePath in SResourceFactory and the total resource path, ie
// m_ResourcePath concatenated with relative path
const uint32_t RESOURCE_PATH_MAX = 1024;

const uint32_t MAX_RESOURCE_TYPES = 128;

struct SResourceFactory
{
    // TODO: Arg... budget. Two hash-maps. Really necessary?
    THashTable<uint64_t, SResourceDescriptor>*   m_Resources;
    THashTable<uintptr_t, uint64_t>*             m_ResourceToHash;
    SResourceType                                m_ResourceTypes[MAX_RESOURCE_TYPES];
    uint32_t                                     m_ResourceTypesCount;
    char                                         m_ResourcePath[RESOURCE_PATH_MAX];
};

static SResourceType* FindResourceType(SResourceFactory* factory, const char* extension)
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
    SNPRINTF(buf, RESOURCE_PATH_MAX, "%s/%s", base_dir, relative_dir);

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

HFactory NewFactory(uint32_t max_resources, const char* resource_path)
{
    SResourceFactory* factory = new SResourceFactory;
    factory->m_ResourceTypesCount = 0;

    const uint32_t table_size = (3 *max_resources) / 4;
    factory->m_Resources = new THashTable<uint64_t, SResourceDescriptor>(table_size);
    factory->m_Resources->SetCapacity(max_resources);

    factory->m_ResourceToHash = new THashTable<uintptr_t, uint64_t>(table_size);
    factory->m_ResourceToHash->SetCapacity(max_resources);

    strncpy(factory->m_ResourcePath, resource_path, RESOURCE_PATH_MAX);
    factory->m_ResourcePath[RESOURCE_PATH_MAX-1] = '\0';

    return factory;
}

void DeleteFactory(HFactory factory)
{
    delete factory->m_Resources;
    delete factory->m_ResourceToHash;
    delete factory;
}

FactoryError RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           FResourceCreate create_function,
                           FResourceDestroy destroy_function)
{
    if (factory->m_ResourceTypesCount == MAX_RESOURCE_TYPES)
        return FACTORY_ERROR_OUT_OF_RESOURCES;

    // Dots not allowed in extension
    if (strrchr(extension, '.') != 0)
        return FACTORY_ERROR_INVAL;

    if (create_function == 0 || destroy_function == 0)
        return FACTORY_ERROR_INVAL;

    if (FindResourceType(factory, extension) != 0)
        return FACTORY_ERROR_ALREADY_REGISTERED;

    SResourceType resource_type;
    resource_type.m_Extension = extension;
    resource_type.m_Context = context;
    resource_type.m_CreateFunction = create_function;
    resource_type.m_DestroyFunction = destroy_function;

    factory->m_ResourceTypes[factory->m_ResourceTypesCount++] = resource_type;

    return FACTORY_ERROR_OK;
}

FactoryError Get(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);

    *resource = 0;

#if 1
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_ResourcePath, name, canonical_path);

#else
#ifdef _WIN32
    char canonical_path[_PATH_MAX];
    char *canonical_path_p = _fullpath(canonical_path, name, _PATH_MAX);
#else
    char canonical_path[PATH_MAX];
    char *canonical_path_p = realpath(name, canonical_path);
#endif

    if (canonical_path_p == 0)
    {
        return FACTORY_ERROR_RESOURCE_NOT_FOUND;
    }
#endif

    uint64_t canonical_path_hash = HashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);
    if (rd)
    {
        assert(factory->m_ResourceToHash->Get((uintptr_t) rd->m_Resource));
        rd->m_ReferenceCount++;
        *resource = rd->m_Resource;
        return FACTORY_ERROR_OK;
    }

    const char* ext = strrchr(name, '.');
    if (ext)
    {
        ext++;

        SResourceType* resource_type = FindResourceType(factory, ext);
        if (resource_type == 0)
        {
            LogError("Unknown resource type: %s", ext);
            return FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE;
        }

        FILE* f = fopen(canonical_path, "rb");
        if (f == 0)
        {
            LogWarning("Resource not found: %s", canonical_path);
            return FACTORY_ERROR_RESOURCE_NOT_FOUND;
        }

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void* buffer = malloc(file_size+1); // Extra byte for resources expecting null-terminated string...
        if (buffer == 0)
        {
            LogError("Out of memory: %s", ext);
            return FACTORY_ERROR_OUT_OF_MEMORY;
            fclose(f);
        }
        ((char*) buffer)[file_size] = 0; // Null-terminate. See comment above

        if (fread(buffer, 1, file_size, f) != file_size)
        {
            free(buffer);
            fclose(f);
            return FACTORY_ERROR_IO_ERROR;
        }

        // TODO: We should *NOT* allocate SResource dynamically...
        SResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));
        tmp_resource.m_NameHash = canonical_path_hash;
        tmp_resource.m_ReferenceCount = 1;
        tmp_resource.m_ResourceType = (void*) resource_type;

        CreateError create_error = resource_type->m_CreateFunction(factory, resource_type->m_Context, buffer, file_size, &tmp_resource);
        free(buffer);
        fclose(f);

        if (create_error == CREATE_ERROR_OK)
        {
            assert(tmp_resource.m_Resource); // TODO: Or handle gracefully!
            factory->m_Resources->Put(canonical_path_hash, tmp_resource);
            factory->m_ResourceToHash->Put((uintptr_t) tmp_resource.m_Resource, canonical_path_hash);
            *resource = tmp_resource.m_Resource;
            return FACTORY_ERROR_OK;
        }
        else
        {
            LogWarning("Unable ot create resource: %s", canonical_path);
            // TODO: Map CreateError to FactoryError here.
            return FACTORY_ERROR_UNKNOWN;
        }
    }
    else
    {
        return FACTORY_ERROR_MISSING_FILE_EXTENSION;
    }
}

FactoryError GetType(HFactory factory, void* resource, uint32_t* type)
{
    assert(type);

    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    if (!resource_hash)
    {
        return FACTORY_ERROR_NOT_LOADED;
    }

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    *type = (uint32_t) rd->m_ResourceType; // TODO: Not 64-bit friendly...
}

FactoryError GetTypeFromExtension(HFactory factory, const char* extension, uint32_t* type)
{
    assert(type);

    SResourceType* resource_type = FindResourceType(factory, extension);
    if (resource_type)
    {
        *type = (uint32_t) resource_type; // TODO: Not 64-bit friendly...
        return FACTORY_ERROR_OK;
    }
    else
    {
        return FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE;
    }
}

FactoryError GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_ResourcePath, name, canonical_path);

    uint64_t canonical_path_hash = HashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* tmp_descriptor = factory->m_Resources->Get(canonical_path_hash);
    if (tmp_descriptor)
    {
        *descriptor = *tmp_descriptor;
        return FACTORY_ERROR_OK;
    }
    else
    {
        return FACTORY_ERROR_NOT_LOADED;
    }
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
        resource_type->m_DestroyFunction(factory, resource_type->m_Context, rd);

        factory->m_ResourceToHash->Erase((uintptr_t) resource);
        factory->m_Resources->Erase(*resource_hash);
    }
}


}

