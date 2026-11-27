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

import com.dynamo.bob.util.MurmurHash;
import com.dynamo.proto.DdfMath;
import com.dynamo.graphics.proto.Graphics;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.ByteString;
import org.junit.Test;

import static org.junit.Assert.*;

public class MaterialRenderStateTest {
    private static ShaderDesc.ResourceType type(ShaderDesc.ShaderDataType type) {
        ShaderDesc.ResourceType.Builder builder = ShaderDesc.ResourceType.newBuilder();
        builder.setShaderType(type);
        return builder.build();
    }

    private static ShaderDesc.ShaderReflection reflection() {
        ShaderDesc.ResourceMember.Builder tint = ShaderDesc.ResourceMember.newBuilder();
        tint.setName("tint");
        tint.setNameHash(MurmurHash.hash64("tint"));
        tint.setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4));

        ShaderDesc.ResourceTypeInfo.Builder data = ShaderDesc.ResourceTypeInfo.newBuilder();
        data.setName("data");
        data.setNameHash(MurmurHash.hash64("data"));
        data.addMembers(tint);

        ShaderDesc.ResourceType.Builder bufferType = ShaderDesc.ResourceType.newBuilder();
        bufferType.setUseTypeIndex(true);
        bufferType.setTypeIndex(0);

        ShaderDesc.ResourceBinding.Builder uniformBuffer = ShaderDesc.ResourceBinding.newBuilder();
        uniformBuffer.setName("data");
        uniformBuffer.setNameHash(MurmurHash.hash64("data"));
        uniformBuffer.setType(bufferType);

        ShaderDesc.ResourceBinding.Builder instanceData = ShaderDesc.ResourceBinding.newBuilder();
        instanceData.setName("instance_data");
        instanceData.setNameHash(MurmurHash.hash64("instance_data"));
        instanceData.setStageFlags(1);
        instanceData.setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4));

        ShaderDesc.ResourceBinding.Builder texture = ShaderDesc.ResourceBinding.newBuilder();
        texture.setName("tex0");
        texture.setNameHash(MurmurHash.hash64("tex0"));
        texture.setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D));

        ShaderDesc.ShaderReflection.Builder builder = ShaderDesc.ShaderReflection.newBuilder();
        builder.addTypes(data);
        builder.addUniformBuffers(uniformBuffer);
        builder.addInputs(instanceData);
        builder.addTextures(texture);
        return builder.build();
    }

    private static MaterialDesc.Builder material() {
        MaterialDesc.PbrParameters.Builder pbrParameters = MaterialDesc.PbrParameters.newBuilder();
        pbrParameters.setHasParameters(true);
        pbrParameters.setHasIridescence(true);

        DdfMath.Vector4.Builder tintValue = DdfMath.Vector4.newBuilder();
        tintValue.setX(0.25f);
        tintValue.setY(0.5f);
        tintValue.setZ(1);
        tintValue.setW(-2);

        MaterialDesc.Constant.Builder tint = MaterialDesc.Constant.newBuilder();
        tint.setName("tint");
        tint.setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER_COLOR);
        tint.addValue(tintValue);

        MaterialDesc.Sampler.Builder sampler = MaterialDesc.Sampler.newBuilder();
        sampler.setName("tex0");
        sampler.setWrapU(MaterialDesc.WrapMode.WRAP_MODE_REPEAT);
        sampler.setWrapV(MaterialDesc.WrapMode.WRAP_MODE_MIRRORED_REPEAT);
        sampler.setWrapW(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
        sampler.setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR);
        sampler.setFilterMag(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST);
        sampler.setMaxAnisotropy(4);

        Graphics.VertexAttribute.Builder attribute = Graphics.VertexAttribute.newBuilder();
        attribute.setName("instance_data");
        attribute.setNameHash(MurmurHash.hash64("instance_data"));
        attribute.setSemanticType(Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_NONE);
        attribute.setDataType(Graphics.VertexAttribute.DataType.TYPE_UNSIGNED_BYTE);
        attribute.setVectorType(Graphics.VertexAttribute.VectorType.VECTOR_TYPE_VEC4);
        attribute.setStepFunction(Graphics.VertexStepFunction.VERTEX_STEP_FUNCTION_INSTANCE);
        attribute.setCoordinateSpace(Graphics.CoordinateSpace.COORDINATE_SPACE_LOCAL);
        attribute.setNormalize(true);

        MaterialDesc.Builder builder = MaterialDesc.newBuilder();
        builder.setName("a");
        builder.setVertexProgram("");
        builder.setFragmentProgram("");
        builder.setProgram("/a.spc");
        builder.setVertexSpace(MaterialDesc.VertexSpace.VERTEX_SPACE_LOCAL);
        builder.setPbrParameters(pbrParameters);
        builder.addFragmentConstants(tint);
        builder.addSamplers(sampler);
        builder.addAttributes(attribute);
        return builder;
    }

    // Matches the engine's fixed vector without requiring a graphics backend or a shader compiler executable.
    @Test
    public void canonicalEncodingMatchesEngine() {
        MaterialDesc compiled = MaterialBuilder.buildRenderState(material().build(), reflection());
        assertTrue(compiled.hasRenderState());
        assertTrue(compiled.hasRenderHash());
        assertEquals(0xdd87aa41f9d42533L, compiled.getRenderHash());
        assertEquals(1, compiled.getRenderState().getConstantsCount());
        assertEquals(1, compiled.getRenderState().getValuesCount());
        assertEquals(1, compiled.getRenderState().getSamplersCount());
        assertEquals(1, compiled.getRenderState().getAttributesCount());
        assertEquals(compiled.getRenderHash(), MaterialBuilder.buildRenderState(compiled, reflection()).getRenderHash());
    }

    // The shared render library emits the existing graphics layout type without retaining draw-local defaults.
    @Test
    public void attributeStateUsesGraphicsLayout() {
        MaterialDesc.Builder input = material();
        Graphics.VertexAttribute.Builder attribute = input.getAttributesBuilder(0);
        attribute.setBinaryValues(ByteString.copyFrom(new byte[]{1, 2, 3, 4}));
        MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection());
        Graphics.VertexAttribute layout = compiled.getRenderState().getAttributes(0);
        assertEquals("", layout.getName());
        assertEquals(input.getAttributes(0).getNameHash(), layout.getNameHash());
        assertEquals(input.getAttributes(0).getDataType(), layout.getDataType());
        assertEquals(input.getAttributes(0).getVectorType(), layout.getVectorType());
        assertEquals(input.getAttributes(0).getStepFunction(), layout.getStepFunction());
        assertEquals(4, layout.getElementCount());
        assertFalse(layout.hasBinaryValues());
        assertFalse(layout.hasDoubleValues());
        assertFalse(layout.hasLongValues());
    }

    // Names, tags, paths, textures and instance defaults are not persistent shared draw settings.
    @Test
    public void metadataAndInstanceValuesAreExcluded() {
        MaterialDesc original = MaterialBuilder.buildRenderState(material().build(), reflection());
        MaterialDesc.Builder changed = material();
        changed.setName("b");
        changed.addTags("shadow");
        changed.setProgram("/b.spc");
        MaterialDesc.Sampler.Builder sampler = changed.getSamplersBuilder(0);
        sampler.setTexture("/another.texturec");
        Graphics.VertexAttribute.Builder attribute = changed.getAttributesBuilder(0);
        attribute.setBinaryValues(ByteString.copyFrom(new byte[]{1, 2, 3, 4}));
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(changed.build(), reflection()).getRenderHash());
        MaterialDesc.Constant.Builder constant = changed.getFragmentConstantsBuilder(0);
        DdfMath.Vector4.Builder value = constant.getValueBuilder(0);
        value.setX(0.75f);
        assertNotEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(changed.build(), reflection()).getRenderHash());
    }

    // Optimized-out sampler declarations and their aliases must not change effective material compatibility.
    @Test
    public void unusedSamplersDoNotAffectRenderHash() {
        MaterialDesc original = MaterialBuilder.buildRenderState(material().build(), reflection());
        MaterialDesc.Builder input = material();
        MaterialDesc.Sampler.Builder unusedSampler = input.getSamplers(0).toBuilder();
        unusedSampler.setName("unused");
        unusedSampler.addNameIndirections(MurmurHash.hash64("unused_0"));
        input.addSamplers(unusedSampler);
        MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection());
        assertEquals(1, compiled.getRenderState().getSamplersCount());
        assertEquals(original.getRenderHash(), compiled.getRenderHash());
        unusedSampler = input.getSamplersBuilder(1);
        unusedSampler.setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
        unusedSampler.setMaxAnisotropy(16);
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
        MaterialDesc.Sampler.Builder sampler = input.getSamplersBuilder(0);
        sampler.setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
        assertNotEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());

        ShaderDesc.ShaderReflection.Builder reflection = reflection().toBuilder();
        reflection.clearTextures();
        ShaderDesc.ShaderReflection noTextures = reflection.build();
        compiled = MaterialBuilder.buildRenderState(input.build(), noTextures);
        assertEquals(0, compiled.getRenderState().getSamplersCount());
        input.clearSamplers();
        assertEquals(compiled.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), noTextures).getRenderHash());
    }

    // Engine-owned skinning and morph samplers use fixed draw settings, not authored material sampler state.
    @Test
    public void internalSamplersDoNotAffectRenderHash() {
        ShaderDesc.ShaderReflection.Builder reflection = reflection().toBuilder();
        MaterialDesc.Builder input = material();
        String[] internalSamplers = {"pose_matrix_cache", "morph_targets"};
        for (String name : internalSamplers) {
            ShaderDesc.ResourceBinding.Builder texture = ShaderDesc.ResourceBinding.newBuilder();
            texture.setName(name);
            texture.setNameHash(MurmurHash.hash64(name));
            texture.setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D));
            reflection.addTextures(texture);

            MaterialDesc.Sampler.Builder sampler = input.getSamplers(0).toBuilder();
            sampler.setName(name);
            sampler.addNameIndirections(MurmurHash.hash64(name + "_0"));
            input.addSamplers(sampler);
        }
        MaterialDesc original = MaterialBuilder.buildRenderState(material().build(), reflection.build());
        MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection.build());
        assertEquals(1, compiled.getRenderState().getSamplersCount());
        assertEquals(original.getRenderHash(), compiled.getRenderHash());
        for (int i = 1; i < input.getSamplersCount(); ++i) {
            MaterialDesc.Sampler.Builder sampler = input.getSamplersBuilder(i);
            sampler.setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
            sampler.setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST);
            sampler.setMaxAnisotropy(16);
            assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection.build()).getRenderHash());
        }
        MaterialDesc.Sampler.Builder sampler = input.getSamplersBuilder(0);
        sampler.setMaxAnisotropy(16);
        assertNotEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection.build()).getRenderHash());
    }

    // Native and fallback variants, including surplus editor aliases, describe the same logical sampler state.
    @Test
    public void samplerAliasesDoNotAffectRenderHash() {
        ShaderDesc.ShaderDataType[] samplerTypes = {
            ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D,
            ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY,
            ShaderDesc.ShaderDataType.SHADER_TYPE_TEXTURE2D_ARRAY
        };
        for (ShaderDesc.ShaderDataType samplerType : samplerTypes) {
            ShaderDesc.ShaderReflection.Builder reflection = reflection().toBuilder();
            ShaderDesc.ResourceBinding.Builder texture = reflection.getTexturesBuilder(0);
            texture.setType(type(samplerType));
            MaterialDesc.Builder input = material();
            MaterialDesc original = MaterialBuilder.buildRenderState(input.build(), reflection.build());
            MaterialDesc.Sampler.Builder sampler = input.getSamplersBuilder(0);
            sampler.addNameIndirections(MurmurHash.hash64("tex0_0"));
            sampler.addNameIndirections(MurmurHash.hash64("tex0_1"));
            MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection.build());
            assertEquals(1, compiled.getRenderState().getSamplersCount());
            assertEquals(MurmurHash.hash64("tex0"), compiled.getRenderState().getSamplers(0).getNameHash());
            assertEquals(original.getRenderState(), compiled.getRenderState());
            assertEquals(original.getRenderHash(), compiled.getRenderHash());
            // The descriptor still retains the aliases needed to bind fallback shaders.
            assertEquals(input.getSamplers(0).getNameIndirectionsList(), compiled.getSamplers(0).getNameIndirectionsList());
            sampler.setMaxAnisotropy(8);
            assertNotEquals(compiled.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection.build()).getRenderHash());
        }
    }

    // Bob and the editor share alias generation: only active arrays with a fallback variant get aliases.
    @Test
    public void fallbackAliasesFollowShaderVariantsAndReflection() {
        MaterialDesc.Builder input = material();
        input.setMaxPageCount(2);
        MaterialDesc.Sampler.Builder arraySampler = input.getSamplers(0).toBuilder();
        arraySampler.setName("array");
        input.addSamplers(arraySampler);
        MaterialDesc.Sampler.Builder unusedSampler = input.getSamplers(0).toBuilder();
        unusedSampler.setName("unused");
        input.addSamplers(unusedSampler);
        ShaderDesc.ShaderReflection.Builder reflection = reflection().toBuilder();
        ShaderDesc.ResourceBinding.Builder texture = ShaderDesc.ResourceBinding.newBuilder();
        texture.setName("array");
        texture.setNameHash(MurmurHash.hash64("array"));
        texture.setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY));
        reflection.addTextures(texture);

        ShaderDesc.Shader.Builder fragmentShader = ShaderDesc.Shader.newBuilder();
        fragmentShader.setSource(ByteString.EMPTY);
        fragmentShader.setShaderType(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT);
        fragmentShader.setLanguage(ShaderDesc.Language.LANGUAGE_GLES_SM100);
        fragmentShader.setVariantTextureArray(true);

        ShaderDesc.Builder shader = ShaderDesc.newBuilder();
        shader.setReflection(reflection);
        shader.addShaders(fragmentShader);

        for (MaterialDesc.Sampler.Builder sampler : input.getSamplersBuilderList()) {
            sampler.addNameIndirections(MurmurHash.hash64("stale"));
        }
        MaterialBuilder.applyVariantTextureArray(input, shader.build());
        assertEquals(0, input.getSamplers(0).getNameIndirectionsCount());
        assertEquals(2, input.getSamplers(1).getNameIndirectionsCount());
        assertEquals(MurmurHash.hash64("array_0"), input.getSamplers(1).getNameIndirections(0));
        assertEquals(MurmurHash.hash64("array_1"), input.getSamplers(1).getNameIndirections(1));
        assertEquals(0, input.getSamplers(2).getNameIndirectionsCount());
        MaterialDesc fallback = MaterialBuilder.buildRenderState(input.build(), reflection.build());

        MaterialBuilder.applyVariantTextureArray(input, shader.build());
        assertEquals(2, input.getSamplers(1).getNameIndirectionsCount());
        fragmentShader = shader.getShadersBuilder(0);
        fragmentShader.setVariantTextureArray(false);
        MaterialBuilder.applyVariantTextureArray(input, shader.build());
        assertEquals(0, input.getSamplers(1).getNameIndirectionsCount());
        MaterialDesc nativeArray = MaterialBuilder.buildRenderState(input.build(), reflection.build());
        assertEquals(fallback.getRenderState(), nativeArray.getRenderState());
        assertEquals(fallback.getRenderHash(), nativeArray.getRenderHash());
    }

    // Omitted constants and explicitly authored zero defaults normalize to the same reflected value array.
    @Test
    public void defaultsAndStageMerging() {
        MaterialDesc.Builder input = material();
        input.clearFragmentConstants();
        MaterialDesc original = MaterialBuilder.buildRenderState(input.build(), reflection());
        MaterialDesc.Constant.Builder fragmentConstant = MaterialDesc.Constant.newBuilder();
        fragmentConstant.setName("tint");
        fragmentConstant.setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER);
        input.addFragmentConstants(fragmentConstant);
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
        DdfMath.Vector4.Builder value = DdfMath.Vector4.newBuilder();
        value.setX(1);
        fragmentConstant = input.getFragmentConstantsBuilder(0);
        fragmentConstant.addValue(value);
        MaterialDesc.Constant.Builder vertexConstant = MaterialDesc.Constant.newBuilder();
        vertexConstant.setName("tint");
        vertexConstant.setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER);
        vertexConstant.addValue(DdfMath.Vector4.getDefaultInstance());
        input.addVertexConstants(vertexConstant);
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
    }
}
