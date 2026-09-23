// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#ifndef DM_MODELIMPORTER_COMPRESSION_H
#define DM_MODELIMPORTER_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

// Private modelc codec interface. No third-party types cross this boundary.
namespace dmModelImporter
{
    namespace Compression
    {
        enum MeshoptMode
        {
            MODE_ATTRIBUTES,
            MODE_TRIANGLES,
            MODE_INDICES,
            MODE_INVALID
        };
        enum MeshoptFilter
        {
            FILTER_NONE,
            FILTER_OCTAHEDRAL,
            FILTER_QUATERNION,
            FILTER_EXPONENTIAL,
            FILTER_COLOR,
            FILTER_INVALID
        };
        enum MeshoptExtension
        {
            EXT_MESHOPT,
            KHR_MESHOPT
        };
        enum ComponentType
        {
            TYPE_INVALID,
            TYPE_INT8,
            TYPE_UINT8,
            TYPE_INT16,
            TYPE_UINT16,
            TYPE_UINT32,
            TYPE_FLOAT32
        };

        struct MeshoptBuffer
        {
            MeshoptMode      m_Mode;
            MeshoptFilter    m_Filter;
            MeshoptExtension m_Extension;
            size_t           m_Count;
            size_t           m_Stride;
        };

        // Checks decoded size, mode/filter layout, and extension rules without
        // allocating or reading compressed bytes. Returns false with an error.
        // Error buffers may be omitted by passing error_size == 0.
        bool ValidateMeshoptBuffer(const MeshoptBuffer& buffer, size_t output_size, char* error, size_t error_size);

        // Revalidates the descriptor, enforces the extension's bitstream version,
        // then decodes and filters into exactly output_size caller-owned bytes.
        // Source/output must not overlap. On false, error is set and output is unusable.
        bool DecodeMeshoptBuffer(const MeshoptBuffer& buffer, const void* source, size_t source_size, void* output, size_t output_size, char* error, size_t error_size);

        struct DracoMesh;
        typedef DracoMesh* HDracoMesh;
        struct DracoMeshInfo
        {
            uint32_t m_VertexCount;
            uint32_t m_IndexCount;
        };
        struct DracoAttributeInfo
        {
            ComponentType m_Type;
            uint32_t      m_ComponentCount;
        };

        // The returned handle owns all temporary decoder storage. Extraction copies
        // into caller-owned storage, which remains valid after DestroyDracoMesh.
        // Source is needed only during this call. On success, info contains the
        // vertex/index counts for a triangle list or a strip joined by degenerate
        // triangles. Returns null with an error on failure; info is then unusable.
        HDracoMesh DecodeDracoMesh(const void* source, size_t source_size, bool triangle_strip, DracoMeshInfo* info, char* error, size_t error_size);

        // Releases the handle and all codec storage; accepts null.
        void DestroyDracoMesh(HDracoMesh mesh);

        // Queries a valid decoded handle by Draco unique ID, not accessor index.
        // Writes info on success; returns false for a missing attribute or unsupported type.
        bool GetDracoAttributeInfo(HDracoMesh mesh, uint32_t unique_id, DracoAttributeInfo* info);

        // Copies one tightly packed value per point, following Draco's attribute
        // mapping. Storage types are preserved; glTF normalization happens later.
        // Requires a valid handle and vertex count * component count * component size
        // output bytes; false invalidates output.
        bool ReadDracoAttribute(HDracoMesh mesh, uint32_t unique_id, void* output, size_t output_size);

        // Copies list/strip indices from a valid handle into UINT8/16/32 storage.
        // Requires index count * component size output bytes; rejects out-of-range
        // or unrepresentable indices. On false, output may contain a partial result
        // and must be discarded.
        bool ReadDracoIndices(HDracoMesh mesh, ComponentType type, void* output, size_t output_size);
    } // namespace Compression
} // namespace dmModelImporter

#endif
