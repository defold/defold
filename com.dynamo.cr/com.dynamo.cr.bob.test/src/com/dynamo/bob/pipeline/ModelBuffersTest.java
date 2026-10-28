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

import static org.junit.Assert.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.dynamo.proto.DdfMath.Transform;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.rig.proto.Rig.Material;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Model;
import com.google.protobuf.ByteString;
import org.junit.Test;

public class ModelBuffersTest {
    private Mesh.Builder mesh() {
        return Mesh.newBuilder().setAabbMin(Vector3.getDefaultInstance()).setAabbMax(Vector3.getDefaultInstance())
                .addAllPositions(List.of(1.0f, 2.0f, 3.0f)).setIndices(ByteString.copyFrom(new byte[]{0, 0}));
    }

    private MeshSet meshSet(Mesh mesh) {
        return MeshSet.newBuilder().addModels(Model.newBuilder().setId(1).setLocal(Transform.getDefaultInstance()).addMeshes(mesh)).build();
    }

    // GPU bytes must match the engine layout, including absent-attribute defaults.
    @Test
    public void staticLayout() {
        ByteBuffer data = ByteBuffer.wrap(ModelBuffers.compile(List.of(mesh().build()))).order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(2, data.getInt(4));
        assertEquals(1, data.getInt(16));
        assertEquals(1, data.getInt(40));
        assertEquals(72, data.getInt(44));
        int vertex = data.getInt(20) + data.getInt(36);
        assertEquals(1.0f, data.getFloat(vertex), 0);
        assertEquals(2.0f, data.getFloat(vertex + 4), 0);
        assertEquals(3.0f, data.getFloat(vertex + 8), 0);
        assertEquals(0.0f, data.getFloat(vertex + 12), 0);
        assertEquals(1.0f, data.getFloat(vertex + 40), 0);
        assertEquals(1.0f, data.getFloat(vertex + 52), 0);
        assertEquals(0.0f, data.getFloat(vertex + 56), 0);
        assertEquals(0, data.getShort(data.getInt(28) + data.getInt(48)));
    }

    // Static and skinned ranges must coexist in the same aligned vertex/index payloads.
    @Test
    public void mixedLayouts() {
        Mesh skin = mesh().addAllWeights(List.of(0.75f, 0.25f, 0.0f, 0.0f))
                .addAllBoneIndices(List.of(2, 7, 0, 0)).build();
        ByteBuffer data = ByteBuffer.wrap(ModelBuffers.compile(List.of(mesh().build(), skin))).order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(2, data.getInt(16));
        int range = 36 + 28;
        assertEquals(80, data.getInt(range));
        assertEquals(104, data.getInt(range + 8));
        assertEquals(4, data.getInt(range + 12));
        int vertex = data.getInt(20) + data.getInt(range);
        assertEquals(0.75f, data.getFloat(vertex + 72), 0);
        assertEquals(0.25f, data.getFloat(vertex + 76), 0);
        assertEquals(2.0f, data.getFloat(vertex + 88), 0);
        assertEquals(7.0f, data.getFloat(vertex + 92), 0);
    }

    // Bindings and morph weights remain metadata and do not change shared geometry identity.
    @Test
    public void geometryIdentity() {
        Mesh first = mesh().build();
        Mesh second = first.toBuilder().setMaterialIndex(4).setMorphTargetTexture("/different.texturec")
                .setMorphTargetCount(1).addMorphBaseWeights(0.5f).build();
        var geometries = ModelBuffers.geometries(meshSet(first));
        assertArrayEquals(ModelBuffers.compile(geometries), ModelBuffers.compile(ModelBuffers.geometries(meshSet(second))));
        assertFalse(Arrays.equals(ModelBuffers.compile(geometries), ModelBuffers.compile(List.of(first.toBuilder().setPositions(0, 2.0f).build()))));
        Mesh metadata = ModelBuffers.metadata(meshSet(second), geometries).getModels(0).getMeshes(0);
        assertEquals(4, metadata.getMaterialIndex());
        assertEquals("/different.texturec", metadata.getMorphTargetTexture());
        assertEquals(List.of(0.5f), metadata.getMorphBaseWeightsList());
        assertEquals(0, metadata.getPositionsCount());
        assertEquals(0, metadata.getBufferIndex());
    }

