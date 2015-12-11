#include "resource.h"
#include "resource_private.h"
#include <dlib/sol.h>
#include <dlib/log.h>

extern "C"
{
    #include <sol/reflect.h>
    #include <sol/runtime.h>
}

extern "C"
{
    struct SolResourcePreloadParams
    {
        dmSol::HProxy m_Factory;
        const char* m_Filename;
        void* m_Buffer;
        uint32_t m_DataOffset; 
        uint32_t m_DataSize;
        ::Any m_PreloadData;
        dmSol::Handle m_HintInfo;
    };

    struct SolResourceCreateParams
    {
        dmSol::HProxy m_Factory;
        const char* m_Filename;
        void* m_Buffer;
        uint32_t m_DataOffset; 
        uint32_t m_DataSize;
        ::Any m_PreloadData;
        ::Any m_Resource;
    };

    struct SolResourceDestroyParams
    {
        dmSol::HProxy m_Factory;
        ::Any m_Resource;
    };

    struct SolResourceRecreateParams
    {
        dmSol::HProxy m_Factory;
        const char* m_Filename;
        void* m_Buffer;
        uint32_t m_DataOffset; 
        uint32_t m_DataSize;
        ::Any m_Resource;
    };

    // sol_private.sol
    SolResourcePreloadParams* sol_resource_alloc_preload_params();
    SolResourceCreateParams* sol_resource_alloc_create_params();
    SolResourceDestroyParams* sol_resource_alloc_destroy_params();
    SolResourceRecreateParams* sol_resource_alloc_recreate_params();
}

namespace dmResource
{
    // dummy to create unique "type" pointer
    static uint32_t s_HintInfoHandleType;

    /* ---------------------------------------------------------------------
     * Preloading SOL resources in combination with async threading loader
     * is going to need some care, as this function can get called from a
     * separate loading thread in the case of using the threaded async loader.
     *
     * Solutions?:
     *    1. Threaded Sol GC?
     *    2. Do not GC while using threaded loader
     *    3. Do not let SOL resources implement Preload
     *
     * It is included here anyway for completenes.
     * ---------------------------------------------------------------------
     */

    Result SolResourcePreload(const ResourcePreloadParams& params)
    {
        SResourceType* type = (SResourceType*) params.m_Context;
        if (!type->m_SolResourceFns.m_Preload)
            return RESULT_OK;

        dmLogError("Sol preloading is not yet fully supported.");
        return RESULT_NOT_SUPPORTED;
    }

    Result SolResourceCreate(const ResourceCreateParams& params)
    {
        SResourceType* type = (SResourceType*) params.m_Context;

        SolResourceCreateParams *tmp = sol_resource_alloc_create_params();
        runtime_pin((void*)tmp);

        tmp->m_Factory = GetSolProxy(params.m_Factory);
        tmp->m_PreloadData = reflect_create_reference_any(0, 0); // TODO: Fix this
        tmp->m_Filename = dmSol::StrDup(params.m_Filename);

        if (params.m_IsSolArray)
        {
            tmp->m_Buffer = (void*) params.m_Buffer;
        }
        else
        {
            tmp->m_Buffer = runtime_alloc_array(1, params.m_BufferSize);
            memcpy(tmp->m_Buffer, params.m_Buffer, params.m_BufferSize);
        }
        
        tmp->m_DataOffset = 0;
        tmp->m_DataSize = params.m_BufferSize;

        Result res = type->m_SolResourceFns.m_Create(tmp);
        void *pd = (void*)reflect_get_any_value(tmp->m_Resource);
        if (pd)
        {
            runtime_pin(pd);
            params.m_Resource->m_Resource = pd;
            params.m_Resource->m_SolType = reflect_get_any_type(tmp->m_Resource);
        }

        if (params.m_PreloadData)
        {
            runtime_unpin(params.m_PreloadData);
        }

        if (!params.m_IsSolArray)
        {
            runtime_unpin((void*)tmp->m_Buffer);
        }

        runtime_unpin((void*)tmp->m_Filename);
        runtime_unpin((void*)tmp);
        return res;
    }

