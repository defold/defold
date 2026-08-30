// Copyright 2020-2026 The Defold Foundation
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

#include "res_buffer.h"

#include <dlib/buffer.h>
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

        if (resource->m_Buffer) {
            dmBuffer::Destroy(resource->m_Buffer);
        }
    }

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static uint32_t GetValueCount(const dmBufferDDF::StreamDesc& ddf_stream)
    {
        switch (ddf_stream.m_ValueType)
        {
            case dmBufferDDF::VALUE_TYPE_UINT8:
            case dmBufferDDF::VALUE_TYPE_UINT16:
            case dmBufferDDF::VALUE_TYPE_UINT32:
                return ddf_stream.m_Ui.m_Count;

            case dmBufferDDF::VALUE_TYPE_UINT64:
                return ddf_stream.m_Ui64.m_Count;

            case dmBufferDDF::VALUE_TYPE_INT8:
            case dmBufferDDF::VALUE_TYPE_INT16:
            case dmBufferDDF::VALUE_TYPE_INT32:
                return ddf_stream.m_I.m_Count;

            case dmBufferDDF::VALUE_TYPE_INT64:
                return ddf_stream.m_I64.m_Count;

            case dmBufferDDF::VALUE_TYPE_FLOAT32:
                return ddf_stream.m_F.m_Count;

            default:
                assert(false && "Unknown value type of stream, cannot get value count.");
                return 0;
        }
    }

#define MAKE_STREAM_BUILDER(F_NAME, C_DATA_TYPE, C_DEFAULT, DDF_FIELD) \
    static void Build##F_NAME##Stream(void* out_data, const uint32_t count, uint32_t components, uint32_t stride, const dmBufferDDF::StreamDesc& ddf_stream) \
    { \
        C_DATA_TYPE* out_data_typed = (C_DATA_TYPE*)out_data; \
        for (int i = 0; i < count; ++i) \
        { \
            for (int c = 0; c < components; ++c) \
            { \
                uint64_t data_i = i*components + c; \
                if (data_i < ddf_stream.DDF_FIELD.m_Count) { \
                    out_data_typed[c] = ddf_stream.DDF_FIELD.m_Data[data_i]; \
                } else { \
                    out_data_typed[c] = C_DEFAULT; \
                    dmLogError("Trying to get stream data outside of input DDF array."); \
                } \
            } \
            out_data_typed += stride; \
        } \
    }

    MAKE_STREAM_BUILDER(UINT8, uint8_t, 0, m_Ui)
    MAKE_STREAM_BUILDER(UINT16, uint16_t, 0, m_Ui)
    MAKE_STREAM_BUILDER(UINT32, uint32_t, 0, m_Ui)
    MAKE_STREAM_BUILDER(UINT64, uint64_t, 0, m_Ui64)
    MAKE_STREAM_BUILDER(INT8, int8_t, 0, m_I)
    MAKE_STREAM_BUILDER(INT16, int16_t, 0, m_I)
    MAKE_STREAM_BUILDER(INT32, int32_t, 0, m_I)
    MAKE_STREAM_BUILDER(INT64, int64_t, 0, m_I64)
    MAKE_STREAM_BUILDER(FLOAT32, float, 0.0f, m_F)

