#include "res_mesh.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include "gamesys.h"

namespace dmGameSystem
{

    inline dmGraphics::Type Write(void* stream, dmBuffer::ValueType value_type, uint32_t type_count, void* out_data, uint32_t elem_i, uint32_t t_i)
    {
        switch (value_type)
        {
            case dmBuffer::ValueType::VALUE_TYPE_UINT8:
            {
                uint8_t* stream_typed = (uint8_t*)stream;
                (*(uint8_t*)out_data) = stream_typed[elem_i*type_count+t_i];
            } break;
            case dmBuffer::ValueType::VALUE_TYPE_UINT16:
                return dmGraphics::Type::TYPE_UNSIGNED_SHORT;
            case dmBuffer::ValueType::VALUE_TYPE_UINT32:
                return dmGraphics::Type::TYPE_UNSIGNED_INT;
            case dmBuffer::ValueType::VALUE_TYPE_INT8:
                return dmGraphics::Type::TYPE_BYTE;
            case dmBuffer::ValueType::VALUE_TYPE_INT16:
                return dmGraphics::Type::TYPE_SHORT;
            case dmBuffer::ValueType::VALUE_TYPE_INT32:
                return dmGraphics::Type::TYPE_INT;
            case dmBuffer::ValueType::VALUE_TYPE_FLOAT32:
                return dmGraphics::Type::TYPE_FLOAT;

            default:
                assert(false && "unsupported type");
        }

    }

    static dmGraphics::Type BufferTypeToStreamType(dmBuffer::ValueType value_type)
    {
        switch (value_type)
        {
            case dmBuffer::ValueType::VALUE_TYPE_UINT8:
                return dmGraphics::Type::TYPE_UNSIGNED_BYTE;
            case dmBuffer::ValueType::VALUE_TYPE_UINT16:
                return dmGraphics::Type::TYPE_UNSIGNED_SHORT;
            case dmBuffer::ValueType::VALUE_TYPE_UINT32:
                return dmGraphics::Type::TYPE_UNSIGNED_INT;
            case dmBuffer::ValueType::VALUE_TYPE_INT8:
                return dmGraphics::Type::TYPE_BYTE;
            case dmBuffer::ValueType::VALUE_TYPE_INT16:
                return dmGraphics::Type::TYPE_SHORT;
            case dmBuffer::ValueType::VALUE_TYPE_INT32:
                return dmGraphics::Type::TYPE_INT;
            case dmBuffer::ValueType::VALUE_TYPE_FLOAT32:
                return dmGraphics::Type::TYPE_FLOAT;

            default:
                assert(false && "unsupported type");
        }

    }


    static uint32_t TypeToSize(dmBuffer::ValueType value_type)
    {
        switch (value_type)
        {
            case dmBuffer::ValueType::VALUE_TYPE_UINT8:
            case dmBuffer::ValueType::VALUE_TYPE_INT8:
                return 1;
            case dmBuffer::ValueType::VALUE_TYPE_UINT16:
            case dmBuffer::ValueType::VALUE_TYPE_INT16:
                return 2;
            case dmBuffer::ValueType::VALUE_TYPE_UINT32:
            case dmBuffer::ValueType::VALUE_TYPE_INT32:
            case dmBuffer::ValueType::VALUE_TYPE_FLOAT32:
                return 4;

            default:
                assert(false && "unsupported type");
        }

    }

    // static bool CreateVertDeclFromBuffer(DeclNameToHash* name_to_stream, uint32_t name_to_stream_count, dmBuffer::HBuffer buffer)
    static bool CreateVertDeclFromBuffer(MeshContext* context, MeshResource* resource)
    {
        dmBuffer::HBuffer buffer = resource->m_Mesh->m_BufferPtr;
        uint32_t elem_count = 0;
        assert(dmBuffer::RESULT_OK, dmBuffer::GetElementCount(buffer, &elem_count));
        uint32_t stream_count = dmBuffer::GetNumStreams(buffer);
        dmGraphics::VertexElement* ve = new dmGraphics::VertexElement[stream_count];

        uint32_t vert_entry_size = 0;

        for (uint32_t i = 0; i < stream_count; ++i)
        {
            dmhash_t stream_name;
            assert(dmBuffer::RESULT_OK == dmBuffer::GetStreamName(buffer, i, &stream_name));

            dmBuffer::ValueType type;
            uint32_t type_count;
            assert(dmBuffer::RESULT_OK == dmBuffer::GetStreamType(buffer, stream_name, &type, &type_count));

            vert_entry_size += TypeToSize(type);

            dmGraphics::VertexElement el = {0x0, stream_name, i, type_count, BufferTypeToStreamType(type), false};
            ve[i] = el;
        }

        // TODO copy buffer data
        uint32_t data_size = elem_count * vert_entry_size;
        uint8_t* data = new uint8_t[data_size];
        uint32_t offset = 0;
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            dmhash_t stream_name;
            assert(dmBuffer::RESULT_OK == dmBuffer::GetStreamName(buffer, i, &stream_name));

            dmBuffer::ValueType type;
            uint32_t type_count;
            assert(dmBuffer::RESULT_OK == dmBuffer::GetStreamType(buffer, stream_name, &type, &type_count));

            uint32_t type_size = TypeToSize(type);

            for (int e = 0; e < elem_count; ++e)
            {
                for (int ti = 0; ti < type_count; ++ti)
                {
                    (void*)&data[e*vert_entry_size+ti*type_size];
                }
            }
        }

