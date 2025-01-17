// Copyright 2020-2025 The Defold Foundation
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

import static org.junit.Assert.assertEquals;
import java.util.List;

import com.dynamo.graphics.proto.Graphics;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Material.MaterialDesc;

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
            src.append("    semantic_type: " + semanticType);
        }
        src.append("    double_values {");
        for (int i=0; i < elementCount; i++) {
            src.append("        v: 1.0");
        }
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

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_combined\"\n");
        src.append("vertex_program: \"/test_combined.vp\"\n");
        src.append("fragment_program: \"/test_combined.fp\"\n");

        MaterialDesc material = (MaterialDesc) build("/test_migrate_vx_attributes.material", src.toString()).get(0);
    }

    @Test
    public void testMigrateVertexAttributes() throws Exception {
        addFile("/test_migrate_vx_attributes.material", "");
        addFile("/test_migrate_vx_attributes.vp", "");
        addFile("/test_migrate_vx_attributes.fp", "");

        StringBuilder srcShader = new StringBuilder();
        srcShader.append("void main() {}\n");

        build("/test_migrate_vx_attributes.vp", srcShader.toString());
        build("/test_migrate_vx_attributes.fp", srcShader.toString());

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
    public void testMigrateTextures() throws Exception {

        addFile("/test.material", "");
        addFile("/test.vp", "");
        addFile("/test.fp", "");

        StringBuilder srcShader = new StringBuilder();
        srcShader.append("void main() {}\n");

        build("/test.vp", srcShader.toString());
        build("/test.fp", srcShader.toString());

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("vertex_program: \"/test.vp\"\n");
        src.append("fragment_program: \"/test.fp\"\n");

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
}
