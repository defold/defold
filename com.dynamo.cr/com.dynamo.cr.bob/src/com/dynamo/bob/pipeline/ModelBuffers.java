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

package com.dynamo.bob.pipeline;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig.IndexBufferFormat;
import com.dynamo.rig.proto.Rig.Material;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Model;

/**
 * Shared Bob/editor compiler for one immutable geometry resource per model.
 *
 * The build selects meshes with selectMeshes(), deduplicates their geometry with
 * geometries(), and writes GPU-ready bytes with compile(). metadata() replaces
 * the source vertex/index arrays with range indices into that same geometry list,
 * while keeping node identity, transforms, material bindings and morph metadata.
 * compile() and metadata() must receive the same ordered geometry list.
 *
 * The runtime reader is gamesys/resources/res_model_buffers.cpp. Keep the binary
 * format and attribute layout in sync with that reader and the rig vertex structs.
 */
public final class ModelBuffers {
    private static final String MAGIC = "DMMB";
    static final int VERSION = 2;
    /** Identifies the serialized format in Bob and editor build-cache keys. */
    public static final String FORMAT_ID = MAGIC + "-v" + VERSION;
    private static final int HEADER_SIZE = 36;
    private static final int RANGE_SIZE = 28;
    private static final int STATIC_VERTEX_STRIDE = 72;
    private static final int SKINNED_VERTEX_STRIDE = 104;

    // Attribute indices are also the DMMB mask bit positions.
    private static final int ATTRIBUTE_POSITION = 0;
    private static final int ATTRIBUTE_NORMAL = 1;
    private static final int ATTRIBUTE_TANGENT = 2;
    private static final int ATTRIBUTE_COLOR = 3;
    private static final int ATTRIBUTE_TEXCOORD0 = 4;
    private static final int ATTRIBUTE_TEXCOORD1 = 5;
    private static final int ATTRIBUTE_WEIGHTS = 6;
    private static final int ATTRIBUTE_BONE_INDICES = 7;
    private static final int ATTRIBUTE_MASK_SKINNING = (1 << ATTRIBUTE_WEIGHTS) | (1 << ATTRIBUTE_BONE_INDICES);
    // Scalar values per vertex attribute in the packed layout. Array indices match
    // attribute-mask bit positions, not the flag values themselves. Missing source
    // attributes within the selected layout still occupy fixed-width GPU slots;
    // attributeMask() records which streams to restore when CPU geometry is requested.
    private static final int[] ATTRIBUTE_COMPONENT_COUNTS = {
        3, // Position: x, y, z.
        3, // Normal: x, y, z.
        4, // Tangent: x, y, z, handedness.
        4, // Color: red, green, blue, alpha.
        2, // Texcoord0: u, v.
        2, // Texcoord1: u, v.
        4, // Skin weights: four bone influences.
        4  // Bone indices: four indices, stored as floats for the GPU layout.
    };

    private ModelBuffers() {}

    private static int align(int value, int alignment) {
        return Math.addExact(value, alignment - 1) & -alignment;
    }

    // Tracks source streams, including the paired skinning streams that select
    // the larger vertex layout. It does not strip unused GPU attribute slots.
    private static int attributeMask(Mesh mesh) {
        int mask = 1 << ATTRIBUTE_POSITION;
        if (mesh.getNormalsCount() != 0) mask |= 1 << ATTRIBUTE_NORMAL;
        if (mesh.getTangentsCount() != 0) mask |= 1 << ATTRIBUTE_TANGENT;
        if (mesh.getColorsCount() != 0) mask |= 1 << ATTRIBUTE_COLOR;
        if (mesh.getTexcoord0Count() != 0) mask |= 1 << ATTRIBUTE_TEXCOORD0;
        if (mesh.getTexcoord1Count() != 0) mask |= 1 << ATTRIBUTE_TEXCOORD1;
        if (mesh.getWeightsCount() != 0) mask |= 1 << ATTRIBUTE_WEIGHTS;
        if (mesh.getBoneIndicesCount() != 0) mask |= 1 << ATTRIBUTE_BONE_INDICES;
        return mask;
    }

    // Only vertex/index content contributes to sharing. Bounds, bindings and
    // morph metadata remain on each mesh in the compiled model metadata.
    private static Mesh geometry(Mesh mesh) {
        return mesh.toBuilder()
                .setAabbMin(Vector3.getDefaultInstance()).setAabbMax(Vector3.getDefaultInstance())
                .setIndicesFormat(mesh.getIndicesFormat())
                .clearMaterialIndex().clearMorphBaseWeights().clearMorphTargetTexture().clearMorphTargetCount()
                .clearPrimitiveType().clearNumTexcoord0Components().clearNumTexcoord1Components()
                .clearBufferIndex().clearVertexCount().build();
    }

