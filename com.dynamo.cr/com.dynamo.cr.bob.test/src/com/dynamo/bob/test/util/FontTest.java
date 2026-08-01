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
import static org.junit.Assert.assertFalse;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;

import org.apache.commons.io.IOUtils;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.font.BMFont;
import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.BMFont.ChannelData;
import com.dynamo.bob.font.BMFont.Char;
import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.EditorFontMap;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.GlyphBank;
import com.dynamo.render.proto.Font.GlyphBank.Glyph;
import com.dynamo.render.proto.Font.FontTextureFormat;

public class FontTest {

    private static final double EPSILON = 0.000001;

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private String copyResourceToDir(String tmpDir, String resName) throws IOException {
        String outputPath = Paths.get(tmpDir, resName).toString();

        InputStream inputStream = getClass().getResourceAsStream(resName);
        File outputFile = new File(outputPath);
        OutputStream outputStream = new FileOutputStream(outputFile);
        IOUtils.copy(inputStream, outputStream);
        inputStream.close();
        outputStream.close();

        return outputPath;
    }

    private static void assertBMFontFormatException( BMFont bmFont, InputStream in ) throws Exception {

        boolean success = true;
        try {
            bmFont.parse(in);
        } catch (BMFontFormatException e) {
            success = false;
        }

        assertFalse(success);
    }

    private static void assertEntryParse(HashMap<String,String> expected, String entries) throws Exception {
        HashMap<String,String> res = BMFont.splitEntries( entries );

        for (HashMap.Entry<String,String> expectedEntry : expected.entrySet()) {
            String key = expectedEntry.getKey();
            String expectedValue = expectedEntry.getValue();
            assertTrue(res.containsKey(key));

            String actualValue = res.get(key);
            assertEquals( expectedValue, actualValue );
        }

        for (HashMap.Entry<String,String> resEntry : res.entrySet()) {
            String key = resEntry.getKey();
            String resValue = resEntry.getValue();
            assertTrue(expected.containsKey(key));

            String actualValue = expected.get(key);
            assertEquals( resValue, actualValue );
        }
    }

    @Test
    public void testBMFontEntryParse() throws Exception {

        HashMap<String,String> entries = new HashMap<String,String>();
        entries.clear();
        entries.put("a", "32");
        entries.put("bcd", "0");
        entries.put("efg", "wut");
        entries.put("string", "farbror melker");
        entries.put("array", "0,0,0,0");
        assertEntryParse(entries, "a=32 bcd=0 efg=wut string=\"farbror melker\" array=0,0,0,0");

        entries.clear();
        entries.put("string", "båtsman i saltkråkan");
        assertEntryParse(entries, "string=\"båtsman i saltkråkan\"");

        entries.clear();
        entries.put("string", "   lofty     strings ");
        assertEntryParse(entries, "    string=\"   lofty     strings \"       ");

        entries.clear();
        entries.put("a", "b");
        entries.put("c", "d");
        entries.put("apa", "bepa cepa   ");
        assertEntryParse(entries, "  a=b c=d apa=\"bepa cepa   \"");

        entries.clear();
        entries.put("bcd", "0");
        assertEntryParse(entries, "a= bcd=0 asd aasd");

        assertBMFontFormatException(new BMFont(), new ByteArrayInputStream("info a=\"asdasdas".getBytes(StandardCharsets.UTF_8)));

    }

    @Test
    public void testBMFontInvalidFormat() throws Exception {
        BMFont bmfont = new BMFont();
        InputStream input = getClass().getResourceAsStream("invalid1.fnt");
        assertBMFontFormatException(bmfont, input);

        input = getClass().getResourceAsStream("invalid2.fnt");
        assertBMFontFormatException(bmfont, input);

        input = getClass().getResourceAsStream("invalid3.fnt");
        assertBMFontFormatException(bmfont, input);
    }