    // Material identity ignores names and indices and distinguishes different PBR values.
    @Test
    public void materialIdentity() {
        Material first = Material.newBuilder().setName("First").setIndex(0).setAlphaCutoff(0.25f).build();
        Material renamed = first.toBuilder().setName("Second").setIndex(2).setIsSkinned(true).build();
        Material different = first.toBuilder().setAlphaCutoff(0.75f).build();
        MeshSet source = MeshSet.newBuilder().addMaterials(first).addMaterials(renamed).addMaterials(different).build();
        MeshSet metadata = ModelBuffers.metadata(source, List.of());
        assertNotEquals(0L, metadata.getMaterials(0).getMaterialHash());
        assertEquals(metadata.getMaterials(0).getMaterialHash(), metadata.getMaterials(1).getMaterialHash());
        assertNotEquals(metadata.getMaterials(0).getMaterialHash(), metadata.getMaterials(2).getMaterialHash());
        assertEquals(first, metadata.getMaterials(0).toBuilder().clearMaterialHash().build());
    }

    // Reordering and duplicating nodes must preserve geometry identity and shared range indices.
    @Test
    public void canonicalRanges() {
        Model first = meshSet(mesh().build()).getModels(0);
        Model second = first.toBuilder().setId(2).setMeshes(0, mesh().setPositions(0, 9).build()).build();
        MeshSet source = MeshSet.newBuilder().addModels(first).addModels(second).addModels(first.toBuilder().setId(3)).build();
        MeshSet reordered = source.toBuilder().clearModels().addModels(second).addModels(first).build();
        var geometries = ModelBuffers.geometries(source);
        assertEquals(2, geometries.size());
        assertArrayEquals(ModelBuffers.compile(geometries), ModelBuffers.compile(ModelBuffers.geometries(reordered)));
        MeshSet metadata = ModelBuffers.metadata(source, geometries);
        assertEquals(metadata.getModels(0).getMeshes(0).getBufferIndex(), metadata.getModels(2).getMeshes(0).getBufferIndex());
        assertNotEquals(metadata.getModels(0).getMeshes(0).getBufferIndex(), metadata.getModels(1).getMeshes(0).getBufferIndex());
    }

    // Explicit source-mesh selection strips unrelated geometry while whole scenes keep runtime alternatives.
    @Test
    public void meshSelection() {
        Model first = meshSet(mesh().build()).getModels(0).toBuilder().setMeshIndex(0).build();
        Model second = first.toBuilder().setId(2).setMeshIndex(1).setMeshes(0, mesh().setPositions(0, 9).build()).build();
        MeshSet source = MeshSet.newBuilder().addModels(first).addModels(second).addRawModels(first).addRawModels(second).build();
        MeshSet selected = ModelBuffers.selectMeshes(source, 1);
        assertEquals(1, selected.getModelsCount());
        assertEquals(1, selected.getRawModelsCount());
        assertEquals(1, ModelBuffers.geometries(selected).size());
        MeshSet whole = ModelBuffers.selectMeshes(source, -1);
        assertEquals(2, whole.getModelsCount());
        assertEquals(0, whole.getRawModelsCount());
        assertEquals(2, ModelBuffers.geometries(whole).size());
    }

    // Many primitives must remain draw ranges in one resource, not separate resource dependencies.
    @Test
    public void manyRanges() {
        List<Mesh> meshes = new ArrayList<>();
        for (int i = 0; i < 1100; ++i) meshes.add(mesh().setPositions(0, i).build());
        ByteBuffer data = ByteBuffer.wrap(ModelBuffers.compile(meshes)).order(ByteOrder.LITTLE_ENDIAN);
        assertEquals(1100, data.getInt(16));
        assertTrue(data.getInt(20) > 36 + 1100 * 28);
        assertEquals(data.capacity(), data.getInt(28) + data.getInt(32));
    }
}
