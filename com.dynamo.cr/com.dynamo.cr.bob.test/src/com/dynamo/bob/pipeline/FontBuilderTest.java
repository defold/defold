package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.render.proto.Font.FontMap;

public class FontBuilderTest extends AbstractProtoBuilderTest {

    @Before
    public void setup() {
        addTestFiles();

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("tags: \"text\"\n");
        src.append("vertex_program: \"/test.vp\"\n");
        src.append("fragment_program: \"/test.fp\"\n");
        addFile("/test.material", src.toString());
    }

    @Test
    public void testTTF() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");

        FontMap fontMap = (FontMap)build("/test.font", src.toString()).get(0);
        assertEquals(fontMap.getMaterial(), "/test.materialc");
    }

    @Test
    public void testFNT() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        FontMap fontMap = (FontMap)build("/test.font", src.toString()).get(0);

        assertEquals(fontMap.getMaterial(), "/test.materialc");

    }

    @Test
    public void testFNTSubDir() throws Exception {
        byte[] toff_file = getFile("/bmfont.png");
        assertTrue(toff_file != null);
        addFile("/subdir/bmfont.png", toff_file);

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        FontMap fontMap = (FontMap)build("/subdir/test.font", src.toString()).get(0);

        assertEquals(fontMap.getMaterial(), "/test.materialc");

    }
}
