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
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.Message;

public class LightBuilderTest extends AbstractProtoBuilderTest {

    private static final String LIGHT_SOURCE =
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

    private static final String POINT_LIGHT_SOURCE =
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
            assertTrue(e.getMessage().contains(expectedMessagePart));
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
    public void testLightBuildersUseSourceSpecificOutputExtensions() throws Exception {
        for (String ext : new String[] {"point_light", "directional_light", "spot_light"}) {
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
    public void testLightBuilderRequiresStructPayload() throws Exception {
        String source = "data { number: 1.0 }\n";
        assertCompileFailure("/light/test.point_light", source, "light data must contain a struct payload");
    }
}
