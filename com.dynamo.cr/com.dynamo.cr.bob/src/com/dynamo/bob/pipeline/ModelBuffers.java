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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.rig.proto.Rig.Material;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Model;

/**
 * Shared Bob/editor adapter for the native model-buffer compiler.
 *
 * Native import packs geometry and assigns Mesh.bufferIndex before returning to
 * Java. This class selects runtime-reachable ranges and retains node identity,
 * transforms, bindings and morph metadata in the compiled model. The actual file
 * layout is defined in gamesys/modelbuffers.h and written by modelimporter_buffers.cpp.
 */
public final class ModelBuffers {
    public record Compiled(MeshSet metadata, byte[] data) {}

    private ModelBuffers() {}

    /** Packs Java meshes after large-mesh splitting, assigning their native range indices. */
    static byte[] compileScene(Modelimporter.Scene scene) {
        List<Modelimporter.Mesh> meshes = new ArrayList<>();
        for (Modelimporter.Model model : scene.models) {
            meshes.addAll(List.of(model.meshes));
        }
        return ModelImporterJni.CompileModelBuffers(meshes.toArray(Modelimporter.Mesh[]::new));
    }

    /** Keeps all runtime-selectable scene meshes, or one explicit source-mesh selection. */
    static MeshSet selectMeshes(MeshSet source, int meshIndex) {
        MeshSet.Builder result = source.toBuilder().clearModels().clearRawModels();
        for (Model model : source.getModelsList()) {
            if (meshIndex < 0 || model.getMeshIndex() == meshIndex) {
                result.addModels(model);
            }
        }
        if (meshIndex >= 0) {
            for (Model model : source.getRawModelsList()) {
                if (model.getMeshIndex() == meshIndex) {
                    result.addRawModels(model);
                }
            }
        }
        return result.build();
    }

    /**
     * Selects native-packed ranges and remaps their indices into one geometry resource.
     * Input meshes must retain the bufferIndex assigned during native compilation.
     * Node order, placements and bindings do not affect the resulting DMMB bytes.
     */
    public static Compiled compile(MeshSet source, byte[] buffers) {
        TreeSet<Integer> selected = new TreeSet<>();
        for (List<Model> models : List.of(source.getModelsList(), source.getRawModelsList())) {
            for (Model model : models) {
                for (Mesh mesh : model.getMeshesList()) {
                    selected.add(mesh.getBufferIndex());
                }
            }
        }
        int[] selection = selected.stream().mapToInt(Integer::intValue).toArray();
        Map<Integer, Integer> indices = new HashMap<>();
        for (int i = 0; i < selection.length; ++i) {
            indices.put(selection[i], i);
        }
        MeshSet.Builder metadata = source.toBuilder();
        for (List<Model.Builder> models : List.of(metadata.getModelsBuilderList(), metadata.getRawModelsBuilderList())) {
            for (Model.Builder model : models) {
                for (int i = 0; i < model.getMeshesCount(); ++i) {
                    Mesh mesh = model.getMeshes(i);
                    model.setMeshes(i, metadata(mesh).setBufferIndex(indices.get(mesh.getBufferIndex())));
                }
            }
        }
        setMaterialHashes(metadata);
        return new Compiled(metadata.build(), ModelImporterJni.SelectModelBuffers(buffers, selection));
    }

    /**
     * Applies native range indices to the raw mesh-set producer's metadata. Morph
     * texture paths are resolved by that producer; its vertex/index arrays are discarded.
     * Source mesh index and primitive order identify the same meshes in both imports.
     */
    static MeshSet metadata(MeshSet source, MeshSet compiled) {
        Map<Integer, List<Mesh>> ranges = new HashMap<>();
        for (List<Model> models : List.of(compiled.getModelsList(), compiled.getRawModelsList())) {
            for (Model model : models) {
                if (model.getMeshesCount() > 0) {
                    ranges.put(model.getMeshIndex(), model.getMeshesList());
                }
            }
        }
        MeshSet.Builder result = source.toBuilder();
        for (List<Model.Builder> models : List.of(result.getModelsBuilderList(), result.getRawModelsBuilderList())) {
            for (Model.Builder model : models) {
                for (int i = 0; i < model.getMeshesCount(); ++i) {
                    Mesh range = ranges.get(model.getMeshIndex()).get(i);
                    model.setMeshes(i, metadata(model.getMeshes(i))
                            .setBufferIndex(range.getBufferIndex()).setVertexCount(range.getVertexCount()));
                }
            }
        }
        setMaterialHashes(result);
        return result.build();
    }

    private static Mesh.Builder metadata(Mesh mesh) {
        return mesh.toBuilder().clearPositions().clearNormals().clearTangents().clearColors()
                .clearTexcoord0().clearTexcoord1().clearWeights().clearBoneIndices().clearIndices();
    }

    private static void setMaterialHashes(MeshSet.Builder meshSet) {
        for (Material.Builder material : meshSet.getMaterialsBuilderList()) {
            byte[] values = material.clone().clearName().clearIndex().clearIsSkinned().clearMaterialHash().build().toByteArray();
            material.setMaterialHash(MurmurHash.hash64(values, values.length));
        }
    }
}
