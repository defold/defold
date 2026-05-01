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

import static org.junit.Assert.assertEquals;
import java.util.List;

import com.dynamo.graphics.proto.Graphics;
import com.dynamo.render.proto.Compute;
import com.google.protobuf.Message;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Compute.ComputeDesc;

public class ComputeBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    @Test
    public void testSimple() throws Exception {

        String srcCompute = """
                #version 430
                void main() {}
                """;

        addAndBuildShaderDesc("/test.cp", srcCompute, "/test.shbundle");

        StringBuilder src = new StringBuilder();
        src.append("compute_program: \"/test.cp\"\n");

        src.append("constants {\n");
        src.append("  name: \"constant_one\"\n");
        src.append("  type: CONSTANT_TYPE_USER\n");
        src.append("  value {\n");
        src.append("    x: 1.0\n");
        src.append("    y: 0.0\n");
        src.append("    z: 0.0\n");
        src.append("    w: 0.0\n");
        src.append("  }\n");
        src.append("}\n");

        src.append("constants {\n");
        src.append("  name: \"constant_two\"\n");
        src.append("  type: CONSTANT_TYPE_VIEWPROJ\n");
        src.append("}\n");

        src.append("samplers {\n");
        src.append("  name: \"texture_in\"\n");
        src.append("  wrap_u: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("  wrap_v: WRAP_MODE_CLAMP_TO_EDGE\n");
        src.append("  filter_min: FILTER_MODE_MIN_LINEAR\n");
        src.append("  filter_mag: FILTER_MODE_MAG_LINEAR\n");
        src.append("}\n");

        src.append("samplers {\n");
        src.append("  name: \"texture_out\"\n");
        src.append("  wrap_u: WRAP_MODE_REPEAT\n");
        src.append("  wrap_v: WRAP_MODE_REPEAT\n");
        src.append("  filter_min: FILTER_MODE_MIN_NEAREST\n");
        src.append("  filter_mag: FILTER_MODE_MAG_NEAREST\n");
        src.append("}\n");

        List<Message> msg = build("/test.compute", src.toString());

        ComputeDesc compute = getMessage(build("/test.compute", src.toString()), ComputeDesc.class);

        assertEquals(2, compute.getSamplersCount());
        assertEquals(2, compute.getConstantsCount());

        List<MaterialDesc.Sampler> samplers = compute.getSamplersList();
        List<MaterialDesc.Constant> constants = compute.getConstantsList();

        assertEquals("texture_in", samplers.get(0).getName());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE, samplers.get(0).getWrapU());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_CLAMP_TO_EDGE, samplers.get(0).getWrapV());
        assertEquals(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_LINEAR, samplers.get(0).getFilterMin());
        assertEquals(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_LINEAR, samplers.get(0).getFilterMag());

        assertEquals("texture_out", samplers.get(1).getName());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_REPEAT, samplers.get(1).getWrapU());
        assertEquals(MaterialDesc.WrapMode.WRAP_MODE_REPEAT, samplers.get(1).getWrapV());
        assertEquals(MaterialDesc.FilterModeMin.FILTER_MODE_MIN_NEAREST, samplers.get(1).getFilterMin());
        assertEquals(MaterialDesc.FilterModeMag.FILTER_MODE_MAG_NEAREST, samplers.get(1).getFilterMag());

        float EPSILON = 0.0001f;

        assertEquals("constant_one", constants.get(0).getName());
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_USER, constants.get(0).getType());
        assertEquals(1.0, constants.get(0).getValue(0).getX(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getY(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getZ(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getW(), EPSILON);

        assertEquals("constant_two", constants.get(1).getName());
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_VIEWPROJ, constants.get(1).getType());
    }
}
