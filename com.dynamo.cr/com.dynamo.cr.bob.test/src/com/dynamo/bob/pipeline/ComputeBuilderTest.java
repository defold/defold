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

import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Compute.ComputeDesc;

public class ComputeBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();
    }

    private static void addConstant(StringBuilder src, String name, MaterialDesc.ConstantType type) {
        src.append("constants {\n");
        src.append("  name: \"").append(name).append("\"\n");
        src.append("  type: ").append(type.name()).append("\n");
        src.append("}\n");
    }

    private static void addConstant(StringBuilder src, String name, MaterialDesc.ConstantType type, float x, float y, float z, float w) {
        src.append("constants {\n");
        src.append("  name: \"").append(name).append("\"\n");
        src.append("  type: ").append(type.name()).append("\n");
        src.append("  value {\n");
        src.append("    x: ").append(x).append("\n");
        src.append("    y: ").append(y).append("\n");
        src.append("    z: ").append(z).append("\n");
        src.append("    w: ").append(w).append("\n");
        src.append("  }\n");
        src.append("}\n");
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

        MaterialDesc.ConstantType[] constantTypes = {
                MaterialDesc.ConstantType.CONSTANT_TYPE_USER,
                MaterialDesc.ConstantType.CONSTANT_TYPE_VIEWPROJ,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLD,
                MaterialDesc.ConstantType.CONSTANT_TYPE_TEXTURE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_VIEW,
                MaterialDesc.ConstantType.CONSTANT_TYPE_PROJECTION,
                MaterialDesc.ConstantType.CONSTANT_TYPE_NORMAL,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEW,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEWPROJ,
                MaterialDesc.ConstantType.CONSTANT_TYPE_USER_MATRIX4,
                MaterialDesc.ConstantType.CONSTANT_TYPE_TIME,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLD_INVERSE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_VIEW_INVERSE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_PROJECTION_INVERSE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_VIEWPROJ_INVERSE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEW_INVERSE,
                MaterialDesc.ConstantType.CONSTANT_TYPE_WORLDVIEWPROJ_INVERSE
        };

        for (int i = 0; i < constantTypes.length; ++i) {
            MaterialDesc.ConstantType constantType = constantTypes[i];
            String constantName = "constant_" + i;
            if (constantType == MaterialDesc.ConstantType.CONSTANT_TYPE_USER) {
                addConstant(src, constantName, constantType, 1.0f, 0.0f, 0.0f, 0.0f);
            } else if (constantType == MaterialDesc.ConstantType.CONSTANT_TYPE_USER_MATRIX4) {
                addConstant(src, constantName, constantType, 2.0f, 3.0f, 4.0f, 5.0f);
            } else {
                addConstant(src, constantName, constantType);
            }
        }

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

        ComputeDesc compute = getMessage(build("/test.compute", src.toString()), ComputeDesc.class);

        assertEquals(2, compute.getSamplersCount());
        assertEquals(constantTypes.length, compute.getConstantsCount());

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

        assertEquals("constant_0", constants.get(0).getName());
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_USER, constants.get(0).getType());
        assertEquals(1.0, constants.get(0).getValue(0).getX(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getY(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getZ(), EPSILON);
        assertEquals(0.0, constants.get(0).getValue(0).getW(), EPSILON);

        for (int i = 0; i < constantTypes.length; ++i) {
            assertEquals("constant_" + i, constants.get(i).getName());
            assertEquals(constantTypes[i], constants.get(i).getType());
        }

        assertEquals("constant_9", constants.get(9).getName());
        assertEquals(MaterialDesc.ConstantType.CONSTANT_TYPE_USER_MATRIX4, constants.get(9).getType());
        assertEquals(2.0, constants.get(9).getValue(0).getX(), EPSILON);
        assertEquals(3.0, constants.get(9).getValue(0).getY(), EPSILON);
        assertEquals(4.0, constants.get(9).getValue(0).getZ(), EPSILON);
        assertEquals(5.0, constants.get(9).getValue(0).getW(), EPSILON);
    }
}
