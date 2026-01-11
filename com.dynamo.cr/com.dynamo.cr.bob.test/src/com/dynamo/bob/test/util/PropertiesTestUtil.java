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

package com.dynamo.bob.test.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarationEntry;
import com.dynamo.properties.proto.PropertiesProto.PropertyDeclarations;

public class PropertiesTestUtil {
    public static void assertNumber(PropertyDeclarations properties, float expected, int index) {
        assertEquals(expected, properties.getFloatValues(properties.getNumberEntries(index).getIndex()), 0);
    }

    public static void assertHash(PropertyDeclarations properties, long expected, int index) {
        assertEquals(expected, properties.getHashValues(properties.getHashEntries(index).getIndex()));
    }

    public static void assertURL(PropertyDeclarations properties, String expected, int index) {
        assertEquals(expected, properties.getStringValues(properties.getUrlEntries(index).getIndex()));
    }

    public static void assertVector3(PropertyDeclarations properties, float expectedX, float expectedY, float expectedZ, int index) {
        PropertyDeclarationEntry entry = properties.getVector3Entries(index);
        assertEquals(expectedX, properties.getFloatValues(entry.getIndex()+0), 0);
        assertEquals(expectedY, properties.getFloatValues(entry.getIndex()+1), 0);
        assertEquals(expectedZ, properties.getFloatValues(entry.getIndex()+2), 0);
    }

    public static void assertVector4(PropertyDeclarations properties, float expectedX, float expectedY, float expectedZ, float expectedW, int index) {
        PropertyDeclarationEntry entry = properties.getVector4Entries(index);
        assertEquals(expectedX, properties.getFloatValues(entry.getIndex()+0), 0);
        assertEquals(expectedY, properties.getFloatValues(entry.getIndex()+1), 0);
        assertEquals(expectedZ, properties.getFloatValues(entry.getIndex()+2), 0);
        assertEquals(expectedW, properties.getFloatValues(entry.getIndex()+3), 0);
    }

    public static void assertQuat(PropertyDeclarations properties, float expectedX, float expectedY, float expectedZ, float expectedW, int index) {
        PropertyDeclarationEntry entry = properties.getQuatEntries(index);
        assertEquals(expectedX, properties.getFloatValues(entry.getIndex()+0), 0);
        assertEquals(expectedY, properties.getFloatValues(entry.getIndex()+1), 0);
        assertEquals(expectedZ, properties.getFloatValues(entry.getIndex()+2), 0);
        assertEquals(expectedW, properties.getFloatValues(entry.getIndex()+3), 0);
    }

    public static void assertBoolean(PropertyDeclarations properties, boolean expected, int index) {
        assertTrue(expected == (properties.getFloatValues(properties.getBoolEntries(index).getIndex()) != 0.0f));
    }
}
