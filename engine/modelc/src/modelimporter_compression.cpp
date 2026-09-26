// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#include "modelimporter_compression.h"

#include <meshoptimizer/meshoptimizer.h>
#include <draco/compression/decode.h>
#include <draco/mesh/mesh_stripifier.h>
#include <iterator>
#include <vector>
#include <stdio.h>
#include <string.h>

namespace dmModelImporter
{
    namespace Compression
    {
        // Copies a diagnostic when storage is supplied and returns false for
        // direct propagation through the wrapper's boolean result functions.
        static bool Error(char* error, size_t size, const char* message)
        {
            if (size)
                snprintf(error, size, "%s", message);
            return false;
        }

        bool ValidateMeshoptBuffer(const MeshoptBuffer& buffer, size_t output_size, char* error, size_t error_size)
        {
            if (buffer.m_Count == 0 || buffer.m_Stride == 0 || output_size > 512U * 1024U * 1024U ||
                buffer.m_Count > output_size / buffer.m_Stride || buffer.m_Count * buffer.m_Stride != output_size)
                return Error(error, error_size, "Invalid decoded size, count, or stride.");
            if (buffer.m_Extension != EXT_MESHOPT && buffer.m_Extension != KHR_MESHOPT)
                return Error(error, error_size, "Unknown Meshopt extension.");
            if (buffer.m_Mode == MODE_ATTRIBUTES)
            {
                if (buffer.m_Stride % 4 || buffer.m_Stride > 256)
                    return Error(error, error_size, "Attribute stride must be a multiple of four, at most 256.");
            }
            else if (buffer.m_Mode == MODE_TRIANGLES || buffer.m_Mode == MODE_INDICES)
            {
                if ((buffer.m_Stride != 2 && buffer.m_Stride != 4) || buffer.m_Filter != FILTER_NONE ||
                    (buffer.m_Mode == MODE_TRIANGLES && buffer.m_Count % 3))
                    return Error(error, error_size, "Invalid index stride, count, or filter.");
            }
            else
                return Error(error, error_size, "Unknown Meshopt mode.");

            switch (buffer.m_Filter)
            {
                case FILTER_NONE:
                case FILTER_EXPONENTIAL:
                    break;
                case FILTER_OCTAHEDRAL:
                case FILTER_COLOR:
                    if ((buffer.m_Stride != 4 && buffer.m_Stride != 8) ||
                        (buffer.m_Filter == FILTER_COLOR && buffer.m_Extension != KHR_MESHOPT))
                        return Error(error, error_size, "Invalid filter stride or extension.");
                    break;
                case FILTER_QUATERNION:
                    if (buffer.m_Stride != 8)
                        return Error(error, error_size, "Quaternion filter requires stride eight.");
                    break;
                default:
                    return Error(error, error_size, "Unknown Meshopt filter.");
            }
            return true;
        }

        bool DecodeMeshoptBuffer(const MeshoptBuffer& buffer, const void* source, size_t source_size, void* output, size_t output_size, char* error, size_t error_size)
        {
            if (!ValidateMeshoptBuffer(buffer, output_size, error, error_size))
                return false;
            if (!source || !source_size || !output)
                return Error(error, error_size, "Missing compressed or output buffer.");
            const unsigned char* bytes = (const unsigned char*)source;
            int                  result;
            if (buffer.m_Mode == MODE_ATTRIBUTES)
            {
                if (bytes[0] != 0xa0 && !(buffer.m_Extension == KHR_MESHOPT && bytes[0] == 0xa1))
                    return Error(error, error_size, "Unsupported attribute bitstream version for this extension.");
                result = meshopt_decodeVertexBuffer(output, buffer.m_Count, buffer.m_Stride, bytes, source_size);
            }
            else if (buffer.m_Mode == MODE_TRIANGLES)
            {
                if (bytes[0] != 0xe1)
                    return Error(error, error_size, "Unsupported triangle bitstream version.");
                result = meshopt_decodeIndexBuffer(output, buffer.m_Count, buffer.m_Stride, bytes, source_size);
            }
            else
            {
                if (bytes[0] != 0xd1)
                    return Error(error, error_size, "Unsupported index bitstream version.");
                result = meshopt_decodeIndexSequence(output, buffer.m_Count, buffer.m_Stride, bytes, source_size);
            }
            if (result != 0)
                return Error(error, error_size, "Invalid or truncated compressed buffer.");
            switch (buffer.m_Filter)
            {
                case FILTER_OCTAHEDRAL:
                    meshopt_decodeFilterOct(output, buffer.m_Count, buffer.m_Stride);
                    break;
                case FILTER_QUATERNION:
                    meshopt_decodeFilterQuat(output, buffer.m_Count, buffer.m_Stride);
                    break;
                case FILTER_EXPONENTIAL:
                    meshopt_decodeFilterExp(output, buffer.m_Count, buffer.m_Stride);
                    break;
                case FILTER_COLOR:
                    meshopt_decodeFilterColor(output, buffer.m_Count, buffer.m_Stride);
                    break;
                default:
                    break;
            }
            return true;
        }

        struct DracoMesh
        {
            draco::Mesh           m_Mesh;
            std::vector<uint32_t> m_Strip;
            bool                  m_TriangleStrip;
        };

