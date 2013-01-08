package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;
import java.util.Map;

import org.apache.commons.io.IOUtils;
import org.junit.Test;

import com.dynamo.bob.pipeline.LuaScanner;
import com.dynamo.bob.pipeline.LuaScanner.PropertyLine;

public class LuaScannerTest {

    String getFile(String file) throws IOException {
        InputStream input = getClass().getResourceAsStream(file);
        ByteArrayOutputStream output = new ByteArrayOutputStream(1024);
        IOUtils.copy(input, output);
        return new String(output.toByteArray());
    }

    @Test
    public void testScanner() throws Exception {
        String file = getFile("test_scanner.lua");
        List<String> modules = LuaScanner.scan(file);

        assertEquals("a", modules.get(0));
        assertEquals("a", modules.get(1));
        assertEquals("a/b", modules.get(2));
        assertEquals("a/b", modules.get(3));
        assertEquals("a/b/c", modules.get(4));
        assertEquals("a/b/c", modules.get(5));
    }

    private void assertPropertyLine(Map<String, PropertyLine> properties, String key, String value, int line) {
        PropertyLine propLine = properties.get(key);
        assertEquals(value, propLine.value);
        assertEquals(line, propLine.line);
    }

    @Test
    public void testScannerProps() throws Exception {
        String file = getFile("test_scanner_props.lua");
        Map<String, PropertyLine> properties = LuaScanner.scanProperties(file);

        assertEquals(7, properties.size());
        assertPropertyLine(properties, "number", "12", 2);
        assertPropertyLine(properties, "hash", "hash(\"hash\")", 3);
        assertPropertyLine(properties, "url", "msg.url()", 4);
        assertPropertyLine(properties, "vec3", "vmath.vector3(1, 2, 3)", 5);
        assertPropertyLine(properties, "vec4", "vmath.vector4(4, 5, 6, 7)", 6);
        assertPropertyLine(properties, "quat", "vmath.quat(8, 9, 10, 11)", 7);
        assertPropertyLine(properties, "space_num", "13", 9);
    }
}