    @Test
    public void testBMFont() throws Exception {

        InputStream input = getClass().getResourceAsStream("bmfont.fnt");
        BMFont bmfont = new BMFont();
        bmfont.parse(input);

        // verify font info
        assertEquals(32, bmfont.size, EPSILON);
        assertEquals(false, bmfont.bold);
        assertEquals(false, bmfont.italics);
        assertEquals("", bmfont.charset);
        assertEquals(false, bmfont.unicode);
        assertEquals(100, bmfont.stretchH, EPSILON);
        assertEquals(false, bmfont.smooth);
        assertEquals(1, bmfont.aa);
        assertEquals(4, bmfont.padding.length);
        assertEquals(0, bmfont.padding[0], EPSILON);
        assertEquals(0, bmfont.padding[1], EPSILON);
        assertEquals(0, bmfont.padding[2], EPSILON);
        assertEquals(0, bmfont.padding[3], EPSILON);
        assertEquals(2, bmfont.spacing.length);
        assertEquals(1, bmfont.spacing[0], EPSILON);
        assertEquals(1, bmfont.spacing[1], EPSILON);


        // verify font common
        assertEquals(80, bmfont.lineHeight);
        assertEquals(26, bmfont.base);
        assertEquals(512, bmfont.scaleW);
        assertEquals(1024, bmfont.scaleH);
        assertEquals(1, bmfont.pages);
        assertEquals(false, bmfont.packed);
        assertEquals(ChannelData.OUTLINE, bmfont.alphaChnl);
        assertEquals(ChannelData.GLYPH, bmfont.redChnl);
        assertEquals(ChannelData.GLYPH, bmfont.greenChnl);
        assertEquals(ChannelData.GLYPH, bmfont.blueChnl);

        // verify chars
        assertEquals(96, bmfont.chars);
        assertEquals(96, bmfont.charArray.size());

        for (int i = 0; i < bmfont.chars; i++)
        {
            Char c = bmfont.charArray.get(i);
            assertTrue( (c.id <= 126 && c.id >= 32) || c.id == 9 );
        }

    }

    @Test
    public void testBMFontUnsortedGlyphs() throws Exception {

        InputStream input = getClass().getResourceAsStream("bmfont_unsorted.fnt");
        BMFont bmfont = new BMFont();
        bmfont.parse(input);

        assertEquals(96, bmfont.chars);
        assertEquals(96, bmfont.charArray.size());

        // verify that the characters have been sorted
        int previousId = -1;
        for (int i = 0; i < bmfont.chars; i++)
        {
            Char c = bmfont.charArray.get(i);
            assertTrue( c.id > previousId);
            previousId = c.id;
        }
    }

    @Test
    public void testTTF() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setExtraCharacters("åäöÅÄÖ")
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("glyph-bank-output.glyph_bankc");

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        fontc.compile(fontInputStream, fontDesc, false);

        GlyphBank glyphBank = fontc.getGlyphBank();
        glyphBank.writeTo(fontOutputStream);

        fontInputStream.close();
        fontOutputStream.close();

        // verify output
        BufferedInputStream glyphBankCStream = new BufferedInputStream(new FileInputStream(outfile));
        glyphBank = GlyphBank.newBuilder().mergeFrom(glyphBankCStream).build();

        // glyph count
        int expectedCharCount = (127 - 32) + 7; // (127 - 32) default chars, 7 extra åäöÅÄÖ
        assertEquals(expectedCharCount, glyphBank.getGlyphsCount());

