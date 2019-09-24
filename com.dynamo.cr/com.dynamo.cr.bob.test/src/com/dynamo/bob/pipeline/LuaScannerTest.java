package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.apache.commons.io.IOUtils;
import org.junit.Test;

import com.dynamo.bob.pipeline.LuaScanner.Property;
import com.dynamo.bob.pipeline.LuaScanner.Property.Status;

public class LuaScannerTest {

    String getFile(String file) throws IOException {
        InputStream input = getClass().getResourceAsStream(file);
        ByteArrayOutputStream output = new ByteArrayOutputStream(1024);
        IOUtils.copy(input, output);
        return new String(output.toByteArray());
    }

    private void assertValidRequire(String test, String expected) {
        List<String> modules = LuaScanner.scan(test);
        assertTrue(modules.size() == 1);
        assertEquals(expected, modules.get(0));
    }
    
    @Test
    public void testScanner() throws Exception {
        String file = getFile("test_scanner.lua");
        List<String> modules = LuaScanner.scan(file);

        assertEquals("a", modules.get(0));
        assertEquals("a", modules.get(1));
        assertEquals("a.b", modules.get(2));
        assertEquals("a.b", modules.get(3));
        assertEquals("a", modules.get(4));
        assertEquals("a", modules.get(5));
        assertEquals("a.b", modules.get(6));
        assertEquals("a.b", modules.get(7));
        assertEquals("a.b.c", modules.get(8));
        assertEquals("a.b.c", modules.get(9));
        assertEquals("foo1", modules.get(10));
        assertEquals("foo2", modules.get(11));
        assertEquals("foo3", modules.get(12));
        assertEquals("foo4", modules.get(13));
        assertEquals("foo5", modules.get(14));
        assertEquals("foo6", modules.get(15));
        assertEquals("foo7", modules.get(16));
        assertEquals("foo8", modules.get(17));
        assertEquals("foo9", modules.get(18));
        assertEquals("foo10", modules.get(19));
        assertEquals("foo11", modules.get(20));
        assertEquals("foo12", modules.get(21));
        assertEquals("foo13", modules.get(22));
        
        // test require detection with a trailing comment
        assertValidRequire("require \"foo.bar\" -- some comment", "foo.bar");
        assertValidRequire("require 'foo.bar' -- some comment", "foo.bar");
        assertValidRequire("require (\"foo.bar\") -- some comment", "foo.bar");
        assertValidRequire("require ('foo.bar') -- some comment", "foo.bar");

        // test require detection with a trailing empty comment
        assertValidRequire("require \"foo.bar\" --", "foo.bar");
        assertValidRequire("require 'foo.bar' --", "foo.bar");
        assertValidRequire("require (\"foo.bar\") --", "foo.bar");
        assertValidRequire("require ('foo.bar') --", "foo.bar");

        // test require detection with a trailing multi-line comment
        assertValidRequire("require \"foo.bar\" --[[ some comment]]--", "foo.bar");
        assertValidRequire("require 'foo.bar' --[[ some comment]]--", "foo.bar");
        assertValidRequire("require (\"foo.bar\") --[[ some comment]]--", "foo.bar");
        assertValidRequire("require ('foo.bar') --[[ some comment]]--", "foo.bar");
    }

    private Property findProperty(List<Property> properties, String name) {
        for (Property p : properties) {
            if (p.name != null && p.name.equals(name)) {
                return p;
            }
        }
        return null;
    }

    private void assertProperty(List<Property> properties, String name, Object value, int line) {
        Property property = findProperty(properties, name);
        if (property != null && property.status == Status.OK && property.name.equals(name)) {
            assertEquals(value, property.value);
            assertEquals(line, property.line);
            return;
        }
        assertTrue(false);
    }

    private void assertPropertyStatus(List<Property> properties, String name, Status status, int line) {
        Property property = findProperty(properties, name);
        assertTrue(property != null && property.status == status && property.line == line);
    }

    private List<Property> scanProperties(String path) throws IOException {
        String source = getFile(path);
        return LuaScanner.scanProperties(source);
    }

    @Test
    public void testProps() throws Exception {
        List<Property> properties = scanProperties("test_props.lua");

        assertEquals(7, properties.size());
        assertProperty(properties, "prop1", new Double(0), 10);
        assertProperty(properties, "prop2", new Double(0), 13);
        assertProperty(properties, "prop3", new Double(0), 14);
        assertProperty(properties, "prop4", new Double(0), 15);
        assertEquals(Status.INVALID_ARGS, properties.get(4).status);
        assertPropertyStatus(properties, "three_args", Status.INVALID_VALUE, 18);
        assertPropertyStatus(properties, "unknown_type", Status.INVALID_VALUE, 19);
    }

