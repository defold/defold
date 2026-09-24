// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#include "modelimporter.h"
#include "modelimporter_tangents.h"

#include <meshoptimizer/meshoptimizer.h>
#include <float.h>
#include <string.h>

namespace dmModelImporter
{
    // Each output vertex retains the original vertex's skinning and morph data,
    // even when its different per-corner tangents require separate vertices.
    template <typename T>
    static void RemapTangentStream(dmArray<T>& stream, uint32_t components, const dmArray<uint32_t>& vertices)
    {
        if (stream.Empty())
            return;
        dmArray<T> output;
        output.SetCapacity(vertices.Size() * components);
        output.SetSize(vertices.Size() * components);
        for (uint32_t i = 0; i < vertices.Size(); ++i)
            memcpy(&output[i * components], &stream[vertices[i] * components], components * sizeof(T));
        stream.Swap(output);
    }

    bool GenerateTangents(Mesh* mesh, const float* texcoords)
    {
        uint32_t source_count = mesh->m_Indices.Empty() ? mesh->m_VertexCount : mesh->m_Indices.Size();
        uint64_t index_count = source_count;
        if (mesh->m_PrimitiveType == PRIMITIVE_TYPE_TRIANGLE_STRIP && source_count >= 3)
            index_count = uint64_t(source_count - 2) * 3;
        else if (mesh->m_PrimitiveType != PRIMITIVE_TYPE_TRIANGLES || source_count % 3)
            return false;

        // Match the importer's per-accessor storage limit, including the largest
        // output stream (float4 tangents) after splitting tangent seams.
        if (!index_count || index_count > (512U * 1024U * 1024U) / (4 * sizeof(float)))
            return false;

        dmArray<uint32_t> indices;
        indices.SetCapacity((uint32_t)index_count);
        indices.SetSize((uint32_t)index_count);
        for (uint32_t i = 0; i < indices.Size(); ++i)
        {
            uint32_t corner = i;
            if (mesh->m_PrimitiveType == PRIMITIVE_TYPE_TRIANGLE_STRIP)
            {
                uint32_t face = i / 3;
                uint32_t local = i % 3;
                corner = face + ((face & 1) && local < 2 ? 1 - local : local);
            }
            indices[i] = mesh->m_Indices.Empty() ? corner : mesh->m_Indices[corner];
            if (indices[i] >= mesh->m_VertexCount)
                return false;
        }

        dmArray<float> corners;
        corners.SetCapacity(indices.Size() * 4);
        corners.SetSize(indices.Size() * 4);
        meshopt_generateTangents(corners.Begin(), indices.Begin(), indices.Size(),
            mesh->m_Positions.Begin(), mesh->m_VertexCount, 3 * sizeof(float),
            mesh->m_Normals.Begin(), 3 * sizeof(float), texcoords, 2 * sizeof(float), meshopt_TangentCompatible);
        for (uint32_t i = 0; i < corners.Size(); ++i)
        {
            if (!(corners[i] >= -FLT_MAX && corners[i] <= FLT_MAX))
                return false;
        }

        // Weld only corners with both the same source vertex and tangent. Using
        // the source index also preserves seams in attributes not used by the generator.
        meshopt_Stream streams[] = {
            { indices.Begin(), sizeof(uint32_t), sizeof(uint32_t) },
            { corners.Begin(), 4 * sizeof(float), 4 * sizeof(float) }
        };
        dmArray<uint32_t> remap;
        remap.SetCapacity(indices.Size());
        remap.SetSize(indices.Size());
        uint32_t vertex_count = (uint32_t)meshopt_generateVertexRemapMulti(remap.Begin(), (const unsigned int*)0,
            indices.Size(), indices.Size(), streams, 2);

        dmArray<uint32_t> vertices;
        vertices.SetCapacity(vertex_count);
        vertices.SetSize(vertex_count);
        mesh->m_Tangents.SetCapacity(vertex_count * 4);
        mesh->m_Tangents.SetSize(vertex_count * 4);
        for (uint32_t i = 0; i < indices.Size(); ++i)
        {
            vertices[remap[i]] = indices[i];
            memcpy(&mesh->m_Tangents[remap[i] * 4], &corners[i * 4], 4 * sizeof(float));
        }

        RemapTangentStream(mesh->m_Positions, 3, vertices);
        RemapTangentStream(mesh->m_Normals, 3, vertices);
        RemapTangentStream(mesh->m_Colors, 4, vertices);
        RemapTangentStream(mesh->m_Weights, 4, vertices);
        RemapTangentStream(mesh->m_Bones, 4, vertices);
        RemapTangentStream(mesh->m_TexCoords0, mesh->m_TexCoords0NumComponents, vertices);
        RemapTangentStream(mesh->m_TexCoords1, mesh->m_TexCoords1NumComponents, vertices);
        for (uint32_t i = 0; i < mesh->m_MorphTargets.Size(); ++i)
        {
            RemapTangentStream(mesh->m_MorphTargets[i].m_Positions, 3, vertices);
            RemapTangentStream(mesh->m_MorphTargets[i].m_Normals, 3, vertices);
            RemapTangentStream(mesh->m_MorphTargets[i].m_Tangents, 4, vertices);
        }
        mesh->m_Indices.Swap(remap);
        mesh->m_VertexCount = vertex_count;
        mesh->m_PrimitiveType = PRIMITIVE_TYPE_TRIANGLES;
        return true;
    }
}
