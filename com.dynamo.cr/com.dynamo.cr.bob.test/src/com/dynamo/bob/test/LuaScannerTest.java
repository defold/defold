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

    @Test
    public void testScannerProps() throws Exception {
        String file = getFile("test_scanner_props.lua");
        Map<String, String> properties = LuaScanner.scanProperties(file);

        assertEquals(7, properties.size());
        assertEquals("12", properties.get("number"));
        assertEquals("hash(\"hash\")", properties.get("hash"));
        assertEquals("msg.url()", properties.get("url"));
        assertEquals("vmath.vector3(1, 2, 3)", properties.get("vec3"));
        assertEquals("vmath.vector4(4, 5, 6, 7)", properties.get("vec4"));
        assertEquals("vmath.quat(8, 9, 10)", properties.get("quat"));
        assertEquals("12", properties.get("space_num"));
    }
}
