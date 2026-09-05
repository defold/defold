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

package com.dynamo.bob.fs;

import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

import com.dynamo.bob.pipeline.Modelimporter;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.TextFormat;

/**
 * A virtual Defold material generated from one glTF material. The descriptor
 * supplies the asset-pbr shader interface and source sampler state. PBR factors
 * and extension values remain in the imported mesh-set material data.
 */
public class GltfMaterialResource extends GltfResource {

    private static final String VERTEX_PROGRAM = "/defold-pbr/shaders/pbr.vp";
    private static final String FRAGMENT_PROGRAM = "/defold-pbr/shaders/pbr.fp";

    private final MaterialDesc materialDesc;
    private final Modelimporter.Material sourceMaterial;
    private final Map<String, GltfContainer.SamplerBinding> samplerBindings;

    GltfMaterialResource(IFileSystem fileSystem, IResource sourceResource, String path,
                         Modelimporter.Material sourceMaterial) {
        this(fileSystem, sourceResource, path, sourceMaterial, createMaterialDesc(sourceMaterial),
                Collections.<String, GltfContainer.SamplerBinding>emptyMap());
    }

    GltfMaterialResource(IFileSystem fileSystem, IResource sourceResource, String path,
                         Modelimporter.Material sourceMaterial, MaterialDesc materialDesc) {
        this(fileSystem, sourceResource, path, sourceMaterial, materialDesc,
                Collections.<String, GltfContainer.SamplerBinding>emptyMap());
    }

    GltfMaterialResource(IFileSystem fileSystem, IResource sourceResource, String path,
                         Modelimporter.Material sourceMaterial, MaterialDesc materialDesc,
                         Map<String, GltfContainer.SamplerBinding> samplerBindings) {
        super(fileSystem, sourceResource, path, Kind.MATERIAL, sourceMaterial.index, sourceMaterial.name,
                toTextBytes(materialDesc));
        this.materialDesc = materialDesc;
        this.sourceMaterial = sourceMaterial;
        this.samplerBindings = GltfContainer.immutableSamplerBindings(samplerBindings);
    }

    public MaterialDesc getMaterialDesc() {
        return materialDesc;
    }

    /** Complete native modelimporter material graph retained as authoritative inspection metadata. */
    public Modelimporter.Material getSourceMaterial() {
        return sourceMaterial;
    }

    /**
     * Sampler names mapped to successfully extracted virtual image assets.
     * Every binding path is relative to the source glTF resource.
     */
    public Map<String, GltfContainer.SamplerBinding> getSamplerBindings() {
        return samplerBindings;
    }

    private static byte[] toTextBytes(MaterialDesc materialDesc) {
        return TextFormat.printToString(materialDesc).getBytes(StandardCharsets.UTF_8);
    }

    static MaterialDesc createMaterialDesc(Modelimporter.Material material) {
        String name = material.name == null || material.name.isEmpty()
                ? "gltf_material_" + material.index
                : material.name;
        MaterialDesc.Builder builder = MaterialDesc.newBuilder()
                .setName(name)
                .addTags("model")
                .setVertexProgram(VERTEX_PROGRAM)
                .setFragmentProgram(FRAGMENT_PROGRAM)
                .setVertexSpace(MaterialDesc.VertexSpace.VERTEX_SPACE_LOCAL);

        builder.addVertexConstants(createConstant("mtx_worldview", MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEW));
        builder.addVertexConstants(createConstant("mtx_view", MaterialDesc.ConstantType.CONSTANT_TYPE_VIEW));
        builder.addVertexConstants(createConstant("mtx_proj", MaterialDesc.ConstantType.CONSTANT_TYPE_PROJECTION));
        builder.addVertexConstants(createConstant("mtx_normal", MaterialDesc.ConstantType.CONSTANT_TYPE_NORMAL));

        for (Map.Entry<String, Modelimporter.TextureView> entry : samplerTextureViews(material).entrySet()) {
            builder.addSamplers(createSampler(entry.getKey(), entry.getValue()));
        }
        return builder.build();
    }

    static Map<String, Modelimporter.TextureView> samplerTextureViews(Modelimporter.Material material) {
        Map<String, Modelimporter.TextureView> result =
                new LinkedHashMap<String, Modelimporter.TextureView>();
        Modelimporter.PbrMetallicRoughness metallicRoughness = material.pbrMetallicRoughness;
        putTextureView(result, "PbrMetallicRoughness_baseColorTexture",
                metallicRoughness == null ? null : metallicRoughness.baseColorTexture);
        putTextureView(result, "PbrMetallicRoughness_metallicRoughnessTexture",
                metallicRoughness == null ? null : metallicRoughness.metallicRoughnessTexture);
        putTextureView(result, "PbrMaterial_normalTexture", material.normalTexture);
        putTextureView(result, "PbrMaterial_occlusionTexture", material.occlusionTexture);
        putTextureView(result, "PbrMaterial_emissiveTexture", material.emissiveTexture);
        return result;
    }

    private static void putTextureView(Map<String, Modelimporter.TextureView> result, String name,
                                       Modelimporter.TextureView textureView) {
        if (textureView != null && textureView.texture != null) {
            result.put(name, textureView);
        }
    }

    private static MaterialDesc.Constant createConstant(String name, MaterialDesc.ConstantType type) {
        return MaterialDesc.Constant.newBuilder()
                .setName(name)
                .setType(type)
                .build();
    }

    private static MaterialDesc.Sampler createSampler(String name, Modelimporter.TextureView textureView) {
        Modelimporter.Sampler sampler = textureView == null || textureView.texture == null
                ? null
                : textureView.texture.sampler;
        int wrapS = sampler == null ? 10497 : sampler.wrapS;
        int wrapT = sampler == null ? 10497 : sampler.wrapT;
        int minFilter = sampler == null ? 9729 : sampler.minFilter;
        int magFilter = sampler == null ? 9729 : sampler.magFilter;
        return MaterialDesc.Sampler.newBuilder()
                .setName(name)
                .setWrapU(toWrapMode(wrapS))
                .setWrapV(toWrapMode(wrapT))
                .setFilterMin(toMinFilter(minFilter))
                .setFilterMag(toMagFilter(magFilter))
                .build();
    }

    private static MaterialDesc.WrapMode toWrapMode(int value) {
        switch (value) {
            case 33071:
                return MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE;
            case 33648:
                return MaterialDesc.WrapMode.WRAP_MODE_MIRRORED_REPEAT;
            case 10497:
            default:
                return MaterialDesc.WrapMode.WRAP_MODE_REPEAT;
        }
    }

    private static MaterialDesc.FilterModeMin toMinFilter(int value) {
        switch (value) {
            case 9728:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST;
            case 9984:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST_MIPMAP_NEAREST;
            case 9985:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_NEAREST;
            case 9986:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST_MIPMAP_LINEAR;
            case 9987:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR;
            case 9729:
            default:
                return MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR;
        }
    }

    private static MaterialDesc.FilterModeMag toMagFilter(int value) {
        switch (value) {
            case 9728:
                return MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST;
            case 9729:
            default:
                return MaterialDesc.FilterModeMag.FILTER_MODE_MAG_LINEAR;
        }
    }
}
