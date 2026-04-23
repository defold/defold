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

import org.junit.Test;

import com.dynamo.proto.DdfStruct;

public class DDFTest {

    @Test
    public void testDdfStructUsage() {
        DdfStruct.Struct.Builder rootBuilder = DdfStruct.Struct.newBuilder();

        // Simple string field
        DdfStruct.Value stringValue = DdfStruct.Value.newBuilder()
                .setString("hello")
                .build();
        rootBuilder.putFields("string", stringValue);

        // Nested struct field
        DdfStruct.Struct.Builder nestedBuilder = DdfStruct.Struct.newBuilder();
        DdfStruct.Value nestedNumber = DdfStruct.Value.newBuilder()
                .setNumber(42.0)
                .build();
        nestedBuilder.putFields("answer", nestedNumber);
        DdfStruct.Value nestedStructValue = DdfStruct.Value.newBuilder()
                .setStruct(nestedBuilder.build())
                .build();
        rootBuilder.putFields("nested", nestedStructValue);

        // List field with different kinds of values
        DdfStruct.ListValue list = DdfStruct.ListValue.newBuilder()
                .addValues(DdfStruct.Value.newBuilder().setBool(true).build())
                .addValues(DdfStruct.Value.newBuilder().setNumber(3.14).build())
                .addValues(DdfStruct.Value.newBuilder().setString("world").build())
                .build();
        DdfStruct.Value listValue = DdfStruct.Value.newBuilder()
                .setList(list)
                .build();
        rootBuilder.putFields("list", listValue);

        // Explicit null field
        DdfStruct.Value nullValue = DdfStruct.Value.newBuilder()
                .setNull(DdfStruct.NullValue.NULL_VALUE)
                .build();
        rootBuilder.putFields("null", nullValue);

        DdfStruct.Struct root = rootBuilder.build();

        // Verify top-level fields
        assertEquals(4, root.getFieldsCount());
        assertEquals("hello", root.getFieldsOrThrow("string").getString());

        // Verify nested struct
        DdfStruct.Value nested = root.getFieldsOrThrow("nested");
        assertTrue(nested.hasStruct());
        DdfStruct.Struct nestedStruct = nested.getStruct();
        assertEquals(1, nestedStruct.getFieldsCount());
        assertEquals(42.0, nestedStruct.getFieldsOrThrow("answer").getNumber(), 0.0);

        // Verify list contents
        DdfStruct.Value listField = root.getFieldsOrThrow("list");
        assertTrue(listField.hasList());
        DdfStruct.ListValue listValueOut = listField.getList();
        assertEquals(3, listValueOut.getValuesCount());
        assertTrue(listValueOut.getValues(0).getBool());
        assertEquals(3.14, listValueOut.getValues(1).getNumber(), 0.0);
        assertEquals("world", listValueOut.getValues(2).getString());

        // Verify null field
        DdfStruct.Value nullField = root.getFieldsOrThrow("null");
        assertTrue(nullField.hasNull());
        assertEquals(DdfStruct.NullValue.NULL_VALUE, nullField.getNull());
    }
}