#undef MAKE_STREAM_BUILDER

    static bool BuildBuffer(BufferResource* buffer_resource)
    {
        // Figure out stream count (+ element count, by counting max entries in each stream)
        uint64_t max_elem = 0;
        uint32_t stream_count = buffer_resource->m_BufferDDF->m_Streams.m_Count;
        dmBuffer::StreamDeclaration* streams_decl = (dmBuffer::StreamDeclaration*)malloc(stream_count * sizeof(dmBuffer::StreamDeclaration));
        for (uint32_t i = 0; i < stream_count; ++i)
        {
            const dmBufferDDF::StreamDesc& ddf_stream = buffer_resource->m_BufferDDF->m_Streams[i];
            streams_decl[i].m_Name                    = ddf_stream.m_NameHash;
            streams_decl[i].m_Type                    = (dmBuffer::ValueType)ddf_stream.m_ValueType;
            streams_decl[i].m_Count                   = ddf_stream.m_ValueCount;

            assert(streams_decl[i].m_Count > 0);

            uint64_t elem_count = GetValueCount(ddf_stream) / streams_decl[i].m_Count;
            if (elem_count > max_elem) {
                max_elem = elem_count;
            }
        }

        buffer_resource->m_ElementCount = max_elem;

        dmBuffer::Result r = dmBuffer::Create(max_elem, streams_decl, stream_count, &buffer_resource->m_Buffer);

        if (r != dmBuffer::RESULT_OK) {
            dmLogError("Unable to create buffer (%d)", r);
            free(streams_decl);
            return false;
        }

        // Figure out buffer elem count
        for (int s = 0; s < stream_count; ++s)
        {
            dmBuffer::StreamDeclaration& stream_decl = streams_decl[s];
            const dmBufferDDF::StreamDesc& ddf_stream = buffer_resource->m_BufferDDF->m_Streams[s];

            void* data = 0x0;
            uint32_t count = 0;
            uint32_t components = 0;
            uint32_t stride = 0;

            dmBuffer::Result r = dmBuffer::GetStream(buffer_resource->m_Buffer, stream_decl.m_Name, &data, &count, &components, &stride);

            if (r == dmBuffer::RESULT_OK)
            {
                switch (ddf_stream.m_ValueType)
                {
                    case dmBufferDDF::VALUE_TYPE_UINT8:
                        BuildUINT8Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_UINT16:
                        BuildUINT16Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_UINT32:
                        BuildUINT32Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_UINT64:
                        BuildUINT64Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_INT8:
                        BuildINT8Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_INT16:
                        BuildINT16Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_INT32:
                        BuildINT32Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_INT64:
                        BuildINT64Stream(data, count, components, stride, ddf_stream);
                        break;
                    case dmBufferDDF::VALUE_TYPE_FLOAT32:
                        BuildFLOAT32Stream(data, count, components, stride, ddf_stream);
                        break;

                    default:
                        dmLogError("Could not build stream data of unknown type.");
                        free(streams_decl);
                        return false;
                }

            } else {
                dmLogError("Could not get raw bytes from buffer resource.");
                free(streams_decl);
                return false;
            }
        }

        free(streams_decl);

        buffer_resource->m_Stride = dmBuffer::GetStructSize(buffer_resource->m_Buffer);

        return true;
    }

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams* params)
    {
        BufferResource* buffer_resource = new BufferResource();
        memset(buffer_resource, 0, sizeof(BufferResource));
        buffer_resource->m_BufferDDF = (dmBufferDDF::BufferDesc*) params->m_PreloadData;
        ResourceDescriptorSetResource(params->m_Resource, buffer_resource);
        buffer_resource->m_NameHash = dmHashString64(params->m_Filename);

        if (!BuildBuffer(buffer_resource))
        {
            return dmResource::RESULT_INVALID_DATA;
        }

        dmBuffer::GetContentVersion(buffer_resource->m_Buffer, &buffer_resource->m_Version);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams* params)
    {
        BufferResource* buffer_resource = (BufferResource*)ResourceDescriptorGetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, buffer_resource);
        delete buffer_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmBufferDDF::BufferDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmBufferDDF_BufferDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        BufferResource* buffer_resource = (BufferResource*)ResourceDescriptorGetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, buffer_resource);
        buffer_resource->m_BufferDDF = ddf;

        if (!BuildBuffer(buffer_resource))
        {
            return dmResource::RESULT_INVALID_DATA;
        }

        dmBuffer::UpdateContentVersion(buffer_resource->m_Buffer);

        return dmResource::RESULT_OK;
    }
}
