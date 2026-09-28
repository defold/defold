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
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import java.util.List;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.ListValue;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class LightBuilderTest extends AbstractProtoBuilderTest {

    private static final String LIGHT_SOURCE =
            "data {\n" +
            "  struct {\n" +
            "    fields {\n" +
            "      key: \"color\"\n" +
            "      value {\n" +
            "        list {\n" +
            "          values { number: 0.25 }\n" +
            "          values { number: 0.5 }\n" +
            "          values { number: 0.75 }\n" +
            "        }\n" +
            "      }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"intensity\"\n" +
            "      value { number: 1.0 }\n" +
            "    }\n" +
            "  }\n" +
            "}\n";

    private static final String POINT_LIGHT_SOURCE =
            "data {\n" +
            "  struct {\n" +
            "    fields {\n" +
            "      key: \"color\"\n" +
            "      value {\n" +
            "        list {\n" +
            "          values { number: 1.0 }\n" +
            "          values { number: 0.5 }\n" +
            "          values { number: 0.25 }\n" +
            "        }\n" +
            "      }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"intensity\"\n" +
            "      value { number: 1.0 }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"range\"\n" +
            "      value { number: 10.0 }\n" +
            "    }\n" +
            "  }\n" +
            "}\n";

    private static final String SPOT_LIGHT_SOURCE =
            "data {\n" +
            "  struct {\n" +
            "    fields {\n" +
            "      key: \"color\"\n" +
            "      value {\n" +
            "        list {\n" +
            "          values { number: 0.2 }\n" +
            "          values { number: 0.8 }\n" +
            "          values { number: 0.1 }\n" +
            "        }\n" +
            "      }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"intensity\"\n" +
            "      value { number: 1.0 }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"range\"\n" +
            "      value { number: 10.0 }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"inner_cone_angle\"\n" +
            "      value { number: 15.0 }\n" +
            "    }\n" +
            "    fields {\n" +
            "      key: \"outer_cone_angle\"\n" +
            "      value { number: 30.0 }\n" +
            "    }\n" +
            "  }\n" +
            "}\n";

    private static final String AREA_LIGHT_SOURCE =
            "data { struct {\n" +
            "  fields { key: \"color\" value { list {\n" +
            "    values { number: 0.25 } values { number: 0.5 } values { number: 0.75 }\n" +
            "  } } }\n" +
            "  fields { key: \"intensity\" value { number: 2.0 } }\n" +
            "  fields { key: \"range\" value { number: 12.0 } }\n" +
            "  fields { key: \"width\" value { number: 3.0 } }\n" +
            "  fields { key: \"height\" value { number: 4.0 } }\n" +
            "} }\n";

    private static Data.Builder areaLightSourceBuilder() throws TextFormat.ParseException {
        Data.Builder builder = Data.newBuilder();
        TextFormat.merge(AREA_LIGHT_SOURCE, builder);
        return builder;
    }

    private void assertLightTags(String path, String source, String expectedTypeTag) throws Exception {
        List<Message> messages = build(path, source);
        assertEquals(1, messages.size());

        Data data = getMessage(messages, Data.class);
        assertNotNull(data);
        assertEquals(2, data.getTagsCount());
        assertEquals("light", data.getTags(0));
        assertEquals(expectedTypeTag, data.getTags(1));
    }

    private void assertCompileFailure(String path, String source, String expectedMessagePart) throws Exception {
        try {
            build(path, source);
            fail("Expected light build to fail");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains(expectedMessagePart));
        }
    }

    private void assertBuiltLightColor(String path, String source, double... expectedComponents) throws Exception {
        List<Message> messages = build(path, source);
        Data data = getMessage(messages, Data.class);
        assertNotNull(data);
        List<Value> colorComponents = data.getData().getStruct().getFieldsOrThrow("color").getList().getValuesList();
        assertEquals(expectedComponents.length, colorComponents.size());

        for (int i = 0; i < expectedComponents.length; i++) {
            assertEquals(expectedComponents[i], colorComponents.get(i).getNumber(), 0.000001);
        }
    }

    @Test
    public void testPointLightBuilderTags() throws Exception {
        assertLightTags("/light/test.point_light", POINT_LIGHT_SOURCE, "point_light");
    }

    @Test
    public void testDirectionalLightBuilderTags() throws Exception {
        assertLightTags("/light/test.directional_light", LIGHT_SOURCE, "directional_light");
    }

    @Test
    public void testSpotLightBuilderTags() throws Exception {
        assertLightTags("/light/test.spot_light", SPOT_LIGHT_SOURCE, "spot_light");
    }

    @Test
    public void testAmbientLightBuilderTags() throws Exception {
        assertLightTags("/light/test.ambient_light", LIGHT_SOURCE, "ambient_light");
    }

    // Verifies area light tags and extension mapping, including mixed-case source paths.
    @Test
    public void testAreaLightBuilderTagsAndExtension() throws Exception {
        assertLightTags("/light/Test.ArEa_LiGhT", AREA_LIGHT_SOURCE, "area_light");
        assertNotNull(getFile("build/light/Test.area_light.lightc"));
        assertEquals(".area_light.lightc", ResourceUtil.getOutputExt(".area_light"));
    }

    // Verifies the serialized rectangle dimensions and common fields retain their source values.
    @Test
    public void testAreaLightBuilderSerializesShape() throws Exception {
        Data data = getMessage(build("/light/test.area_light", AREA_LIGHT_SOURCE), Data.class);
        assertNotNull(data);
        assertEquals(2.0, data.getData().getStruct().getFieldsOrThrow("intensity").getNumber(), 0.000001);
        assertEquals(12.0, data.getData().getStruct().getFieldsOrThrow("range").getNumber(), 0.000001);
        assertEquals(3.0, data.getData().getStruct().getFieldsOrThrow("width").getNumber(), 0.000001);
        assertEquals(4.0, data.getData().getStruct().getFieldsOrThrow("height").getNumber(), 0.000001);
        assertBuiltLightColor("/light/color.area_light", AREA_LIGHT_SOURCE, 0.25, 0.5, 0.75);
    }

    // Verifies all area fields are required instead of silently substituting editor template defaults.
    @Test
    public void testAreaLightBuilderRequiresFields() throws Exception {
        for (String field : new String[] {"color", "intensity", "range", "width", "height"}) {
            Data.Builder source = areaLightSourceBuilder();
            source.getDataBuilder().getStructBuilder().removeFields(field);
            assertCompileFailure("/light/missing_" + field + ".area_light", source.toString(), "missing required field '" + field + "'");
        }
    }

    // Verifies malformed area fields fail with field-specific errors before resource serialization.
    @Test
    public void testAreaLightBuilderRejectsMalformedFields() throws Exception {
        for (String field : new String[] {"intensity", "range", "width", "height"}) {
            Data.Builder source = areaLightSourceBuilder();
            source.getDataBuilder().getStructBuilder().putFields(field, Value.newBuilder().setString("invalid").build());
            assertCompileFailure("/light/malformed_" + field + ".area_light", source.toString(), "field '" + field + "' must be a number");
        }

        Data.Builder source = areaLightSourceBuilder();
        source.getDataBuilder().getStructBuilder().putFields("color", Value.newBuilder().setNumber(1.0).build());
        assertCompileFailure("/light/malformed_color.area_light", source.toString(), "field 'color' must be a list of 3 numbers");

        ListValue.Builder color = areaLightSourceBuilder().getData().getStruct().getFieldsOrThrow("color").getList().toBuilder();
        color.setValues(1, Value.newBuilder().setString("invalid").build());
        source.getDataBuilder().getStructBuilder().putFields("color", Value.newBuilder().setList(color).build());
        assertCompileFailure("/light/malformed_color_component.area_light", source.toString(), "field 'color' must contain only numbers");

        color.removeValues(1);
        source.getDataBuilder().getStructBuilder().putFields("color", Value.newBuilder().setList(color).build());
        assertCompileFailure("/light/malformed_color_length.area_light", source.toString(), "field 'color' must contain 3 numbers");
        assertCompileFailure("/light/malformed_payload.area_light", "data { number: 1.0 }", "light data must contain a struct payload");
    }

    // Verifies NaN, infinity, and float overflow cannot reach area light shader inputs.
    @Test
    public void testAreaLightBuilderRejectsNonFiniteFields() throws Exception {
        double[] invalidNumbers = {Double.NaN, Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, 1.0e100, -1.0e100};
        for (int i = 0; i < invalidNumbers.length; ++i) {
            Value number = Value.newBuilder().setNumber(invalidNumbers[i]).build();
            for (String field : new String[] {"intensity", "range", "width", "height"}) {
                Data.Builder source = areaLightSourceBuilder();
                source.getDataBuilder().getStructBuilder().putFields(field, number);
                assertCompileFailure("/light/nonfinite_" + field + i + ".area_light", source.toString(), "field '" + field + "' must contain finite numbers");
            }
            for (int component = 0; component < 3; ++component) {
                Data.Builder source = areaLightSourceBuilder();
                ListValue.Builder color = source.getData().getStruct().getFieldsOrThrow("color").getList().toBuilder();
                color.setValues(component, number);
                source.getDataBuilder().getStructBuilder().putFields("color", Value.newBuilder().setList(color).build());
                assertCompileFailure("/light/nonfinite_color" + component + "_" + i + ".area_light", source.toString(), "field 'color' must contain finite numbers");
            }
        }
    }

    // Verifies negative area parameters clamp to zero and zero remains available to disable contribution.
    @Test
    public void testAreaLightBuilderNormalizesNonnegativeFields() throws Exception {
        for (double number : new double[] {-2.0, 0.0}) {
            Data.Builder source = areaLightSourceBuilder();
            for (String field : new String[] {"intensity", "range", "width", "height"}) {
                source.getDataBuilder().getStructBuilder().putFields(field, Value.newBuilder().setNumber(number).build());
            }
            Data data = getMessage(build("/light/normalized" + number + ".area_light", source.toString()), Data.class);
            assertNotNull(data);
            for (String field : new String[] {"intensity", "range", "width", "height"}) {
                assertEquals(field, 0.0, data.getData().getStruct().getFieldsOrThrow(field).getNumber(), 0.0);
            }
        }
    }

    @Test
    public void testLightBuilderWithMixedCaseExtension() throws Exception {
        assertLightTags("/light/Test.PoInT_LiGhT", POINT_LIGHT_SOURCE, "point_light");
        assertNotNull(getFile("build/light/Test.point_light.lightc"));
    }

    @Test
    public void testLightBuilderColor() throws Exception {
        assertBuiltLightColor("/light/test.directional_light", LIGHT_SOURCE, 0.25, 0.5, 0.75);
        assertBuiltLightColor("/light/test.ambient_light", LIGHT_SOURCE, 0.25, 0.5, 0.75);
        assertBuiltLightColor("/light/test.point_light", POINT_LIGHT_SOURCE, 1.0, 0.5, 0.25);
        assertBuiltLightColor("/light/test.spot_light", SPOT_LIGHT_SOURCE, 0.2, 0.8, 0.1);
    }

    @Test
    public void testLightBuilderRejectsAlphaComponent() throws Exception {
        String source =
                "data {\n" +
                "  struct {\n" +
                "    fields {\n" +
                "      key: \"color\"\n" +
                "      value {\n" +
                "        list {\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "        }\n" +
                "      }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"intensity\"\n" +
                "      value { number: 1.0 }\n" +
                "    }\n" +
                "  }\n" +
                "}\n";

        assertCompileFailure("/light/test.directional_light", source, "field 'color' must contain 3 numbers");
    }

    @Test
    public void testLightBuildersUseSourceSpecificOutputExtensions() throws Exception {
        for (String ext : new String[] {"point_light", "directional_light", "spot_light", "ambient_light"}) {
            String source = ext.equals("spot_light") ? SPOT_LIGHT_SOURCE : ext.equals("point_light") ? POINT_LIGHT_SOURCE : LIGHT_SOURCE;
            String path = "/light/test." + ext;

            build(path, source);
            assertNotNull(getFile("build/light/test." + ext + ".lightc"));
            assertEquals("." + ext + ".lightc", ResourceUtil.getOutputExt("." + ext));
        }
    }

    @Test
    public void testSpotLightBuilderConvertsConeAnglesToRadians() throws Exception {
        String source =
                "data {\n" +
                "  struct {\n" +
                "    fields {\n" +
                "      key: \"color\"\n" +
                "      value {\n" +
                "        list {\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 0.5 }\n" +
                "          values { number: 0.25 }\n" +
                "        }\n" +
                "      }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"intensity\"\n" +
                "      value { number: 2.0 }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"range\"\n" +
                "      value { number: 12.0 }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"inner_cone_angle\"\n" +
                "      value { number: 15.0 }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"outer_cone_angle\"\n" +
                "      value { number: 30.0 }\n" +
                "    }\n" +
                "  }\n" +
                "}\n";

        List<Message> messages = build("/light/test.spot_light", source);
        Data data = getMessage(messages, Data.class);
        assertNotNull(data);

        Value payload = data.getData().getStruct().getFieldsOrThrow("inner_cone_angle");
        assertEquals(Math.toRadians(15.0), payload.getNumber(), 0.000001);

        payload = data.getData().getStruct().getFieldsOrThrow("outer_cone_angle");
        assertEquals(Math.toRadians(30.0), payload.getNumber(), 0.000001);
    }

    @Test
    public void testPointLightBuilderRequiresRange() throws Exception {
        assertCompileFailure("/light/test.point_light", LIGHT_SOURCE, "missing required field 'range'");
    }

    @Test
    public void testDirectionalLightBuilderRequiresIntensity() throws Exception {
        String source =
                "data {\n" +
                "  struct {\n" +
                "    fields {\n" +
                "      key: \"color\"\n" +
                "      value {\n" +
                "        list {\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "        }\n" +
                "      }\n" +
                "    }\n" +
                "  }\n" +
                "}\n";
        assertCompileFailure("/light/test.directional_light", source, "missing required field 'intensity'");
    }

    @Test
    public void testAmbientLightBuilderRequiresIntensity() throws Exception {
        String source =
                "data {\n" +
                "  struct {\n" +
                "    fields {\n" +
                "      key: \"color\"\n" +
                "      value {\n" +
                "        list {\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "          values { number: 1.0 }\n" +
                "        }\n" +
                "      }\n" +
                "    }\n" +
                "  }\n" +
                "}\n";
        assertCompileFailure("/light/test.ambient_light", source, "missing required field 'intensity'");
    }

    @Test
    public void testLightBuilderRequiresStructPayload() throws Exception {
        String source = "data { number: 1.0 }\n";
        assertCompileFailure("/light/test.point_light", source, "light data must contain a struct payload");
    }
}