    Result SolResourceDestroy(const ResourceDestroyParams& params)
    {
        SResourceDescriptor* resource = params.m_Resource;

        SResourceType* type = (SResourceType*) params.m_Context;
        SolResourceDestroyParams *tmp = sol_resource_alloc_destroy_params();
        runtime_pin((void*)tmp);

        tmp->m_Factory = GetSolProxy(params.m_Factory);
        tmp->m_Resource = ::reflect_create_reference_any(resource->m_SolType, resource->m_Resource);
        Result res = type->m_SolResourceFns.m_Destroy(tmp);

        if (resource->m_Resource)
        {
            runtime_unpin(resource->m_Resource);
            resource->m_Resource = 0;
            resource->m_SolType = 0;
        }

        runtime_unpin((void*)tmp);
        return res;
    }

    Result SolResourceRecreate(const ResourceRecreateParams& params)
    {
        SResourceDescriptor* resource = params.m_Resource;

        SResourceType* type = (SResourceType*) params.m_Context;
        SolResourceRecreateParams *tmp = sol_resource_alloc_recreate_params();
        runtime_pin((void*)tmp);

        tmp->m_Factory = GetSolProxy(params.m_Factory);
        tmp->m_Filename = dmSol::StrDup(params.m_Filename);

        if (params.m_IsSolArray)
        {
            tmp->m_Buffer = (void*) params.m_Buffer;
        }
        else
        {
            tmp->m_Buffer = runtime_alloc_array(1, params.m_BufferSize);
            memcpy(tmp->m_Buffer, params.m_Buffer, params.m_BufferSize);
        }
        
        tmp->m_DataOffset = 0;
        tmp->m_DataSize = params.m_BufferSize;

        tmp->m_Resource = ::reflect_create_reference_any(resource->m_SolType, resource->m_Resource);

        Result res = type->m_SolResourceFns.m_Recreate(tmp);

        if (!params.m_IsSolArray)
        {
            runtime_unpin((void*)tmp->m_Buffer);
        }

        runtime_unpin((void*)tmp->m_Filename);
        runtime_unpin((void*)tmp);
        return res;
    }

    extern "C"
    {
        Result SolResourceGet(dmSol::HProxy factory_proxy, const char* path, ::Any* out)
        {
            HFactory factory = (HFactory) dmSol::ProxyGet(factory_proxy);
            if (!factory || !path || !out)
            {
                return RESULT_INVAL;
            }

            void* res = 0;
            dmResource::Result r = dmResource::Get(factory, path, &res);
            if (r == RESULT_OK)
            {
                dmResource::SResourceDescriptor desc;
                r = GetDescriptor(factory, path, &desc);
                assert(r == RESULT_OK);
                if (desc.m_SolType != 0)
                {
                    *out = ::reflect_create_reference_any(desc.m_SolType, desc.m_Resource);
                }
                else
                {
                    // Note: Here we only rewrite the pointe portion of the pointer; assuming the internal
                    // sol implementation of Get will supply something meaningful.
                    *out = ::reflect_create_reference_any(::reflect_get_any_type(*out), desc.m_Resource);
                }
            }
            return r;
        }

        void SolResourceRelease(dmSol::HProxy factory_proxy, void* res)
        {
            HFactory factory = (HFactory) dmSol::ProxyGet(factory_proxy);
            if (factory)
            {
                Release(factory, res);
            }
        }

        void SolPreloadHint(dmSol::Handle hint_info_handle, const char* path)
        {
            HPreloadHintInfo preloader = (HPreloadHintInfo) dmSol::GetHandle(hint_info_handle, &s_HintInfoHandleType);
            if (preloader)
            {
                PreloadHint(preloader, path);
            }
        }
    }
}