    @Test
    public void testPropsStripped() throws Exception {
        String source = getFile("test_props.lua");
        List<Property> properties = LuaScanner.scanProperties(source);
        assertEquals(7, properties.size());
        assertProperty(properties, "prop1", new Double(0), 10);
        assertProperty(properties, "prop2", new Double(0), 13);
        assertProperty(properties, "prop3", new Double(0), 14);
        assertProperty(properties, "prop4", new Double(0), 15);
        assertEquals(Status.INVALID_ARGS, properties.get(4).status);
        assertPropertyStatus(properties, "three_args", Status.INVALID_VALUE, 18);
        assertPropertyStatus(properties, "unknown_type", Status.INVALID_VALUE, 19);
        source = LuaScanner.stripProperties(source);
        properties = LuaScanner.scanProperties(source);
        assertEquals(0, properties.size());
    }

    @Test
    public void testPropsNumber() throws Exception {
        List<Property> properties = scanProperties("test_props_number.lua");

        assertEquals(4, properties.size());
        assertProperty(properties, "prop1", new Double(12), 0);
        assertProperty(properties, "prop2", new Double(1.0), 1);
        assertProperty(properties, "prop3", new Double(.1), 2);
        assertProperty(properties, "prop4", new Double(-1.0E2), 3);
    }

    @Test
    public void testPropsHash() throws Exception {
        List<Property> properties = scanProperties("test_props_hash.lua");

        assertEquals(4, properties.size());
        assertProperty(properties, "prop1", "hash", 0);
        assertProperty(properties, "prop2", "", 1);
        assertPropertyStatus(properties, "prop3", Status.INVALID_VALUE, 2);
        assertProperty(properties, "prop4", "hash", 3);
    }

    @Test
    public void testPropsUrl() throws Exception {
        List<Property> properties = scanProperties("test_props_url.lua");

        assertEquals(4, properties.size());
        assertProperty(properties, "prop1", "url", 0);
        assertProperty(properties, "prop2", "", 1);
        assertProperty(properties, "prop3", "", 2);
        assertProperty(properties, "prop4", "url", 3);
    }

    @Test
    public void testPropsVec3() throws Exception {
        List<Property> properties = scanProperties("test_props_vec3.lua");

        assertEquals(3, properties.size());
        assertProperty(properties, "prop1", new Vector3d(), 0);
        assertProperty(properties, "prop2", new Vector3d(1, 2, 3), 1);
        assertPropertyStatus(properties, "prop3", Status.INVALID_VALUE, 2);
    }

    @Test
    public void testPropsVec4() throws Exception {
        List<Property> properties = scanProperties("test_props_vec4.lua");

        assertEquals(3, properties.size());
        assertProperty(properties, "prop1", new Vector4d(), 0);
        assertProperty(properties, "prop2", new Vector4d(1, 2, 3, 4), 1);
        assertPropertyStatus(properties, "prop3", Status.INVALID_VALUE, 2);
    }

    @Test
    public void testPropsQuat() throws Exception {
        List<Property> properties = scanProperties("test_props_quat.lua");

        assertEquals(3, properties.size());
        assertProperty(properties, "prop1", new Quat4d(), 0);
        Quat4d q = new Quat4d();
        q.set(1, 2, 3, 4);
        assertProperty(properties, "prop2", q, 1);
        assertPropertyStatus(properties, "prop3", Status.INVALID_VALUE, 2);
    }

    @Test
    public void testPropsBool() throws Exception {
        List<Property> properties = scanProperties("test_props_bool.lua");

        assertEquals(2, properties.size());
        assertProperty(properties, "prop1", true, 0);
        assertProperty(properties, "prop2", false, 1);
    }

    @Test
    public void testPropsMaterial() throws Exception {
        List<Property> properties = scanProperties("test_props_material.lua");

        assertEquals(4, properties.size());
        assertProperty(properties, "prop1", "material", 0);
        assertProperty(properties, "prop2", "", 1);
        assertProperty(properties, "prop3", "", 2);
        assertProperty(properties, "prop4", "material", 3);
    }

}