        resource->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(context->m_RenderContext), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        resource->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(context->m_RenderContext), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        dmGraphics::SetVertexBufferData(resource->m_VertexBuffer, data_size,
                                        data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        return true;
    }

    dmResource::Result AcquireResources(dmResource::HFactory factory, MeshContext* context, MeshResource* resource, const char* filename)
    {
        // dmResource::Result result = dmResource::Get(factory, resource->m_Mesh->m_RigScene, (void**) &resource->m_RigScene);
        // if (result != dmResource::RESULT_OK)
        //     return result;

        dmResource::Result result = dmResource::RESULT_OK;

        if (resource->m_Mesh->m_BufferPtr != 0x0)
        {
            dmLogError("buffer ptr set, should create vert data");
            CreateVertDeclFromBuffer(context, resource);
        }

        if (resource->m_Mesh->m_Material != 0x0)
        {
            result = dmResource::Get(factory, resource->m_Mesh->m_Material, (void**) &resource->m_Material);
            if (result != dmResource::RESULT_OK)
                return result;
        }

        if (resource->m_Mesh->m_Textures.m_Count > 0)
        {
            dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
            memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
            for (uint32_t i = 0; i < resource->m_Mesh->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                const char* texture_path = resource->m_Mesh->m_Textures[i];
                if (*texture_path != 0)
                {
                    dmResource::Result r = dmResource::Get(factory, texture_path, (void**) &textures[i]);
                    if (r != dmResource::RESULT_OK)
                    {
                        if (result == dmResource::RESULT_OK) {
                            result = r;
                        }
                    } else {
                        r = dmResource::GetPath(factory, textures[i], &resource->m_TexturePaths[i]);
                        if (r != dmResource::RESULT_OK) {
                           result = r;
                        }
                    }
                }
            }
            if (result != dmResource::RESULT_OK)
            {
                for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                    if (textures[i]) dmResource::Release(factory, (void*) textures[i]);
                return result;
            }
            memcpy(resource->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);
        }
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshResource* resource)
    {
        if (resource->m_Mesh != 0x0)
            dmDDF::FreeMessage(resource->m_Mesh);
        resource->m_Mesh = 0x0;

        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        resource->m_Material = 0x0;

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resource->m_Textures[i])
                dmResource::Release(factory, (void*)resource->m_Textures[i]);
            resource->m_Textures[i] = 0x0;
        }
    }

    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmMeshDDF::Mesh* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_Mesh_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams& params)
    {
        MeshResource* mesh_resource = new MeshResource();
        mesh_resource->m_Mesh = (dmMeshDDF::Mesh*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, (MeshContext*)params.m_Context, mesh_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) mesh_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, mesh_resource);
            delete mesh_resource;
        }
        return r;
    }

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams& params)
    {
        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, mesh_resource);
        delete mesh_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmMeshDDF::Mesh* ddf = (dmMeshDDF::Mesh*)params.m_Message;
        if( !ddf )
        {
            dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_Mesh_DESCRIPTOR, (void**) &ddf);
            if ( e != dmDDF::RESULT_OK )
            {
                return dmResource::RESULT_FORMAT_ERROR;
            }
        }

        MeshResource* mesh_resource = (MeshResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, mesh_resource);
        mesh_resource->m_Mesh = ddf;

        if (ddf->m_BufferPtr != 0x0)
        {
            dmLogError("mesh recreated with buffer ptr!");
        } else {
            dmLogError("mesh recreated with WITHOUT buffer ptr!");
        }

        return AcquireResources(params.m_Factory, (MeshContext*)params.m_Context, mesh_resource, params.m_Filename);
    }
}