        // unicode chars
        assertEquals(0xF8FF, glyphBank.getGlyphs(glyphBank.getGlyphsCount() - 1).getCharacter());
    }

    @Test
    public void testCompileForEditorReturnsUncompressedFontData() throws Exception {
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setCharacters("Ag ")
            .build();

        EditorFontMap editorFontMap;
        try (InputStream input = getClass().getResourceAsStream(fontDesc.getFont())) {
            editorFontMap = new Fontc().compileForEditor(input, fontDesc, null, null);
        }

        FontMap fontMap = editorFontMap.fontMap;
        GlyphBank glyphBank = editorFontMap.glyphBank;
        assertEquals("/font.materialc", fontMap.getMaterial());
        assertEquals(fontDesc.getCharacters(), fontMap.getCharacters());
        assertEquals(glyphBank.getCacheWidth(), fontMap.getCacheWidth());
        assertEquals(glyphBank.getCacheHeight(), fontMap.getCacheHeight());
        assertEquals(glyphBank.getPadding(), fontMap.getPadding());
        assertEquals(glyphBank.getGlyphsCount(), editorFontMap.glyphCellWidths.length);
        assertEquals(glyphBank.getGlyphsCount(), editorFontMap.glyphCellHeights.length);

        long expectedOffset = 0;
        int maxAscent = 0;
        int maxDescent = 0;
        for (int i = 0; i < glyphBank.getGlyphsCount(); ++i) {
            Glyph glyph = glyphBank.getGlyphs(i);
            long expectedSize = (long)editorFontMap.glyphCellWidths[i]
                              * editorFontMap.glyphCellHeights[i]
                              * glyphBank.getGlyphChannels();
            assertEquals(expectedOffset, glyph.getGlyphDataOffset());
            assertEquals(expectedSize, glyph.getGlyphDataSize());
            expectedOffset += expectedSize;
            maxAscent = Math.max(maxAscent, glyph.getAscent());
            maxDescent = Math.max(maxDescent, glyph.getDescent());
        }
        assertEquals(expectedOffset, glyphBank.getGlyphData().size());
        assertEquals(maxAscent + maxDescent + 2 * glyphBank.getGlyphPadding(), glyphBank.getCacheCellHeight());
    }

    @Test
    public void testCompileForEditorBuildReturnsCompressedGlyphData() throws Exception {
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setCharacters("Ag ")
            .build();

        GlyphBank glyphBank;
        try (InputStream input = getClass().getResourceAsStream(fontDesc.getFont())) {
            glyphBank = new Fontc().compileForEditorBuild(input, fontDesc, null, null);
        }

        long expectedOffset = 0;
        for (Glyph glyph : glyphBank.getGlyphsList()) {
            assertEquals(expectedOffset, glyph.getGlyphDataOffset());
            if (glyph.getGlyphDataSize() > 0) {
                byte compressionHeader = glyphBank.getGlyphData().byteAt((int)expectedOffset);
                assertTrue(compressionHeader == 0 || compressionHeader == 1);
            }
            expectedOffset += glyph.getGlyphDataSize();
        }
        assertEquals(expectedOffset, glyphBank.getGlyphData().size());
    }

    @Test
    public void testCompileForEditorDistanceFieldReturnsMetadataOnly() throws Exception {
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setCharacters("Ag ")
            .setOutputFormat(FontTextureFormat.TYPE_DISTANCE_FIELD)
            .build();

        EditorFontMap editorFontMap;
        try (InputStream input = getClass().getResourceAsStream(fontDesc.getFont())) {
            editorFontMap = new Fontc().compileForEditor(input, fontDesc, null, null);
        }
        Fontc fontc = new Fontc();
        try (InputStream input = getClass().getResourceAsStream(fontDesc.getFont())) {
            fontc.compile(input, fontDesc, false);
        }

        GlyphBank glyphBank = editorFontMap.glyphBank;
        GlyphBank compiledGlyphBank = fontc.getGlyphBank();
        assertEquals(0, glyphBank.getGlyphData().size());
        assertEquals(compiledGlyphBank.getCacheWidth(), glyphBank.getCacheWidth());
        assertEquals(compiledGlyphBank.getCacheHeight(), glyphBank.getCacheHeight());
        assertEquals(compiledGlyphBank.getCacheCellWidth(), glyphBank.getCacheCellWidth());
        assertEquals(compiledGlyphBank.getCacheCellHeight(), glyphBank.getCacheCellHeight());
        assertEquals(compiledGlyphBank.getGlyphsCount(), glyphBank.getGlyphsCount());
        for (int i = 0; i < glyphBank.getGlyphsCount(); ++i) {
            Glyph glyph = glyphBank.getGlyphs(i);
            Glyph compiledGlyph = compiledGlyphBank.getGlyphs(i);
            assertEquals(compiledGlyph.getCharacter(), glyph.getCharacter());
            assertEquals(compiledGlyph.getWidth(), glyph.getWidth(), 0.0f);
            assertEquals(compiledGlyph.getAdvance(), glyph.getAdvance(), 0.0f);
            assertEquals(compiledGlyph.getLeftBearing(), glyph.getLeftBearing(), 0.0f);
            assertEquals(compiledGlyph.getAscent(), glyph.getAscent());
            assertEquals(compiledGlyph.getDescent(), glyph.getDescent());
            assertEquals(0, glyph.getGlyphDataOffset());
            assertEquals(0, glyph.getGlyphDataSize());
        }
    }

    @Test
    public void testNativeDistanceFieldSingleChannelGlyphBank() throws Exception {
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setCharacters("A")
            .setOutputFormat(FontTextureFormat.TYPE_DISTANCE_FIELD)
            .build();

        Fontc fontc = new Fontc();
        try (InputStream input = getClass().getResourceAsStream(fontDesc.getFont())) {
            fontc.compile(input, fontDesc, false);
        }

        GlyphBank glyphBank = fontc.getGlyphBank();
        assertEquals(1, glyphBank.getGlyphChannels());
        assertEquals(1, glyphBank.getGlyphsCount());
        assertEquals(Fontc.GetFontMapPadding(fontDesc), glyphBank.getPadding());
        assertEquals(Fontc.GetFontMapSdfSpread(fontDesc), glyphBank.getSdfSpread(), EPSILON);
        assertEquals(Fontc.GetFontMapSdfOutline(fontDesc), glyphBank.getSdfOutline(), EPSILON);
        assertEquals(Fontc.GetFontMapSdfShadow(fontDesc), glyphBank.getSdfShadow(), EPSILON);
        assertTrue(glyphBank.getGlyphs(0).getGlyphDataSize() > 1);
        assertTrue(glyphBank.getGlyphs(0).getWidth() > 0.0f);
        assertTrue(glyphBank.getGlyphs(0).getAscent() + glyphBank.getGlyphs(0).getDescent() > 0);
    }

    @Test
    public void testTTFUnsortedCharacters() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setCharacters("szktg6TDJ0eN$SM1ZGaIldyRjCxQWv42B7mUOV3Kfhb9cXEu5r8PYFALqHnwo!ip")
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("glyph-bank-output.glyph_bankc");

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        fontc.compile(fontInputStream, fontDesc, false);

        GlyphBank glyphBank = fontc.getGlyphBank();
        glyphBank.writeTo(fontOutputStream);

        fontInputStream.close();
        fontOutputStream.close();

        // verify output
        BufferedInputStream glyphBankCStream = new BufferedInputStream(new FileInputStream(outfile));
        glyphBank = GlyphBank.newBuilder().mergeFrom(glyphBankCStream).build();

        String actual = "";
        for (int i=0; i < glyphBank.getGlyphsCount(); i++)
        {
            actual += new String(Character.toChars(glyphBank.getGlyphs(i).getCharacter()));
        }
        assertEquals(actual, "!$0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    }

    @Test
    public void testTTFJapaneseAllChars() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("DroidSansJapanese.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setAllChars(true)
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("glyph-bank-output.glyph_bankc");

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        fontc.compile(fontInputStream, fontDesc, false);
        GlyphBank glyphBank = fontc.getGlyphBank();
        glyphBank.writeTo(fontOutputStream);

        fontInputStream.close();
        fontOutputStream.close();

        // verify output
        BufferedInputStream glyphBankCStream = new BufferedInputStream(new FileInputStream(outfile));
        glyphBank = GlyphBank.newBuilder().mergeFrom(glyphBankCStream).build();

        // glyph count
        // Native stb_truetype glyph count after filtering missing and zero-width glyphs.
        int expectedCharCount = 6619;
        assertEquals(expectedCharCount, glyphBank.getGlyphsCount());
    }

    @Test
    public void testTTFAllChars() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(24)
            .setAllChars(true)
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("glyph-bank-output.glyph_bankc");

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        fontc.compile(fontInputStream, fontDesc, false);
        GlyphBank glyphBank = fontc.getGlyphBank();
        glyphBank.writeTo(fontOutputStream);

        fontInputStream.close();
        fontOutputStream.close();

        // verify output
        BufferedInputStream glyphBankCStream = new BufferedInputStream(new FileInputStream(outfile));
        glyphBank = GlyphBank.newBuilder().mergeFrom(glyphBankCStream).build();

        // Native stb_truetype glyph count after filtering missing and zero-width glyphs.
        int expectedCharCount = 1499;
        assertEquals(expectedCharCount, glyphBank.getGlyphsCount());
    }

    @Test
    public void testTTFPreview() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("Tuffy.ttf")
            .setMaterial("font.material")
            .setSize(32)
            .setAllChars(true)
            .build();

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        fontc.compile(fontInputStream, fontDesc, true);
        GlyphBank glyphBank = fontc.getGlyphBank();

        fontInputStream.close();

        assertEquals(1024, glyphBank.getCacheWidth());
        assertEquals(2048, glyphBank.getCacheHeight());

        // For previews we don't include all glyphs
        assertTrue(glyphBank.getGlyphsCount() < 1519);

        // Check that all glyphs are inside cache space
        for (int i = 0; i < glyphBank.getGlyphsCount(); i++) {
            Glyph g = glyphBank.getGlyphs(i);
            assertTrue(g.getX() >= 0);
            assertTrue(g.getY() >= 0);
            assertTrue(g.getX() + g.getWidth() < glyphBank.getCacheWidth());
            assertTrue(g.getY() + g.getAscent() + g.getDescent() < glyphBank.getCacheHeight());
        }
    }

    @Test
    public void testBinaryFNT() throws Exception {

        FontDesc fontDesc = FontDesc.newBuilder()
                .setFont("binary.fnt")
                .setMaterial("font.material")
                .setSize(24)
                .build();

        File outfile = temporaryFolder.newFile("font-output.fontc");

        // compile font
        boolean success = true;
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        try {
            fontc.compile(fontInputStream, fontDesc, false);
            GlyphBank glyphBank = fontc.getGlyphBank();
            glyphBank.writeTo(fontOutputStream);
        } catch (IOException e) {
            success = false;
        }

        fontInputStream.close();
        fontOutputStream.close();


        // NOTE: We don't support binary BMFont files at the moment,
        //       make sure we threw a format exception above!
        assertFalse(success);
    }

    @Test
    public void testFNT() throws Exception {

        // copy fnt and texture file
        final Path tmpDir = temporaryFolder.newFolder("fnt-tmp").toPath();
        String tmpFnt = copyResourceToDir(tmpDir.toString(), "bmfont.fnt");
        copyResourceToDir(tmpDir.toString(), "bmfont.png");

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont(tmpFnt)
            .setMaterial("font.material")
            .setSize(24)
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("font-output.fontc");

        // compile font
        Fontc fontc = new Fontc();
        FileInputStream fontInputStream = new FileInputStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        try (InputStream bitmapStream = new FileInputStream(tmpDir.resolve("bmfont.png").toFile())) {
            fontc.compile(fontInputStream, fontDesc, false, "bmfont.png", bitmapStream);
        }
        GlyphBank glyphBank = fontc.getGlyphBank();
        glyphBank.writeTo(fontOutputStream);

        fontInputStream.close();
        fontOutputStream.close();

        // verify output
        BufferedInputStream glyphBankCStream = new BufferedInputStream(new FileInputStream(outfile));
        glyphBank = GlyphBank.newBuilder().mergeFrom(glyphBankCStream).build();

        int expectedCharCount = 96; // Taken from bmfont.fnt
        assertEquals(expectedCharCount, glyphBank.getGlyphsCount());
    }


    // https://github.com/defold/defold/issues/7346
    @Test
    public void testUncompressedChars() throws Exception {

        // create "font file"
        FontDesc fontDesc = FontDesc.newBuilder()
            .setFont("monogram.ttf")
            .setMaterial("font.material")
            .setSize(16)
            .setAllChars(false)
            .build();

        // temp output file
        File outfile = temporaryFolder.newFile("font-output.fontc");

        // compile font
        Fontc fontc = new Fontc();
        InputStream fontInputStream = getClass().getResourceAsStream(fontDesc.getFont());
        FileOutputStream fontOutputStream = new FileOutputStream(outfile);
        fontc.compile(fontInputStream, fontDesc, false);
        GlyphBank glyphBank = fontc.getGlyphBank();
        byte[] glyphData = glyphBank.getGlyphData().toByteArray();
        int glyphCount = glyphBank.getGlyphsCount();
        for (int i = 0; i < glyphCount; i++) {
            Glyph g = glyphBank.getGlyphs(i);
            if ((char)g.getCharacter() == '.') {
                int glyphDataSize = (int)g.getGlyphDataSize();
                int glyphDataOffset = (int)g.getGlyphDataOffset();
                assertTrue(glyphDataSize > 1);
                assertTrue(glyphData[glyphDataOffset] == 0 || glyphData[glyphDataOffset] == 1);
                return;
            }
        }
        // we should not get here unless the '.' glyph wasn't found
        assertTrue(false);
    }

}
