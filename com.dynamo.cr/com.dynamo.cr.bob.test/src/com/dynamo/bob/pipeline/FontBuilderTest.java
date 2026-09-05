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
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;

import java.io.ByteArrayInputStream;
import java.util.Collections;
import java.util.List;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Task;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.FontRenderer;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.font.proto.GlyphBankProto.FontTextureFormat;
import com.dynamo.font.proto.GlyphBankProto.GlyphBank;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontDesc;

import com.google.protobuf.Message;

public class FontBuilderTest extends AbstractProtoBuilderTest {

    static FontMap getFontMap(List<Message> buildResults) {
        for (Message m : buildResults) {
            if (m instanceof FontMap) {
                return (FontMap) m;
            }
        }

        return null;
    }

    @Before
    public void setup() {
        addTestFiles();

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("tags: \"text\"\n");
        src.append("vertex_program: \"/test.vp\"\n");
        src.append("fragment_program: \"/test.fp\"\n");
        addFile("/test.material", src.toString());

        src = new StringBuilder();
        src.append("name: \"test_2_material\"\n");
        src.append("tags: \"test_2\"\n");
        src.append("vertex_program: \"/test2.vp\"\n");
        src.append("fragment_program: \"/test2.fp\"\n");
        addFile("/test2.material", src.toString());
    }

    @Test
    public void testAuthoredStylesAreCompiled() throws Exception {
        String source = "font: \"/Tuffy.ttf\"\nmaterial: \"/test.material\"\nsize: 16\n"
                + "outline_width: 1.375\noutline_alpha: 0.3725\nshadow_alpha: 0.6235\n"
                + "shadow_x: 2.125\nshadow_y: -1.625\nshadow_blur: 0\n"
                + "styles { name: \"default\" }\n"
                + "styles { name: \"alert\" markup: \"<color=#123456><ul><strike><wave amplitude=2><shake amplitude=1>\" }\n";
        FontDesc.Builder description = FontDesc.newBuilder();
        com.google.protobuf.TextFormat.merge(source, description);
        FontMap compiled = getFontMap(build("/styles.font", source));
        assertEquals(com.dynamo.bob.font.FontStyles.compileStyles(description.build()), compiled.getStylesList());
        assertEquals(2, compiled.getStylesCount());
        com.dynamo.render.proto.Font.CompiledStyle defaults = compiled.getStyles(0);
        assertEquals(1.375f, defaults.getOutlineWidth(), 0.0f);
        assertEquals(0.3725f, defaults.getOutlineAlpha(), 0.0f);
        assertEquals(0.6235f, defaults.getShadowAlpha(), 0.0f);
        assertEquals(-1.625f, defaults.getShadowY(), 0.0f);
        assertEquals(0, defaults.getFlags() & ((1 << 2) | (1 << 4)));
        assertEquals(3, compiled.getStyles(1).getDecorationFlags());
        assertEquals(com.dynamo.render.proto.Font.StyleEffect.Type.WAVE, compiled.getStyles(1).getEffects(0).getType());
        assertEquals(com.dynamo.render.proto.Font.StyleEffect.Type.SHAKE, compiled.getStyles(1).getEffects(1).getType());
    }

    @Test
    public void testStyleMigrationAndDeletion() throws Exception {
        FontDesc.Builder font = FontDesc.newBuilder().setFont("/Tuffy.ttf").setMaterial("/test.material").setSize(16);
        List<com.dynamo.render.proto.Font.CompiledStyle> styles = com.dynamo.bob.font.FontStyles.compileStyles(font.build());
        assertEquals(4, styles.size());
        assertEquals("default", styles.get(0).getName());
        assertEquals("link", styles.get(1).getName());
        assertEquals(1, styles.get(1).getDecorationFlags());
        assertEquals(0, styles.get(1).getUnderlinePattern());
        font.addStyles(com.dynamo.render.proto.Font.StyleDesc.newBuilder().setName("default"));
        assertEquals(1, com.dynamo.bob.font.FontStyles.compileStyles(font.build()).size());
        for (String markup : new String[] { "text", "<size=24>", "<link>", "<style=other>", "<color=#fff></color>", "<sprite id=icon>", "<wave unknown=1>" }) {
            font.addStyles(com.dynamo.render.proto.Font.StyleDesc.newBuilder().setName("invalid").setMarkup(markup));
            try {
                com.dynamo.bob.font.FontStyles.compileStyles(font.build());
                fail("Accepted invalid style: " + markup);
            } catch (IllegalArgumentException expected) {
                assertTrue(expected.getMessage().contains("invalid"));
            }
            font.removeStyles(1);
        }
    }

