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

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import static org.junit.Assert.*;

public class MaterialRenderStateTest {
    private static ShaderDesc.ResourceType type(ShaderDesc.ShaderDataType type) {
        return ShaderDesc.ResourceType.newBuilder().setShaderType(type).build();
    }

    private static ShaderDesc.ShaderReflection reflection() {
        return ShaderDesc.ShaderReflection.newBuilder()
            .addTypes(ShaderDesc.ResourceTypeInfo.newBuilder().setName("data").setNameHash(MurmurHash.hash64("data"))
                .addMembers(ShaderDesc.ResourceMember.newBuilder().setName("tint").setNameHash(MurmurHash.hash64("tint"))
                    .setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4))))
            .addUniformBuffers(ShaderDesc.ResourceBinding.newBuilder().setName("data").setNameHash(MurmurHash.hash64("data"))
                .setType(ShaderDesc.ResourceType.newBuilder().setUseTypeIndex(true).setTypeIndex(0)))
            .addInputs(ShaderDesc.ResourceBinding.newBuilder().setName("instance_data").setNameHash(MurmurHash.hash64("instance_data"))
                .setStageFlags(1).setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_VEC4)))
            .addTextures(ShaderDesc.ResourceBinding.newBuilder().setName("tex0").setNameHash(MurmurHash.hash64("tex0"))
                .setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D)))
            .build();
    }

    private static MaterialDesc.Builder material() {
        return MaterialDesc.newBuilder().setName("a").setVertexProgram("").setFragmentProgram("").setProgram("/a.spc")
            .setVertexSpace(MaterialDesc.VertexSpace.VERTEX_SPACE_LOCAL)
            .setPbrParameters(MaterialDesc.PbrParameters.newBuilder().setHasParameters(true).setHasIridescence(true))
            .addFragmentConstants(MaterialDesc.Constant.newBuilder().setName("tint")
                .setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER_COLOR)
                .addValue(DdfMath.Vector4.newBuilder().setX(0.25f).setY(0.5f).setZ(1).setW(-2)))
            .addSamplers(MaterialDesc.Sampler.newBuilder().setName("tex0")
                .setWrapU(MaterialDesc.WrapMode.WRAP_MODE_REPEAT)
                .setWrapV(MaterialDesc.WrapMode.WRAP_MODE_MIRRORED_REPEAT)
                .setWrapW(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE)
                .setFilterMin(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR)
                .setFilterMag(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST).setMaxAnisotropy(4))
            .addAttributes(Graphics.VertexAttribute.newBuilder().setName("instance_data").setNameHash(MurmurHash.hash64("instance_data"))
                .setSemanticType(Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_NONE)
                .setDataType(Graphics.VertexAttribute.DataType.TYPE_UNSIGNED_BYTE)
                .setVectorType(Graphics.VertexAttribute.VectorType.VECTOR_TYPE_VEC4)
                .setStepFunction(Graphics.VertexStepFunction.VERTEX_STEP_FUNCTION_INSTANCE)
                .setCoordinateSpace(Graphics.CoordinateSpace.COORDINATE_SPACE_LOCAL).setNormalize(true));
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
        input.getAttributesBuilder(0).setBinaryValues(ByteString.copyFrom(new byte[]{1, 2, 3, 4}));
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
        MaterialDesc.Builder changed = material().setName("b").addTags("shadow").setProgram("/b.spc");
        changed.getSamplersBuilder(0).setTexture("/another.texturec");
        changed.getAttributesBuilder(0).setBinaryValues(ByteString.copyFrom(new byte[]{1, 2, 3, 4}));
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(changed.build(), reflection()).getRenderHash());
        changed.getFragmentConstantsBuilder(0).getValueBuilder(0).setX(0.75f);
        assertNotEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(changed.build(), reflection()).getRenderHash());
    }

    // Optimized-out sampler declarations and their aliases must not change effective material compatibility.
    @Test
    public void unusedSamplersDoNotAffectRenderHash() {
        MaterialDesc original = MaterialBuilder.buildRenderState(material().build(), reflection());
        MaterialDesc.Builder input = material();
        input.addSamplers(input.getSamplers(0).toBuilder().setName("unused")
            .addNameIndirections(MurmurHash.hash64("unused_0")));
        MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection());
        assertEquals(1, compiled.getRenderState().getSamplersCount());
        assertEquals(original.getRenderHash(), compiled.getRenderHash());
        input.getSamplersBuilder(1).setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE).setMaxAnisotropy(16);
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
        input.getSamplersBuilder(0).setWrapU(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE);
        assertNotEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());

        ShaderDesc.ShaderReflection noTextures = reflection().toBuilder().clearTextures().build();
        compiled = MaterialBuilder.buildRenderState(input.build(), noTextures);
        assertEquals(0, compiled.getRenderState().getSamplersCount());
        assertEquals(compiled.getRenderHash(), MaterialBuilder.buildRenderState(input.clearSamplers().build(), noTextures).getRenderHash());
    }

    // Array fallback aliases are generated from a reflected base sampler, not separate reflection entries.
    @Test
    public void reflectedArraySamplerKeepsFallbackAliases() {
        ShaderDesc.ShaderReflection.Builder reflection = reflection().toBuilder();
        reflection.getTexturesBuilder(0).setType(type(ShaderDesc.ShaderDataType.SHADER_TYPE_SAMPLER2D_ARRAY));
        long slice0 = MurmurHash.hash64("tex0_0");
        long slice1 = MurmurHash.hash64("tex0_1");
        MaterialDesc.Builder input = material();
        input.getSamplersBuilder(0).addNameIndirections(slice0).addNameIndirections(slice1);
        MaterialDesc compiled = MaterialBuilder.buildRenderState(input.build(), reflection.build());
        assertEquals(3, compiled.getRenderState().getSamplersCount());
        Set<Long> names = new HashSet<>();
        compiled.getRenderState().getSamplersList().forEach(sampler -> names.add(sampler.getNameHash()));
        assertEquals(new HashSet<>(Arrays.asList(MurmurHash.hash64("tex0"), slice0, slice1)), names);
        input.getSamplersBuilder(0).setMaxAnisotropy(8);
        assertNotEquals(compiled.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection.build()).getRenderHash());
    }

    // Omitted constants and explicitly authored zero defaults normalize to the same reflected value array.
    @Test
    public void defaultsAndStageMerging() {
        MaterialDesc.Builder input = material().clearFragmentConstants();
        MaterialDesc original = MaterialBuilder.buildRenderState(input.build(), reflection());
        input.addFragmentConstants(MaterialDesc.Constant.newBuilder().setName("tint")
            .setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER));
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
        input.getFragmentConstantsBuilder(0).addValue(DdfMath.Vector4.newBuilder().setX(1));
        input.addVertexConstants(MaterialDesc.Constant.newBuilder().setName("tint")
            .setType(MaterialDesc.ConstantType.CONSTANT_TYPE_USER).addValue(DdfMath.Vector4.getDefaultInstance()));
        assertEquals(original.getRenderHash(), MaterialBuilder.buildRenderState(input.build(), reflection()).getRenderHash());
    }
}
