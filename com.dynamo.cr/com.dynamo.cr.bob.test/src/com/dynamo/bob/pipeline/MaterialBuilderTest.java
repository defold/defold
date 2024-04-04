// Copyright 2020-2024 The Defold Foundation
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

import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Material.MaterialDesc;
import com.google.protobuf.Message;

public class MaterialBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testMigrateTextures() throws Exception {

        addFile("/test.material", "");
        addFile("/test.vp", "");
        addFile("/test.fp", "");

        build("/test.vp", "");
        build("/test.fp", "");

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

        MaterialDesc material = (MaterialDesc) build("/test.material", src.toString()).get(0);
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
