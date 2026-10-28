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
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig.Material;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Model;

/** Shared Bob/editor compiler for one immutable geometry resource per model. */
public final class ModelBuffers {
    static final int VERSION = 2;
    private static final int HEADER_SIZE = 36;
    private static final int RANGE_SIZE = 28;
    private static final int ATTRIBUTE_POSITION = 1 << 0;
    private static final int ATTRIBUTE_NORMAL = 1 << 1;
    private static final int ATTRIBUTE_TANGENT = 1 << 2;
    private static final int ATTRIBUTE_COLOR = 1 << 3;
    private static final int ATTRIBUTE_TEXCOORD0 = 1 << 4;
    private static final int ATTRIBUTE_TEXCOORD1 = 1 << 5;
    private static final int ATTRIBUTE_WEIGHTS = 1 << 6;
    private static final int ATTRIBUTE_BONE_INDICES = 1 << 7;
    private static final int ATTRIBUTE_MASK_SKINNING = ATTRIBUTE_WEIGHTS | ATTRIBUTE_BONE_INDICES;
    private static final int[] COMPONENTS = {3, 3, 4, 4, 2, 2, 4, 4};

    private ModelBuffers() {}

    private static int align(int value, int alignment) {
        return Math.addExact(value, alignment - 1) & -alignment;
    }

    private static int attributeMask(Mesh mesh) {
        int mask = ATTRIBUTE_POSITION;
        if (mesh.getNormalsCount() != 0) mask |= ATTRIBUTE_NORMAL;
        if (mesh.getTangentsCount() != 0) mask |= ATTRIBUTE_TANGENT;
        if (mesh.getColorsCount() != 0) mask |= ATTRIBUTE_COLOR;
        if (mesh.getTexcoord0Count() != 0) mask |= ATTRIBUTE_TEXCOORD0;
        if (mesh.getTexcoord1Count() != 0) mask |= ATTRIBUTE_TEXCOORD1;
        if (mesh.getWeightsCount() != 0) mask |= ATTRIBUTE_WEIGHTS;
        if (mesh.getBoneIndicesCount() != 0) mask |= ATTRIBUTE_BONE_INDICES;
        return mask;
    }

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
     * MBFC v2: a 36-byte header, 28-byte range descriptors, then vertex and index
     * payloads. All meshes share one GPU vertex buffer and one GPU index buffer.
     * Each range describes its byte offsets, vertex count/stride, index count/type
     * and source-attribute mask. Node identity and bindings remain in .modelc.
     */
    public static byte[] compile(List<Mesh> geometries) {
        int count = geometries.size();
        int[] vertexOffsets = new int[count];
        int[] indexOffsets = new int[count];
        int vertexSize = 0;
        int indexSize = 0;
        for (int i = 0; i < count; ++i) {
            Mesh mesh = geometries.get(i);
            int stride = (attributeMask(mesh) & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING ? 104 : 72;
            vertexOffsets[i] = align(vertexSize, 16);
            indexOffsets[i] = align(indexSize, 4);
            vertexSize = Math.addExact(vertexOffsets[i], Math.multiplyExact(mesh.getPositionsCount() / 3, stride));
            indexSize = Math.addExact(indexOffsets[i], mesh.getIndices().size());
        }
        vertexSize = align(vertexSize, 16);
        int vertexStart = align(Math.addExact(HEADER_SIZE, Math.multiplyExact(count, RANGE_SIZE)), 16);
        int indexStart = Math.addExact(vertexStart, vertexSize);
        ByteBuffer out = ByteBuffer.allocate(Math.addExact(indexStart, indexSize)).order(ByteOrder.LITTLE_ENDIAN);
        out.put(new byte[]{'M', 'B', 'F', 'C'}).putInt(VERSION);
        out.putInt(0); // Archive compression is handled by the resource loader.
        out.putInt(Math.addExact(vertexSize, indexSize));
        out.putInt(count).putInt(vertexStart).putInt(vertexSize).putInt(indexStart).putInt(indexSize);
        for (int i = 0; i < count; ++i) {
            Mesh mesh = geometries.get(i);
            int mask = attributeMask(mesh);
            boolean skinned = (mask & ATTRIBUTE_MASK_SKINNING) == ATTRIBUTE_MASK_SKINNING;
            int vertexCount = mesh.getPositionsCount() / 3;
            int stride = skinned ? 104 : 72;
            int indexElementSize = mesh.getIndicesFormat().getNumber() == 0 ? 2 : 4;
            out.position(HEADER_SIZE + i * RANGE_SIZE);
            out.putInt(vertexOffsets[i]).putInt(vertexCount).putInt(stride);
            out.putInt(indexOffsets[i]).putInt(mesh.getIndices().size() / indexElementSize).putInt(indexElementSize).putInt(mask);
            out.position(vertexStart + vertexOffsets[i]);
            for (int v = 0; v < vertexCount; ++v) {
                putAttribute(out, mesh.getPositionsList(), v, COMPONENTS[0], 0);
                putAttribute(out, mesh.getNormalsList(), v, COMPONENTS[1], 0);
                putAttribute(out, mesh.getTangentsList(), v, COMPONENTS[2], 0);
                putAttribute(out, mesh.getColorsList(), v, COMPONENTS[3], 1);
                putAttribute(out, mesh.getTexcoord0List(), v, COMPONENTS[4], 0);
                putAttribute(out, mesh.getTexcoord1List(), v, COMPONENTS[5], 0);
                if (skinned) {
                    putAttribute(out, mesh.getWeightsList(), v, COMPONENTS[6], 0);
                    for (int c = 0; c < COMPONENTS[7]; ++c) out.putFloat(mesh.getBoneIndices(v * 4 + c));
                }
            }
            mesh.getIndices().copyTo(out.array(), indexStart + indexOffsets[i]);
        }
        return out.array();
    }

    /** Compiles model metadata with shared geometry ranges and material identities. */
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
                            .setVertexCount(mesh.getPositionsCount() / 3)
                            .clearPositions().clearNormals().clearTangents().clearColors()
                            .clearTexcoord0().clearTexcoord1().clearWeights().clearBoneIndices().clearIndices());
                }
            }
        }
        return result.build();
    }
}
