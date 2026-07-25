// Standalone helper for creating a minimal .bufferc resource from stream declarations.

#include <stdint.h>
#include <stdlib.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/gamesys/resources/res_buffer.h>
#include <gamesys/buffer_ddf.h>

ResourceResult CreateBufferResourceFromStreams(HResourceFactory factory, const char* path, const dmBuffer::StreamDeclaration* streams_decl, uint32_t streams_decl_count, uint32_t num_vertices, dmGameSystem::BufferResource** out_resource)
{
    dmBufferDDF::StreamDesc* ddf_streams = (dmBufferDDF::StreamDesc*)calloc(streams_decl_count, sizeof(dmBufferDDF::StreamDesc));

    uint64_t total_bytes = 0;

    for (uint32_t i = 0; i < streams_decl_count; ++i)
    {
        const dmBuffer::StreamDeclaration& decl = streams_decl[i];

        dmBufferDDF::StreamDesc& ddf_stream = ddf_streams[i];
        ddf_stream.m_NameHash                = decl.m_Name;
        ddf_stream.m_ValueType               = (dmBufferDDF::ValueType)decl.m_Type;
        ddf_stream.m_ValueCount              = decl.m_Count;
        ddf_stream.m_Name                    = dmHashReverseSafe64(decl.m_Name);

        uint64_t total_values = (uint64_t)num_vertices * (uint64_t)decl.m_Count;

        switch (decl.m_Type)
        {
            case dmBuffer::VALUE_TYPE_UINT8: total_bytes += total_values * sizeof(uint8_t); break;
            case dmBuffer::VALUE_TYPE_UINT16: total_bytes += total_values * sizeof(uint16_t); break;
            case dmBuffer::VALUE_TYPE_UINT32: total_bytes += total_values * sizeof(uint32_t); break;
            case dmBuffer::VALUE_TYPE_UINT64:
                total_bytes += total_values * sizeof(uint64_t);
                break;
            case dmBuffer::VALUE_TYPE_INT8: total_bytes += total_values * sizeof(int8_t); break;
            case dmBuffer::VALUE_TYPE_INT16: total_bytes += total_values * sizeof(int16_t); break;
            case dmBuffer::VALUE_TYPE_INT32: total_bytes += total_values * sizeof(int32_t); break;
            case dmBuffer::VALUE_TYPE_INT64:
                total_bytes += total_values * sizeof(int64_t);
                break;
            case dmBuffer::VALUE_TYPE_FLOAT32:
                total_bytes += total_values * sizeof(float);
                break;
            default:
                break;
        }
    }

    dmBufferDDF::BufferDesc buffer_desc = {};
    buffer_desc.m_Streams.m_Data        = ddf_streams;
    buffer_desc.m_Streams.m_Count       = streams_decl_count;

    dmArray<uint8_t> ddf_buffer;
    ddf_buffer.SetCapacity((uint32_t)total_bytes);
    dmDDF::SaveMessageToArray(&buffer_desc, dmBufferDDF::BufferDesc::m_DDFDescriptor, ddf_buffer);

    ResourceResult result = ResourceCreateResource(factory, path, ddf_buffer.Begin(), ddf_buffer.Size(), (void**)out_resource);

    dmGameSystem::BufferResource* resource = *out_resource;
    dmBuffer::HBuffer new_buffer = 0;
    dmBuffer::Create(num_vertices, streams_decl, (uint8_t)streams_decl_count, &new_buffer);
    dmBuffer::Destroy(resource->m_Buffer);
    resource->m_Buffer = new_buffer;
    resource->m_ElementCount = num_vertices;
    void* bytes_data = 0;
    uint32_t bytes_size = 0;
    dmBuffer::GetBytes(new_buffer, &bytes_data, &bytes_size);
    resource->m_Stride = bytes_size / num_vertices;
    dmBuffer::GetContentVersion(new_buffer, &resource->m_Version);

    free(ddf_streams);
    return result;
}