    /** Keeps all runtime-selectable scene meshes, or one explicit source-mesh selection. */
    public static MeshSet selectMeshes(MeshSet source, int meshIndex) {
        MeshSet.Builder result = source.toBuilder().clearModels().clearRawModels();
        for (Model model : source.getModelsList()) {
            if (meshIndex < 0 || model.getMeshIndex() == meshIndex) result.addModels(model);
        }
        if (meshIndex >= 0) {
            for (Model model : source.getRawModelsList()) {
                if (model.getMeshIndex() == meshIndex) result.addRawModels(model);
            }
        }
        return result.build();
    }

    /** Canonical unique geometry order, independent of node names, placements and bindings. */
    public static List<Mesh> geometries(MeshSet source) {
        Map<Long, Mesh> geometries = new TreeMap<>();
        for (List<Model> models : List.of(source.getModelsList(), source.getRawModelsList())) {
            for (Model model : models) {
                for (Mesh mesh : model.getMeshesList()) {
                    Mesh geometry = geometry(mesh);
                    byte[] bytes = geometry.toByteArray();
                    geometries.putIfAbsent(MurmurHash.hash64(bytes, bytes.length), geometry);
                }
            }
        }
        return List.copyOf(geometries.values());
    }

    private static void putAttribute(ByteBuffer out, List<Float> values, int vertex, int components, float defaultValue) {
        for (int c = 0; c < components; ++c) {
            out.putFloat(values.isEmpty() ? defaultValue : values.get(vertex * components + c));
        }
    }

