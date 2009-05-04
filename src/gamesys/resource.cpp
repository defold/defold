#include <vector>
#include <assert.h>
#include <string.h>

#ifdef __linux__
#include <limits.h>
#elif defined (__MACH__)
#include <sys/param.h>
#elif defined (_WIN32)
#endif

#include <dlib/hash.h>
#include <dlib/hashtable.h>

#include "resource.h"

/*
 * TODO:
 *
 *  - Resources could be loaded twice if canonical path is different for equivalent files.
 *    We could use realpath or similar function but we want to avoid file accesses when converting
 *    a canonical path to hash value. This functionality is used in the GetDescriptor function.
 *
 *  - If GetCanonicalPath exceeds RESOURCE_PATH_MAX PATH_TOO_LONG should be returned.
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

struct SResourceFactory
{
    THashTable<uint64_t, SResourceDescriptor>*   m_Resources;
    std::vector<SResourceType>                   m_ResourceTypes;
    char                                         m_ResourcePath[RESOURCE_PATH_MAX];
};

static SResourceType* FindResourceType(SResourceFactory* factory, const char* extension)
{
    std::vector<SResourceType>::iterator i;
    for (i = factory->m_ResourceTypes.begin(); i != factory->m_ResourceTypes.end(); ++i)
    {
        if (strcmp(extension, i->m_Extension) == 0)
        {
            return &(*i);
        }
    }
    return 0;
}

// TODO: Test this...
static void GetCanonicalPath(const char* base_dir, const char* relative_dir, char* buf)
{
    snprintf(buf, RESOURCE_PATH_MAX, "%s/%s", base_dir, relative_dir);

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

    factory->m_Resources = new THashTable<uint64_t, SResourceDescriptor>((3 *max_resources) / 4);
    factory->m_Resources->SetCapacity(max_resources);

    strncpy(factory->m_ResourcePath, resource_path, RESOURCE_PATH_MAX);
    factory->m_ResourcePath[RESOURCE_PATH_MAX-1] = '\0';
}

void DeleteFactory(HFactory factory)
{
    delete factory->m_Resources;
    delete factory;
}

FactoryError RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           FResourceCreate create_function,
                           FResourceDestroy destroy_function)
{
    // Dots not allowed in extension
    if (strrchr(extension, '.') != 0)
        return FACTORY_ERROR_INVAL;

    if (create_function == 0 || destroy_function == 0)
        return FACTORY_ERROR_INVAL;

    if (FindResourceType(factory, extension) != 0)
            return FACTORY_ERROR_ALREADY_REGISTRED;

    SResourceType resource_type;
    resource_type.m_Extension = extension;
    resource_type.m_Context = context;
    resource_type.m_CreateFunction = create_function;
    resource_type.m_DestroyFunction = destroy_function;

    factory->m_ResourceTypes.push_back(resource_type);

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

    const char* ext = strrchr(name, '.');
    if (ext)
    {
        ext++;

        SResourceType* resource_type = FindResourceType(factory, ext);
        if (resource_type == 0)
        {
            return FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE;
        }

        FILE* f = fopen(canonical_path, "rb");
        if (f == 0)
            return FACTORY_ERROR_RESOURCE_NOT_FOUND;

        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        void* buffer = malloc(file_size);
        if (buffer == 0)
        {
            return FACTORY_ERROR_OUT_OF_MEMORY;
            fclose(f);
        }

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

        CreateError create_error = resource_type->m_CreateFunction(factory, resource_type->m_Context, buffer, file_size, &tmp_resource);
        free(buffer);
        fclose(f);

        if (create_error == CREATE_ERROR_OK)
        {
            factory->m_Resources->Put(canonical_path_hash, tmp_resource);
            *resource = tmp_resource.m_Resource;
            return FACTORY_ERROR_OK;
        }
        else
        {
            // TODO: Map CreateError to FactoryError here.
            return FACTORY_ERROR_UNKNOWN;
        }
    }
    else
    {

        return FACTORY_ERROR_MISSING_FILE_EXTENSION;
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

}

