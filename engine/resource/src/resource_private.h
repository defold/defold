#ifndef RESOURCE_PRIVATE_H
#define RESOURCE_PRIVATE_H

#include <ddf/ddf.h>
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
    };

    typedef dmArray<char> LoadBufferType;

    struct SResourceDescriptor;

    Result CheckSuppliedResourcePath(const char* name);

    // load with default internal buffer and its management, returns buffer ptr in 'buffer'
    Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size);
    // load with own buffer
    Result DoLoadResource(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer);

    Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, SResourceDescriptor* descriptor);
    void GetCanonicalPath(const char* relative_dir, char* buf);
    void GetCanonicalPathFromBase(const char* base_dir, const char* relative_dir, char* buf);

    SResourceDescriptor* GetByHash(HFactory factory, uint64_t canonical_path_hash);
    SResourceType* FindResourceType(SResourceFactory* factory, const char* extension);
    uint32_t GetRefCount(HFactory factory, void* resource);
    uint32_t GetRefCount(HFactory factory, dmhash_t identifier);

    Result DecryptSignatureHash(Manifest* manifest, const uint8_t* pub_key_buf, uint32_t pub_key_len, char*& out_digest, uint32_t &out_digest_len);

    struct PreloadRequest;
    struct PreloadHintInfo
    {
        HPreloader m_Preloader;
        int32_t m_Parent;
    };
}

#endif
