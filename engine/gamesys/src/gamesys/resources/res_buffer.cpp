#include "res_buffer.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>

#include <render/render.h>

namespace dmGameSystem
{
    static void ReleaseResources(dmResource::HFactory factory, BufferResource* resource)
    {
        if (resource->m_BufferDDF != 0x0)
            dmDDF::FreeMessage(resource->m_BufferDDF);
        resource->m_BufferDDF = 0x0;
    }

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static bool BuildBuffer(BufferResource* buffer_resource)
    {
        // const dmBuffer::StreamDeclaration streams_decl[] = {
        //     {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
        //     {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_UINT16, 2},
        //     {dmHashString64("color"), dmBuffer::VALUE_TYPE_UINT8, 4},
        // };
        // dmBuffer::HBuffer buffer = 0x0;
        // dmBuffer::Result r = dmBuffer::Create(1024, streams_decl, 3, &buffer);

        // if (r == dmBuffer::RESULT_OK) {
        //     // success
        // } else {
        //     // handle error
        // }
        // buffer_resource

        // Figure out stream count (+ element count, by counting max entries in each stream)
        uint64_t max_elem = 0;
        uint32_t stream_count = buffer_resource->m_BufferDDF->m_Streams.m_Count;
        dmBuffer::StreamDeclaration* streams_decl = (dmBuffer::StreamDeclaration*)malloc(stream_count * sizeof(dmBuffer::StreamDeclaration));
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            const dmBufferDDF::StreamDesc& ddf_stream = buffer_resource->m_BufferDDF->m_Streams[i];
            streams_decl[i].m_Name = dmHashString64(ddf_stream.m_Name);
            streams_decl[i].m_Type = (dmBuffer::ValueType)ddf_stream.m_ValueType;
            streams_decl[i].m_Count = ddf_stream.m_ValueCount;

            assert(streams_decl[i].m_Count > 0);

            // TODO: Look at the correct typed buffer array instead of always float.
            uint64_t elem_count = ddf_stream.m_FloatData.m_Count / streams_decl[i].m_Count;
            if (elem_count > max_elem) {
                max_elem = elem_count;
            }
        }

        buffer_resource->m_ElementCount = max_elem;

        dmBuffer::Result r = dmBuffer::Create(max_elem, streams_decl, stream_count, &buffer_resource->m_Buffer);

        if (r != dmBuffer::RESULT_OK) {
            free(streams_decl);
            return false;
        }

        // Figure out buffer elem count
        for (int s = 0; s < stream_count; ++s)
        {
            dmBuffer::StreamDeclaration& stream_decl = streams_decl[s];
            const dmBufferDDF::StreamDesc& ddf_stream = buffer_resource->m_BufferDDF->m_Streams[s];

            float* positions = 0x0;
            uint32_t count = 0;
            uint32_t components = 0;
            uint32_t stride = 0;

            dmBuffer::Result r = dmBuffer::GetStream(buffer_resource->m_Buffer, stream_decl.m_Name, (void**)&positions, &count, &components, &stride);

            if (r == dmBuffer::RESULT_OK)
            {
                for (int i = 0; i < count; ++i)
                {
                    for (int c = 0; c < components; ++c)
                    {
                        uint64_t data_i = i*components + c;
                        if (data_i < ddf_stream.m_FloatData.m_Count) {
                            positions[c] = ddf_stream.m_FloatData.m_Data[data_i];
                        } else {
                            positions[c] = 0.0f;
                            dmLogError("Trying to get stream data outside of input DDF array.");
                        }
                    }
                    positions += stride;
                }
            } else {
                // TODO Handle error
                assert(false && "WTF");
            }
        }

        // float* positions = 0x0;
        // uint32_t size = 0;
        // uint32_t components = 0;
        // uint32_t stride = 0;
        // dmBuffer::Result r = dmBuffer::GetStream(buffer, dmHashString64("position"), (void**)&positions, &count, &components, &stride);

        // if (r == dmBuffer::RESULT_OK) {
        //     for (int i = 0; i < count; ++i)
        //     {
        //         for (int c = 0; c < components; ++c)
        //         {
        //              positions[c] *= 1.1f;
        //         }
        //         positions += stride;
        //     }
        // } else {
        //     // handle error
        // }

        free(streams_decl);

        return true;
    }

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams& params)
    {
        BufferResource* buffer_resource = new BufferResource();
        memset(buffer_resource, 0, sizeof(BufferResource));
        buffer_resource->m_BufferDDF = (dmBufferDDF::BufferDesc*) params.m_PreloadData;
        params.m_Resource->m_Resource = (void*) buffer_resource;

        if (!BuildBuffer(buffer_resource))
        {
            return dmResource::RESULT_INVALID_DATA;
        }

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams& params)
    {
        BufferResource* buffer_resource = (BufferResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, buffer_resource);
        delete buffer_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        BufferResource* buffer_resource = (BufferResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, buffer_resource);
        buffer_resource->m_BufferDDF = ddf;

        if (!BuildBuffer(buffer_resource))
        {
            return dmResource::RESULT_INVALID_DATA;
        }


        return dmResource::RESULT_OK;
    }
}
