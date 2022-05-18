// Copyright 2020-2022 The Defold Foundation
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

package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertNotNull;

import com.dynamo.bob.util.BobProjectProperties;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.text.ParseException;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class BobProjectPropertiesTest {
    @Before
    public void setUp() throws Exception {
    }

    @After
    public void tearDown() throws Exception {
    }

    BobProjectProperties createProperties() throws IOException, ParseException {
        BobProjectProperties properties = new BobProjectProperties();
        properties.loadDefaultMetaFile();
        return properties;
    }

    void load(BobProjectProperties properties, String load) throws IOException, ParseException {
        properties.load(new ByteArrayInputStream(load.getBytes()));
    }

    void testValue(String p, String c, String k, Object expected) throws IOException, ParseException {
        BobProjectProperties properties = createProperties();
        load(properties, p);
        Map<String, Map<String, Object>> map = properties.createTypedMap(new BobProjectProperties.PropertyType[]{BobProjectProperties.PropertyType.BOOL});

        Object actual = map.get(c).get(k);
        assertEquals(expected, actual);
    }

    @Test
    public void testTypedProperties() throws IOException, ParseException {
        testValue("[project]\nwrite_log=0", "project", "write_log", false);
        testValue("[project]\nwrite_log=1", "project", "write_log", true);
        testValue("", "project", "write_log", false);
        testValue("", "project", "title", "unnamed");
    }

    @Test
    public void testProperties() throws IOException, ParseException {
        BobProjectProperties properties = createProperties();

        assertEquals(false, properties.getBooleanValue("html5", "doesn't_exist", false));
        assertEquals(new Integer(834), properties.getIntValue("html5", "doesn't_exist", 834));

        assertEquals(false, properties.getBooleanValue("display", "fullscreen"));
        assertEquals(true, properties.getBooleanValue("sound", "use_thread"));
        assertEquals(new Integer(960), properties.getIntValue("display", "width"));
    }

    @Test
    public void testPropertiesOverride() throws IOException, ParseException {
        BobProjectProperties properties = createProperties();
        String main = "[project]\ntitle = Title one\ncustom_string_list#0 = http://test.com/test.zip\ncustom_string_list#2 = http://test.com/test2.zip\ncustom_string_list#1 = http://test.com/test1.zip\n";
        String override = "[project]\ntitle = Title two\ncustom_string_list=http://test.com/new.zip,http://test.com/new1.zip";
        load(properties, main);
        load(properties, override);

        assertEquals("Title two", properties.getStringValue("project", "title"));
        assertEquals("http://test.com/new.zip,http://test.com/new1.zip", properties.getStringValue("project", "custom_string_list"));

        String[] array = properties.getStringArrayValue("project", "custom_string_list");
        assertEquals(2, array.length);
        assertEquals("http://test.com/new.zip", array[0]);
        assertEquals("http://test.com/new1.zip", array[1]);
    }
}