    /**
     * DMMB v2: a 36-byte header, 28-byte range descriptors, then vertex and index
     * payloads. All meshes share one GPU vertex buffer and one GPU index buffer.
     * Each range describes its byte offsets, vertex count/stride, index count/type
     * and source-attribute mask. Node identity and bindings remain in .modelc.
     *
     * Header and descriptor integers are little-endian uint32 values; offsets
     * and sizes are bytes.
     * <pre>
     * File header:
     *   +0  magic: four literal bytes "DMMB"
     *   +4  format version
     *   +8  compression: 0 (archive compression is handled by the resource loader)
     *   +12 decoded payload size: vertex size + index size
     *   +16 range count
     *   +20 vertex payload offset, relative to the file start
     *   +24 vertex payload size
     *   +28 index payload offset, relative to the file start
     *   +32 index payload size
     * Each range descriptor, starting immediately after the header:
     *   +0  vertex offset, relative to the vertex payload
     *   +4  vertex count
     *   +8  vertex stride
     *   +12 index offset, relative to the index payload
     *   +16 index count
     *   +20 index element size: 2 or 4 bytes
     *   +24 source-attribute mask
     * </pre>
     *
     * Vertices store float32 attributes in ATTRIBUTE_COMPONENT_COUNTS order.
     * Static vertices use the first six attributes (72 bytes), matching
     * dmRig::RigModelVertex. Skinned vertices append weights and bone indices
     * (104 bytes total), matching dmRig::RigModelSkinnedVertex. Missing attributes
     * are filled with zero, except colors which default to white. The mask keeps
     * this distinction so CPU recovery restores only the original source streams.
     * Vertex payload/range starts align to 16 bytes; index ranges align to 4 bytes.
     *
     * @param geometries canonical list from geometries(), also passed to metadata()
     * @return complete .modelbuffersc contents, including alignment padding
     */
    public static byte[] compile(List<Mesh> geometries) {
        int count = geometries.size();
        int[] vertexOffsets = new int[count];
        int[] indexOffsets = new int[count];
        int vertexSize = 0;
        int indexSize = 0;
        for (int i = 0; i < count; ++i) {
            Mesh mesh = geometries.get(i);
            int stride = (attributeMask(mesh) & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING ? SKINNED_VERTEX_STRIDE : STATIC_VERTEX_STRIDE;
            vertexOffsets[i] = align(vertexSize, 16);
            indexOffsets[i] = align(indexSize, 4);
            vertexSize = Math.addExact(vertexOffsets[i], Math.multiplyExact(mesh.getPositionsCount() / ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_POSITION], stride));
            indexSize = Math.addExact(indexOffsets[i], mesh.getIndices().size());
        }
        vertexSize = align(vertexSize, 16);
        int vertexStart = align(Math.addExact(HEADER_SIZE, Math.multiplyExact(count, RANGE_SIZE)), 16);
        int indexStart = Math.addExact(vertexStart, vertexSize);
        ByteBuffer out = ByteBuffer.allocate(Math.addExact(indexStart, indexSize)).order(ByteOrder.LITTLE_ENDIAN);
        out.put(MAGIC.getBytes(StandardCharsets.US_ASCII)).putInt(VERSION);
        out.putInt(0); // Archive compression is handled by the resource loader.
        out.putInt(Math.addExact(vertexSize, indexSize));
        out.putInt(count).putInt(vertexStart).putInt(vertexSize).putInt(indexStart).putInt(indexSize);
        for (int i = 0; i < count; ++i) {
            Mesh mesh = geometries.get(i);
            int mask = attributeMask(mesh);
            boolean skinned = (mask & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING;
            int vertexCount = mesh.getPositionsCount() / ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_POSITION];
            int stride = skinned ? SKINNED_VERTEX_STRIDE : STATIC_VERTEX_STRIDE;
            int indexElementSize = mesh.getIndicesFormat() == IndexBufferFormat.INDEXBUFFER_FORMAT_16 ? Short.BYTES : Integer.BYTES;
            out.position(HEADER_SIZE + i * RANGE_SIZE);
            out.putInt(vertexOffsets[i]).putInt(vertexCount).putInt(stride);
            out.putInt(indexOffsets[i]).putInt(mesh.getIndices().size() / indexElementSize).putInt(indexElementSize).putInt(mask);
            out.position(vertexStart + vertexOffsets[i]);
            for (int v = 0; v < vertexCount; ++v) {
                putAttribute(out, mesh.getPositionsList(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_POSITION], 0);
                putAttribute(out, mesh.getNormalsList(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_NORMAL], 0);
                putAttribute(out, mesh.getTangentsList(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_TANGENT], 0);
                putAttribute(out, mesh.getColorsList(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_COLOR], 1);
                putAttribute(out, mesh.getTexcoord0List(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_TEXCOORD0], 0);
                putAttribute(out, mesh.getTexcoord1List(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_TEXCOORD1], 0);
                if (skinned) {
                    putAttribute(out, mesh.getWeightsList(), v, ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_WEIGHTS], 0);
                    int boneIndexComponents = ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_BONE_INDICES];
                    for (int c = 0; c < boneIndexComponents; ++c) out.putFloat(mesh.getBoneIndices(v * boneIndexComponents + c));
                }
            }
            mesh.getIndices().copyTo(out.array(), indexStart + indexOffsets[i]);
        }
        return out.array();
    }

    /**
     * Compiles model metadata with shared geometry ranges and material identities.
     * The geometry list must be the same ordered list passed to compile(): its
     * indices become buffer_index values used by the runtime. Vertex/index arrays
     * are removed from the result; model placement, bounds, materials and morph
     * metadata are retained. The source mesh set is not modified.
     */
    public static MeshSet metadata(MeshSet source, List<Mesh> geometries) {
        Map<Mesh, Integer> indices = new HashMap<>();
        for (int i = 0; i < geometries.size(); ++i) indices.put(geometry(geometries.get(i)), i);
        MeshSet.Builder result = source.toBuilder();
        for (Material.Builder material : result.getMaterialsBuilderList()) {
            byte[] values = material.clone().clearName().clearIndex().clearIsSkinned().clearMaterialHash().build().toByteArray();
            material.setMaterialHash(MurmurHash.hash64(values, values.length));
        }
        for (List<Model.Builder> models : List.of(result.getModelsBuilderList(), result.getRawModelsBuilderList())) {
            for (Model.Builder model : models) {
                for (int i = 0; i < model.getMeshesCount(); ++i) {
                    Mesh mesh = model.getMeshes(i);
                    model.setMeshes(i, mesh.toBuilder()
                            .setBufferIndex(indices.get(geometry(mesh)))
                            .setVertexCount(mesh.getPositionsCount() / ATTRIBUTE_COMPONENT_COUNTS[ATTRIBUTE_POSITION])
                            .clearPositions().clearNormals().clearTangents().clearColors()
                            .clearTexcoord0().clearTexcoord1().clearWeights().clearBoneIndices().clearIndices());
                }
            }
        }
        return result.build();
    }
}
