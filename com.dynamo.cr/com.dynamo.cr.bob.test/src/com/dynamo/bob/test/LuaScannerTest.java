package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

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
}