        void DestroyDracoMesh(HDracoMesh mesh)
        {
            delete mesh;
        }

        HDracoMesh DecodeDracoMesh(const void* source, size_t source_size, bool triangle_strip, DracoMeshInfo* info, char* error, size_t error_size)
        {
            if (!source || !source_size)
            {
                Error(error, error_size, "Missing compressed mesh.");
                return 0;
            }
            DracoMesh* mesh = new DracoMesh;
            mesh->m_TriangleStrip = triangle_strip;
            draco::DecoderBuffer buffer;
            buffer.Init((const char*)source, source_size);
            draco::Decoder decoder;
            draco::Status  status = decoder.DecodeBufferToGeometry(&buffer, &mesh->m_Mesh);
            if (!status.ok())
            {
                Error(error, error_size, status.error_msg());
                delete mesh;
                return 0;
            }
            const uint32_t MAX_ELEMENTS = 512U * 1024U * 1024U / 16U;
            if (mesh->m_Mesh.num_points() == 0 || mesh->m_Mesh.num_points() > MAX_ELEMENTS ||
                mesh->m_Mesh.num_faces() > MAX_ELEMENTS / (triangle_strip ? 6U : 3U))
            {
                Error(error, error_size, "Decoded mesh exceeds the model importer limit.");
                delete mesh;
                return 0;
            }
            info->m_VertexCount = mesh->m_Mesh.num_points();
            info->m_IndexCount = mesh->m_Mesh.num_faces() * 3;
            if (triangle_strip)
            {
                draco::MeshStripifier stripifier;
                if (!stripifier.GenerateTriangleStripsWithDegenerateTriangles(mesh->m_Mesh, std::back_inserter(mesh->m_Strip)))
                {
                    Error(error, error_size, "Could not generate triangle strips.");
                    delete mesh;
                    return 0;
                }
                info->m_IndexCount = (uint32_t)mesh->m_Strip.size();
            }
            return mesh;
        }

        // Limits exposed Draco storage types to those supported by glTF accessors.
        static ComponentType ToComponentType(draco::DataType type)
        {
            switch (type)
            {
                case draco::DT_INT8:
                    return TYPE_INT8;
                case draco::DT_UINT8:
                    return TYPE_UINT8;
                case draco::DT_INT16:
                    return TYPE_INT16;
                case draco::DT_UINT16:
                    return TYPE_UINT16;
                case draco::DT_UINT32:
                    return TYPE_UINT32;
                case draco::DT_FLOAT32:
                    return TYPE_FLOAT32;
                default:
                    return TYPE_INVALID;
            }
        }

        bool GetDracoAttributeInfo(HDracoMesh mesh, uint32_t unique_id, DracoAttributeInfo* info)
        {
            const draco::PointAttribute* attribute = mesh->m_Mesh.GetAttributeByUniqueId(unique_id);
            if (!attribute)
                return false;
            info->m_Type = ToComponentType(attribute->data_type());
            info->m_ComponentCount = attribute->num_components();
            return info->m_Type != TYPE_INVALID;
        }

        bool ReadDracoAttribute(HDracoMesh mesh, uint32_t unique_id, void* output, size_t output_size)
        {
            const draco::PointAttribute* attribute = mesh->m_Mesh.GetAttributeByUniqueId(unique_id);
            if (!attribute || !output)
                return false;
            size_t element_size = draco::DataTypeLength(attribute->data_type()) * attribute->num_components();
            if (output_size != size_t(mesh->m_Mesh.num_points()) * element_size)
                return false;
            for (uint32_t i = 0; i < mesh->m_Mesh.num_points(); ++i)
            {
                draco::AttributeValueIndex index = attribute->mapped_index(draco::PointIndex(i));
                if (index.value() >= attribute->size())
                    return false;
                memcpy((uint8_t*)output + i * element_size, attribute->GetAddress(index), element_size);
            }
            return true;
        }

        bool ReadDracoIndices(HDracoMesh mesh, ComponentType type, void* output, size_t output_size)
        {
            size_t count = mesh->m_TriangleStrip ? mesh->m_Strip.size() : size_t(mesh->m_Mesh.num_faces()) * 3;
            size_t stride;
            switch (type)
            {
                case TYPE_UINT8:
                    stride = 1;
                    break;
                case TYPE_UINT16:
                    stride = 2;
                    break;
                case TYPE_UINT32:
                    stride = 4;
                    break;
                default:
                    return false;
            }
            if (!output || output_size != count * stride)
                return false;
            for (size_t i = 0; i < count; ++i)
            {
                uint32_t index = mesh->m_TriangleStrip ? mesh->m_Strip[i] :
                                                         mesh->m_Mesh.face(draco::FaceIndex((uint32_t)(i / 3)))[i % 3].value();
                if (index >= mesh->m_Mesh.num_points() || (stride == 1 && index > UINT8_MAX) || (stride == 2 && index > UINT16_MAX))
                    return false;
                if (stride == 1)
                    ((uint8_t*)output)[i] = (uint8_t)index;
                else if (stride == 2)
                    ((uint16_t*)output)[i] = (uint16_t)index;
                else
                    ((uint32_t*)output)[i] = index;
            }
            return true;
        }
    } // namespace Compression
} // namespace dmModelImporter
