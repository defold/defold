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

#include "modelimporter.h"
#include "modelimporter_tangents.h"

#include <meshoptimizer/meshoptimizer.h>
#include <float.h>
#include <math.h>
#include <string.h>

namespace dmModelImporter
{
    // Each output vertex retains the original vertex's skinning and morph data,
    // even when its different per-corner tangents require separate vertices.
    template <typename T>
    static void RemapTangentStream(dmArray<T>& stream, uint32_t components, const dmArray<uint32_t>& vertices)
    {
        if (stream.Empty())
        {
            return;
        }
        dmArray<T> output;
        output.SetCapacity(vertices.Size() * components);
        output.SetSize(vertices.Size() * components);
        for (uint32_t i = 0; i < vertices.Size(); ++i)
        {
            memcpy(&output[i * components], &stream[vertices[i] * components], components * sizeof(T));
        }
        stream.Swap(output);
    }

    static bool GenerateTangentCorners(const Mesh* mesh, const dmArray<uint32_t>& indices, const float* positions, const float* normals, const float* texcoords, dmArray<float>& corners)
    {
        corners.SetCapacity(indices.Size() * 4);
        corners.SetSize(indices.Size() * 4);
        meshopt_generateTangents(corners.Begin(), indices.Begin(), indices.Size(), positions, mesh->m_VertexCount, 3 * sizeof(float), normals, 3 * sizeof(float), texcoords, 2 * sizeof(float), meshopt_TangentCompatible);
        for (uint32_t i = 0; i < corners.Size(); ++i)
        {
            if (!(corners[i] >= -FLT_MAX && corners[i] <= FLT_MAX))
            {
                return false;
            }
        }
        return true;
    }

    bool GenerateTangents(Mesh* mesh, const float* texcoords, uint32_t texcoord_count)
    {
        if (texcoord_count != uint64_t(mesh->m_VertexCount) * 2)
        {
            return false;
        }

        uint32_t source_count = mesh->m_Indices.Empty() ? mesh->m_VertexCount : mesh->m_Indices.Size();
        uint64_t index_count = source_count;
        if (mesh->m_PrimitiveType == PRIMITIVE_TYPE_TRIANGLE_STRIP && source_count >= 3)
        {
            index_count = uint64_t(source_count - 2) * 3;
        }
        else if (mesh->m_PrimitiveType != PRIMITIVE_TYPE_TRIANGLES || source_count % 3)
        {
            return false;
        }

        // Match the importer's per-accessor storage limit, including the largest
        // output stream (float4 tangents) after splitting tangent seams.
        if (!index_count || index_count > (512U * 1024U * 1024U) / (4 * sizeof(float)))
        {
            return false;
        }

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
            {
                return false;
            }
        }

        dmArray<float> corners;
        if (!GenerateTangentCorners(mesh, indices, mesh->m_Positions.Begin(), mesh->m_Normals.Begin(), texcoords, corners))
        {
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
        uint32_t vertex_count = (uint32_t)meshopt_generateVertexRemapMulti(remap.Begin(), (const unsigned int*)0, indices.Size(), indices.Size(), streams, 2);

        dmArray<float>* morph_corners = mesh->m_MorphTargets.Empty() ? 0 : new dmArray<float>[mesh->m_MorphTargets.Size()];
        dmArray<float> positions;
        dmArray<float> normals;
        dmArray<uint32_t> next_remap;
        for (uint32_t target = 0; target < mesh->m_MorphTargets.Size(); ++target)
        {
            const MorphTarget& morph = mesh->m_MorphTargets[target];
            if (!morph.m_Tangents.Empty() || (morph.m_Positions.Empty() && morph.m_Normals.Empty()))
            {
                continue;
            }

            const float* pose_positions = mesh->m_Positions.Begin();
            const float* pose_normals = mesh->m_Normals.Begin();
            if (!morph.m_Positions.Empty())
            {
                positions.SetCapacity(mesh->m_Positions.Size());
                positions.SetSize(mesh->m_Positions.Size());
                for (uint32_t i = 0; i < positions.Size(); ++i)
                {
                    positions[i] = mesh->m_Positions[i] + morph.m_Positions[i];
                }
                pose_positions = positions.Begin();
            }
            if (!morph.m_Normals.Empty())
            {
                normals.SetCapacity(mesh->m_Normals.Size());
                normals.SetSize(mesh->m_Normals.Size());
                for (uint32_t i = 0; i < normals.Size(); i += 3)
                {
                    float x = mesh->m_Normals[i] + morph.m_Normals[i];
                    float y = mesh->m_Normals[i + 1] + morph.m_Normals[i + 1];
                    float z = mesh->m_Normals[i + 2] + morph.m_Normals[i + 2];
                    float length = sqrtf(x * x + y * y + z * z);
                    float scale = length > 0 ? 1.0f / length : 0.0f;
                    normals[i] = x * scale;
                    normals[i + 1] = y * scale;
                    normals[i + 2] = z * scale;
                }
                pose_normals = normals.Begin();
            }
            if (!GenerateTangentCorners(mesh, indices, pose_positions, pose_normals, texcoords, morph_corners[target]))
            {
                delete[] morph_corners;
                return false;
            }

            // Refine the previous weld with this pose's tangent, retaining seams
            // from every pose without using one remap stream per morph target.
            // glTF morphs only tangent XYZ; handedness remains the base value.
            streams[0].data = remap.Begin();
            streams[1].data = morph_corners[target].Begin();
            streams[1].size = 3 * sizeof(float);
            next_remap.SetCapacity(indices.Size());
            next_remap.SetSize(indices.Size());
            vertex_count = (uint32_t)meshopt_generateVertexRemapMulti(next_remap.Begin(), (const unsigned int*)0, indices.Size(), indices.Size(), streams, 2);
            remap.Swap(next_remap);
        }

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
            MorphTarget& morph = mesh->m_MorphTargets[i];
            RemapTangentStream(morph.m_Positions, 3, vertices);
            RemapTangentStream(morph.m_Normals, 3, vertices);
            if (morph_corners[i].Empty())
            {
                RemapTangentStream(morph.m_Tangents, 4, vertices);
                continue;
            }
            morph.m_Tangents.SetCapacity(vertex_count * 4);
            morph.m_Tangents.SetSize(vertex_count * 4);
            for (uint32_t corner = 0; corner < indices.Size(); ++corner)
            {
                uint32_t vertex = remap[corner];
                for (uint32_t c = 0; c < 3; ++c)
                {
                    morph.m_Tangents[vertex * 4 + c] = morph_corners[i][corner * 4 + c] - corners[corner * 4 + c];
                }
                morph.m_Tangents[vertex * 4 + 3] = 0;
            }
            morph_corners[i].SetCapacity(0);
        }
        delete[] morph_corners;
        mesh->m_Indices.Swap(remap);
        mesh->m_VertexCount = vertex_count;
        mesh->m_PrimitiveType = PRIMITIVE_TYPE_TRIANGLES;
        return true;
    }
} // namespace dmModelImporter
