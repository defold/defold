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

import java.util.List;
import java.util.Map;

import org.junit.Test;

import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.ListValue;
import com.dynamo.proto.DdfStruct.Struct;
import com.dynamo.proto.DdfStruct.Value;
import com.google.protobuf.Message;

public class DataBuilderTest extends AbstractProtoBuilderTest {

    @Test
    public void testDataBuilderSimple() throws Exception {
        String source =
                "tags: \"tag-one\"\n" +
                "tags: \"tag-two\"\n" +
                "data {\n" +
                "  string: \"hello\"\n" +
                "}\n";

        List<Message> messages = build("/data/test.data", source);
        assertEquals(1, messages.size());

        Data data = getMessage(messages, Data.class);
        assertTrue(data != null);

        assertEquals(2, data.getTagsCount());
        assertEquals("tag-one", data.getTags(0));
        assertEquals("tag-two", data.getTags(1));

        assertTrue(data.hasData());
        assertEquals("hello", data.getData().getString());
    }

    @Test
    public void testDataBuilderNestedStructAndList() throws Exception {
        String source =
                "tags: \"checked_tag\"\n" +
                "data {\n" +
                "  struct {\n" +
                "    fields {\n" +
                "      key: \"name\"\n" +
                "      value { string: \"Checked Player\" }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"health\"\n" +
                "      value { number: 100 }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"active\"\n" +
                "      value { bool: true }\n" +
                "    }\n" +
                "    fields {\n" +
                "      key: \"tags\"\n" +
                "      value {\n" +
                "        list {\n" +
                "          values { string: \"tag_a\" }\n" +
                "          values { string: \"tag_b\" }\n" +
                "          values { string: \"tag_c\" }\n" +
                "        }\n" +
                "      }\n" +
                "    }\n" +
                "  }\n" +
                "}\n";

        List<Message> messages = build("/data/nested.data", source);
        assertEquals(1, messages.size());

        Data data = getMessage(messages, Data.class);
        assertTrue(data != null);

        // Top-level tags
        assertEquals(1, data.getTagsCount());
        assertEquals("checked_tag", data.getTags(0));

        // Nested Value/Struct/ListValue content
        assertTrue(data.hasData());
        Value rootValue = data.getData();
        assertTrue(rootValue.hasStruct());

        Struct struct = rootValue.getStruct();
        Map<String, Value> fields = struct.getFieldsMap();

        Value nameValue = fields.get("name");
        assertTrue(nameValue != null);
        assertTrue(nameValue.hasString());
        assertEquals("Checked Player", nameValue.getString());

        Value healthValue = fields.get("health");
        assertTrue(healthValue != null);
        assertTrue(healthValue.hasNumber());
        assertEquals(100.0, healthValue.getNumber(), 0.0);

        Value activeValue = fields.get("active");
        assertTrue(activeValue != null);
        assertTrue(activeValue.hasBool());
        assertTrue(activeValue.getBool());

        Value tagsValue = fields.get("tags");
        assertTrue(tagsValue != null);
        assertTrue(tagsValue.hasList());

        ListValue list = tagsValue.getList();
        assertEquals(3, list.getValuesCount());
        assertEquals("tag_a", list.getValues(0).getString());
        assertEquals("tag_b", list.getValues(1).getString());
        assertEquals("tag_c", list.getValues(2).getString());
    }
}

