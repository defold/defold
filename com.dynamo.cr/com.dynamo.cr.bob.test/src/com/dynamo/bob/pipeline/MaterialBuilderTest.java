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

import java.util.List;

import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.graphics.proto.Graphics;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Material.MaterialDesc;

import static org.junit.Assert.*;
import static org.junit.Assert.assertEquals;

public class MaterialBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private void addAttribute(StringBuilder src, String name, int elementCount, Graphics.VertexAttribute.SemanticType semanticType) {
        String nameEscaped = "\"" + name + "\"";
        src.append("attributes {");
        src.append("    name: ").append(nameEscaped);
        src.append("    element_count: ").append(elementCount);
        if (semanticType != Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_NONE) {
            src.append("    semantic_type: ").append(semanticType);
        }
        src.append("    double_values {");
        src.append("        v: 1.0".repeat(Math.max(0, elementCount)));
        src.append("    }");
        src.append("}");
    }

    private void addAttribute(StringBuilder src, int elementCount) {
        addAttribute(src, "ele_count_" + elementCount, elementCount, Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_NONE);
    }

    private void assertAttribute(Graphics.VertexAttribute attribute, String name, Graphics.VertexAttribute.VectorType vectorType) {
        assertEquals(name, attribute.getName());
        assertEquals(vectorType, attribute.getVectorType());
    }

    @Test
    public void testCombinedShader() throws Exception {
        String srcShaderStr = "void main() {}\n";
        Graphics.ShaderDesc shaderDesc = addAndBuildShaderDescs(
                new String[]{"/test_combined.vp", "/test_combined.fp"},
                new String[]{srcShaderStr, srcShaderStr},
                "/test_combined.shbundle");
        assertEquals(2, shaderDesc.getShadersCount());

        String src = """
                name: "test_combined"
                vertex_program: "/test_combined.vp"
                fragment_program: "/test_combined.fp"
                """;

        MaterialDesc material = getMessage(build("/test_migrate_vx_attributes.material", src), MaterialDesc.class);
        // We don't use them in runtime, so they should be empty
        assertEquals("", material.getVertexProgram());
        assertEquals("", material.getFragmentProgram());

        assertNotNull(material);
        assertTrue(material.hasProgram());

        String program = material.getProgram();
        String expectedProgram = MaterialBuilder.getShaderName("/test_combined.vp", "/test_combined.fp", 0, ".spc");
        assertEquals(ResourceUtil.minifyPath(expectedProgram), program);
    }

    @Test
    public void testMigrateVertexAttributes() throws Exception {
        addFile("/test_migrate_vx_attributes.material", "");

        String srcShaderStr = "void main() {}\n";
        Graphics.ShaderDesc shaderDesc = addAndBuildShaderDescs(new String[]{"/test_migrate_vx_attributes.vp", "/test_migrate_vx_attributes.fp"}, new String[]{srcShaderStr,srcShaderStr}, "/test_migrate_vx_attributes.shbundle");
        assertEquals(2, shaderDesc.getShadersCount());

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("vertex_program: \"/test_migrate_vx_attributes.vp\"\n");
        src.append("fragment_program: \"/test_migrate_vx_attributes.fp\"\n");

        addAttribute(src, 1);
        addAttribute(src, 2);
        addAttribute(src, 3);
        addAttribute(src, 4);
        addAttribute(src, 9);
        addAttribute(src, 16);

        addAttribute(src, "ele_count_4_normal", 4, Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_NORMAL_MATRIX);
        addAttribute(src, "ele_count_4_world", 4, Graphics.VertexAttribute.SemanticType.SEMANTIC_TYPE_WORLD_MATRIX);

        MaterialDesc material = getMessage(build("/test_migrate_vx_attributes.material", src.toString()), MaterialDesc.class);
        assertEquals(8, material.getAttributesCount());

        assertAttribute(material.getAttributes(0), "ele_count_1", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_SCALAR);
        assertAttribute(material.getAttributes(1), "ele_count_2", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_VEC2);
        assertAttribute(material.getAttributes(2), "ele_count_3", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_VEC3);
        assertAttribute(material.getAttributes(3), "ele_count_4", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_VEC4);
        assertAttribute(material.getAttributes(4), "ele_count_9", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_MAT3);
        assertAttribute(material.getAttributes(5), "ele_count_16", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_MAT4);

        assertAttribute(material.getAttributes(6), "ele_count_4_normal", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_MAT2);
        assertAttribute(material.getAttributes(7), "ele_count_4_world", Graphics.VertexAttribute.VectorType.VECTOR_TYPE_MAT2);
    }

    @Test
    public void testVariantArrayTextures() throws Exception {
        String vsShaderLegacy = "void main() {}\n";
        String fsShaderLegacy =
                """
                varying mediump vec2 var_texcoord0;
                varying lowp vec4 var_color;
                varying lowp float var_page_index;
                uniform lowp sampler2DArray texture_sampler;
                void main() {
                    lowp vec4 tex = texture2DArray(texture_sampler, vec3(var_texcoord0.xy, var_page_index));
                    gl_FragColor = tex * var_color;
                }
                """;

        addFile("/test_vp.vp", vsShaderLegacy);
        addFile("/test_fp.fp", fsShaderLegacy);

        String materialSrc =
                """
                name: "test_material"
                vertex_program: "/test_vp.vp"
                fragment_program: "/test_fp.fp"
                max_page_count: 4
                samplers {
                  name: "texture_sampler"
                  wrap_u: WRAP_MODE_CLAMP_TO_EDGE
                  wrap_v: WRAP_MODE_CLAMP_TO_EDGE
                  filter_min: FILTER_MODE_MIN_DEFAULT
                  filter_mag: FILTER_MODE_MAG_DEFAULT
                }
                """;

        getProject().getProjectProperties().putBooleanValue("shader", "output_glsl120", true);

        addFile("/test.material", "");
        MaterialDesc material = getMessage(build("/test.material", materialSrc), MaterialDesc.class);

        assertEquals("texture_sampler", material.getSamplers(0).getName());

        for (int i = 0; i < material.getMaxPageCount(); i++) {
            long sliceHash = MurmurHash.hash64(ShaderUtil.VariantTextureArrayFallback.samplerNameToSliceSamplerName("texture_sampler", i));
            assertEquals(sliceHash, material.getSamplers(0).getNameIndirections(i));
        }

        getProject().getProjectProperties().putBooleanValue("shader", "output_glsl120", false);
    }

    @Test
    public void testMigrateTextures() throws Exception {
        String srcShaderStr = "void main() {}\n";
        Graphics.ShaderDesc shaderDesc = addAndBuildShaderDescs(new String[]{"/test_vp.vp", "/test_fp.fp"}, new String[]{srcShaderStr,srcShaderStr}, "/test.shbundle");
        assertEquals(2, shaderDesc.getShadersCount());

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("vertex_program: \"/test_vp.vp\"\n");
        src.append("fragment_program: \"/test_fp.fp\"\n");

        // these should be migrated
        src.append("textures: \"tex0\"\n");
        src.append("textures: \"tex1\"\n");
        src.append("textures: \"tex2\"\n");

        // these should not be migrated
        src.append("textures: \"tex0_sampler\"\n");
        src.append("textures: \"tex_already_exist_in_samplers\"\n");

        src.append("samplers: {\n");
        src.append("    name: \"tex0_sampler\"\n");
        src.append("    wrap_u: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("    wrap_v: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("    filter_min: FILTER_MODE_MIN_LINEAR\n");
        src.append("    filter_mag: FILTER_MODE_MAG_LINEAR\n");
        src.append("}\n");

        src.append("samplers: {\n");
        src.append("    name: \"tex_already_exist_in_samplers\"\n");
        src.append("    wrap_u: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("    wrap_v: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("    filter_min: FILTER_MODE_MIN_LINEAR\n");
        src.append("    filter_mag: FILTER_MODE_MAG_LINEAR\n");
        src.append("}\n");

        addFile("/test.material", "");
        MaterialDesc material = getMessage(build("/test.material", src.toString()), MaterialDesc.class);
        assertEquals(0, material.getTexturesCount());
        assertEquals(5, material.getSamplersCount());

        List<MaterialDesc.Sampler> samplers = material.getSamplersList();
        assertEquals("tex0_sampler",                  samplers.get(0).getName());
        assertEquals("tex_already_exist_in_samplers", samplers.get(1).getName());
        assertEquals("tex0",                          samplers.get(2).getName());
        assertEquals("tex1",                          samplers.get(3).getName());
        assertEquals("tex2",                          samplers.get(4).getName());
    }

    private String makeMaterial(int[] tintRgb) {
        String materialFormat =
                """
                name: "sprite"
                tags: "tile"
                vertex_program: "/test_vp.vp"
                fragment_program: "/test_fp.fp"
                fragment_constants {
                  name: "tint"
                  type: CONSTANT_TYPE_USER
                  value {
                    x: %d.0
                    y: %d.0
                    z: %d.0
                    w: 1.0
                  }
                }
                """;
        return String.format(materialFormat, tintRgb[0], tintRgb[1], tintRgb[2]);
    }

    @Test
    public void testSharedShaderProgram() throws Exception {
        String vsShaderLegacy = "void main() {}\n";
        String fsShaderLegacy =
                """
                uniform vec4 tint;
                void main() {
                    gl_FragColor = tint;
                }
                """;

        addFile("/test_vp.vp", vsShaderLegacy);
        addFile("/test_fp.fp", fsShaderLegacy);

        String materialRedStr = makeMaterial(new int[]{1, 0, 0});
        String materialGreenStr = makeMaterial(new int[]{0, 1, 0});
        String materialBlueStr = makeMaterial(new int[]{0, 0, 1});

        addFile("/red.material", "");
        addFile("/green.material", "");
        addFile("/blue.material", "");

        MaterialDesc materialRed = getMessage(build("/red.material", materialRedStr), MaterialDesc.class);
        MaterialDesc materialGreen = getMessage(build("/green.material", materialGreenStr), MaterialDesc.class);
        MaterialDesc materialBlue = getMessage(build("/blue.material", materialBlueStr), MaterialDesc.class);

        assertTrue(materialRed.hasProgram());
        assertEquals(materialRed.getProgram(), materialGreen.getProgram());
        assertEquals(materialGreen.getProgram(), materialBlue.getProgram());
    }

    @Test
    public void testPbrParameters() throws Exception {
        String vsShaderStr = """
                #version 140
                in highp vec4 position;
                void main()
                {
                    gl_Position = position;
                }
                """;

        String fsShaderStr = """
                #version 140
                struct PbrMetallicRoughness
                {
                	vec4 baseColorFactor;
                	vec4 metallicAndRoughnessFactor;
                	vec4 metallicRoughnessTextures;
                };
                struct PbrSpecularGlossiness
                {
                    vec4 diffuseFactor;
                    vec4 specularAndSpecularGlossinessFactor;
                    vec4 specularGlossinessTextures;
                };
                struct PbrClearCoat
                {
                    vec4 clearCoatAndClearCoatRoughnessFactor;
                    vec4 clearCoatTextures;
                };
                struct PbrTransmission
                {
                	vec4 transmissionFactor;
                	vec4 transmissionTextures;
                };
                struct PbrIor
                {
                	vec4 ior;
                };
                struct PbrSpecular
                {
                	vec4 specularColorAndSpecularFactor;
                	vec4 specularTextures;
                };
                struct PbrVolume
                {
                	vec4 thicknessFactorAndAttenuationColor;
                	vec4 attenuationDistance;
                	vec4 volumeTextures;
                };
                struct PbrSheen
                {
                	vec4 sheenColorAndRoughnessFactor;
                	vec4 sheenTextures;
                };
                struct PbrEmissiveStrength
                {
                	vec4 emissiveStrength;
                };
                struct PbrIridescence
                {
                	vec4 iridescenceFactorAndIorAndThicknessMinMax;
                	vec4 iridescenceTextures;
                };
                uniform PbrMaterial
                {
                    vec4 pbrAlphaCutoffAndDoubleSidedAndIsUnlit;
                    vec4 pbrCommonTextures;
                    PbrMetallicRoughness pbrMetallicRoughness;
                    PbrSpecularGlossiness pbrSpecularGlossiness;
                    PbrClearCoat pbrClearCoat;
                    PbrTransmission pbrTransmission;
                    PbrIor pbrIor;
                    PbrSpecular pbrSpecular;
                    PbrVolume pbrVolume;
                    PbrSheen pbrSheen;
                    PbrEmissiveStrength pbrEmissiveStrength;
                    PbrIridescence pbrIridescence;
                };
                out vec4 color_out;
                void main()
                {
                    color_out = pbrMetallicRoughness.baseColorFactor + pbrSpecularGlossiness.diffuseFactor;
                }
                """;

        Graphics.ShaderDesc shaderDesc = addAndBuildShaderDescs(
                new String[]{"/test_pbr_parameters.vp", "/test_pbr_parameters.fp"},
                new String[]{vsShaderStr, fsShaderStr},
                "/test_pbr_parameters_bundle.shbundle");
        assertEquals(2, shaderDesc.getShadersCount());

        String src = """
                name: "test_pbr_parameters"
                vertex_program: "/test_pbr_parameters.vp"
                fragment_program: "/test_pbr_parameters.fp"
                """;

        MaterialDesc material = getMessage(build("/test_pbr_parameters_material.material", src), MaterialDesc.class);
        assertNotNull(material);
        assertTrue(material.hasProgram());

        MaterialDesc.PbrParameters pbrParameters = material.getPbrParameters();
        assertTrue(pbrParameters.getHasParameters());
        assertTrue(pbrParameters.getHasMetallicRoughness());
        assertTrue(pbrParameters.getHasSpecularGlossiness());
        assertTrue(pbrParameters.getHasClearcoat());
        assertTrue(pbrParameters.getHasTransmission());
        assertTrue(pbrParameters.getHasIor());
        assertTrue(pbrParameters.getHasSpecular());
        assertTrue(pbrParameters.getHasVolume());
        assertTrue(pbrParameters.getHasSheen());
        assertTrue(pbrParameters.getHasEmissiveStrength());
        assertTrue(pbrParameters.getHasIridescence());
    }
}