    @Test
    public void testLabelAndGuiStyleSelection() throws Exception {
        ParseUtil.addParser("labelc", com.dynamo.gamesys.proto.Label.LabelDesc::parseFrom);
        addFile("/styles.font", "font: \"/Tuffy.ttf\"\nmaterial: \"/test.material\"\nsize: 16\nstyles { name: \"default\" }\nstyles { name: \"notice\" markup: \"<ul>\" }\n");
        for (String style : new String[] { null, "default", "", "notice" }) {
            String label = "size { x: 100 y: 32 }\nfont: \"/styles.font\"\nmaterial: \"/test.material\"\ntext: \"A\"\n"
                    + (style == null ? "" : "style: \"" + style + "\"\n");
            String expectedStyle = style == null ? "default" : style;
            boolean found = false;
            for (Message result : build("/selection.label", label)) {
                if (result instanceof com.dynamo.gamesys.proto.Label.LabelDesc) {
                    com.dynamo.gamesys.proto.Label.LabelDesc compiled = (com.dynamo.gamesys.proto.Label.LabelDesc)result;
                    assertEquals(expectedStyle, compiled.getStyle());
                    assertEquals(expectedStyle.isEmpty() ? 0 : MurmurHash.hash64(expectedStyle), compiled.getStyleHash());
                    found = true;
                }
            }
            assertTrue(found);
        }
        try {
            build("/missing.label", "size { x: 100 y: 32 }\nfont: \"/styles.font\"\nmaterial: \"/test.material\"\ntext: \"A\"\nstyle: \"missing\"\n");
            fail("Accepted a missing Label style");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage().contains("missing"));
        }
        String gui = "material: \"/test.material\"\nfonts { name: \"font\" font: \"/styles.font\" }\nnodes { type: TYPE_TEXT id: \"text\" font: \"font\" text: \"A\" style: \"notice\" }\n";
        build("/selection.gui", gui);
        try {
            build("/missing.gui", gui.replace("style: \"notice\"", "style: \"missing\""));
            fail("Accepted a missing GUI style");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage().contains("missing"));
        }
    }

    @Test
    public void testTTF() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test
    public void testLegacyGlyphBankWireFormat() throws Exception {
        // Produced by the former com.dynamo.render.proto.Font.GlyphBank schema.
        byte[] bitmapGlyphBank = new byte[] { 0x50, 0x00 };
        byte[] distanceFieldGlyphBank = new byte[] { 0x50, 0x01 };

        assertEquals(FontTextureFormat.TYPE_BITMAP, GlyphBank.parseFrom(bitmapGlyphBank).getImageFormat());
        assertEquals(FontTextureFormat.TYPE_DISTANCE_FIELD, GlyphBank.parseFrom(distanceFieldGlyphBank).getImageFormat());
        byte[] unsignedCacheAscent = new byte[] { (byte)0x90, 0x01, 0x66 };
        assertEquals(102, GlyphBank.parseFrom(unsignedCacheAscent).getCacheCellMaxAscent());
    }

    @Test(timeout = 3000)
    public void testTTFAllCharsBuildPerformance() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");
        src.append("all_chars: true\n");

        List<Message> buildResults = build("/all-chars.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = null;
        for (Message message : buildResults) {
            if (message instanceof GlyphBank) {
                glyphBank = (GlyphBank)message;
                break;
            }
        }

        assertTrue(fontMap != null);
        assertTrue(fontMap.getAllChars());
        assertTrue(fontMap.getGlyphBank().endsWith(".glyph_bankc"));
        assertTrue(glyphBank != null);
        assertEquals(1499, glyphBank.getGlyphsCount());
    }

    @Test
    public void testRuntimeGeneratedOTF() throws Exception {
        getProject().setOption("font-runtime-generation", "true");
        addFile("/Test.otf", getFile("/Tuffy.ttf"));

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Test.otf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");

        addFile("/test.font", src.toString());
        getProject().setInputs(Collections.singletonList("/test.font"));
        List<TaskResult> results = getProject().build(Progress.discarding(), "build");
        assertTrue(results.stream().allMatch(TaskResult::isOk));

        FontMap fontMap = null;
        for (TaskResult result : results) {
            for (IResource output : result.getTask().getOutputs()) {
                if (output.getPath().endsWith(".fontc")) {
                    fontMap = FontMap.parseFrom(output.getContent());
                }
            }
        }
        assertTrue(fontMap != null);
        assertEquals("/Test.otf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
    }

    @Test
    public void testFNT() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test
    public void testFNTWithNegativeCacheAscent() throws Exception {
        addFile("/negative.fnt", "info face=\"Negative\" size=10 bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1\n"
                + "common lineHeight=10 base=10 scaleW=16 scaleH=16 pages=1 packed=0\n"
                + "page id=0 file=\"bmfont.png\"\n"
                + "chars count=1\n"
                + "char id=95 x=1 y=1 width=2 height=3 xoffset=0 yoffset=13 xadvance=3 page=0 chnl=15\n");
        List<Message> results = build("/negative.font", "font: \"/negative.fnt\"\nmaterial: \"/test.material\"\nsize: 10\n");
        GlyphBank glyphBank = null;
        for (Message message : results) {
            if (message instanceof GlyphBank)
                glyphBank = (GlyphBank)message;
        }
        assertTrue(glyphBank != null);
        assertEquals(-3, glyphBank.getCacheCellMaxAscent());
        assertEquals(-3, GlyphBank.parseFrom(glyphBank.toByteArray()).getCacheCellMaxAscent());
        assertEquals(5, glyphBank.getCacheCellHeight());
        assertEquals(-3.0f, glyphBank.getMaxAscent(), 0.0f);
        assertEquals(6.0f, glyphBank.getMaxDescent(), 0.0f);
        GlyphBank serializedGlyphBank = GlyphBank.parseFrom(glyphBank.toByteArray());
        assertEquals(3.0f, serializedGlyphBank.getMaxAscent() + serializedGlyphBank.getMaxDescent(), 0.0f);
    }

    @Test
    public void testFNTWithGlyphsAboveBaseline() throws Exception {
        addFile("/above.fnt", "info face=\"Above\" size=10 bold=0 italic=0 charset=\"\" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1\n"
                + "common lineHeight=10 base=10 scaleW=16 scaleH=16 pages=1 packed=0\n"
                + "page id=0 file=\"bmfont.png\"\n"
                + "chars count=1\n"
                + "char id=65 x=1 y=1 width=2 height=2 xoffset=0 yoffset=1 xadvance=3 page=0 chnl=15\n");
        List<Message> results = build("/above.font", "font: \"/above.fnt\"\nmaterial: \"/test.material\"\nsize: 10\n");
        GlyphBank glyphBank = null;
        for (Message message : results) {
            if (message instanceof GlyphBank)
                glyphBank = GlyphBank.parseFrom(message.toByteArray());
        }
        assertTrue(glyphBank != null);
        assertEquals(9.0f, glyphBank.getMaxAscent(), 0.0f);
        assertEquals(0.0f, glyphBank.getMaxDescent(), 0.0f);
        assertEquals(-7, glyphBank.getGlyphs(0).getDescent());
        assertEquals(4, glyphBank.getCacheCellHeight());

        FontDesc desc = FontDesc.newBuilder().setFont("/above.fnt").setMaterial("/test.material").setSize(10).build();
        try (ByteArrayInputStream input = new ByteArrayInputStream(getProject().getResource("/above.fnt").getContent());
             ByteArrayInputStream bitmap = new ByteArrayInputStream(getProject().getResource("/bmfont.png").getContent())) {
            GlyphBank previewBank = new Fontc().compileForEditor(input, desc, "bmfont.png", bitmap).glyphBank;
            GlyphBank.Glyph glyph = previewBank.getGlyphs(0);
            FontRenderer.GlyphBankGlyph[] glyphs = {
                new FontRenderer.GlyphBankGlyph(glyph.getCharacter(), glyph.getWidth(), glyph.getAdvance(), glyph.getLeftBearing(),
                        glyph.getAscent(), glyph.getDescent(), (int)glyph.getGlyphDataOffset(), (int)glyph.getGlyphDataSize())
            };
            FontRenderer.GlyphBank nativeBank = new FontRenderer.GlyphBank(glyphs, previewBank.getGlyphData().toByteArray(),
                    (int)previewBank.getGlyphPadding(), previewBank.getGlyphChannels(), previewBank.getMaxAscent(), previewBank.getMaxDescent());
            FontRenderer.Params params = new FontRenderer.Params();
            params.size = 10.0f;
            params.cacheWidth = 16;
            params.cacheHeight = 16;
            try (FontRenderer renderer = new FontRenderer("above.fnt", nativeBank, params)) {
                assertEquals(9.0f, renderer.measure("A", false, 0.0f, 1.0f, 0.0f).height, 0.001f);
                FontRenderer.Properties properties = new FontRenderer.Properties();
                properties.height = 16.0f;
                properties.leading = 1.0f;
                properties.faceColor = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
                properties.outlineColor = properties.faceColor;
                properties.shadowColor = properties.faceColor;
                properties.sdfScale = 1.0f;
                renderer.setProperties(properties);
                renderer.setText("A");
                renderer.beginBatch();
                assertTrue(renderer.generateTexture(0).pixels != null);
            }
        }
    }

    @Test
    public void testInvalidFNTReportsCompileException() throws Exception {
        addFile("/invalid.fnt", "invalid");

        StringBuilder src = new StringBuilder();
        src.append("font: \"/invalid.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        addFile("/invalid.font", src.toString());

        Task task = getProject().createTask(getProject().getResource("/invalid.font"), GlyphBankBuilder.class);
        try {
            task.getBuilder().build(task);
            fail("Expected malformed BMFont data to produce a CompileExceptionError");
        } catch (CompileExceptionError e) {
            assertEquals("invalid.font", e.getResource().getPath());
            assertTrue(e.getCause() instanceof BMFontFormatException);
        }
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
        FontMap fontMap = getFontMap(build("/subdir/test.font", src.toString()));

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
    }

    @Test
    public void testFNTGlpyBankPath() throws Exception {

        StringBuilder srcOne = new StringBuilder();
        srcOne.append("font: \"/bmfont.fnt\"\n");
        srcOne.append("material: \"/test.material\"\n");
        srcOne.append("size: 16\n");

        StringBuilder srcTwo = new StringBuilder();
        srcTwo.append("font: \"/bmfont.fnt\"\n");
        srcTwo.append("material: \"/test2.material\"\n");
        srcTwo.append("size: 16\n");

        StringBuilder srcThree = new StringBuilder();
        srcThree.append("font: \"/bmfont.fnt\"\n");
        srcThree.append("material: \"/test2.material\"\n");
        srcThree.append("size: 16\n");
        srcThree.append("shadow_x: 1337.0\n");

        FontMap fontMapOne   = getFontMap(build("/test1.font", srcOne.toString()));
        FontMap fontMapTwo   = getFontMap(build("/test2.font", srcTwo.toString()));
        FontMap fontMapThree = getFontMap(build("/test3.font", srcThree.toString()));

        assertEquals(fontMapOne.getGlyphBank(), fontMapTwo.getGlyphBank());
        assertEquals(fontMapOne.getGlyphBank(), fontMapThree.getGlyphBank());

        assertNotEquals(fontMapOne.getMaterial(), fontMapTwo.getMaterial());
        assertNotEquals(fontMapOne.getShadowX(), fontMapThree.getShadowX());
    }
}
