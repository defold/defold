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
import static org.junit.Assert.assertNotNull;

import java.util.List;

import org.junit.Test;

import com.dynamo.gamesys.proto.DataProto.Data;
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

    private void assertLightTags(String path, String expectedTypeTag) throws Exception {
        List<Message> messages = build(path, LIGHT_SOURCE);
        assertEquals(1, messages.size());

        Data data = getMessage(messages, Data.class);
        assertNotNull(data);
        assertEquals(2, data.getTagsCount());
        assertEquals("light", data.getTags(0));
        assertEquals(expectedTypeTag, data.getTags(1));
    }

    @Test
    public void testPointLightBuilderTags() throws Exception {
        assertLightTags("/light/test.point_light", "point_light");
    }

    @Test
    public void testDirectionalLightBuilderTags() throws Exception {
        assertLightTags("/light/test.directional_light", "directional_light");
    }

    @Test
    public void testSpotLightBuilderTags() throws Exception {
        assertLightTags("/light/test.spot_light", "spot_light");
    }
}
