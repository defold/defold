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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
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
import com.dynamo.font.proto.GlyphBankProto.GlyphBank;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.dynamo.render.proto.Font.VectorFontMode;

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

    static GlyphBank getGlyphBank(List<Message> buildResults) {
        for (Message message : buildResults) {
            if (message instanceof GlyphBank) {
                return (GlyphBank) message;
            }
        }
        return null;
    }

    @Before
    public void setup() {
        addTestFiles();
        ParseUtil.addParser("ttf", content -> null);
        ParseUtil.addParser("otf", content -> null);
        ParseUtil.addParser("labelc", content -> com.dynamo.gamesys.proto.Label.LabelDesc.parseFrom(content));

        StringBuilder src = new StringBuilder();
        src.append("name: \"test_material\"\n");
        src.append("tags: \"text\"\n");
        src.append("vertex_program: \"/test.vp\"\n");
        src.append("fragment_program: \"/test.fp\"\n");
        addFile("/test.material", src.toString());

        src.append("samplers { name: \"curve_texture\" wrap_u: WRAP_MODE_CLAMP_TO_EDGE wrap_v: WRAP_MODE_CLAMP_TO_EDGE filter_min: FILTER_MODE_MIN_DEFAULT filter_mag: FILTER_MODE_MAG_DEFAULT }\n");
        src.append("samplers { name: \"band_texture\" wrap_u: WRAP_MODE_CLAMP_TO_EDGE wrap_v: WRAP_MODE_CLAMP_TO_EDGE filter_min: FILTER_MODE_MIN_NEAREST filter_mag: FILTER_MODE_MAG_NEAREST }\n");
        addFile("/test-vector.material", src.toString());

        src = new StringBuilder();
        src.append("name: \"test_2_material\"\n");
        src.append("tags: \"test_2\"\n");
        src.append("vertex_program: \"/test2.vp\"\n");
        src.append("fragment_program: \"/test2.fp\"\n");
        addFile("/test2.material", src.toString());

        String runtimeMaterial = "name: \"runtime_font_material\"\n" +
                                 "tags: \"text\"\n" +
                                 "vertex_program: \"/test.vp\"\n" +
                                 "fragment_program: \"/test.fp\"\n";
        String vectorRuntimeMaterial = runtimeMaterial +
            "samplers { name: \"curve_texture\" wrap_u: WRAP_MODE_CLAMP_TO_EDGE wrap_v: WRAP_MODE_CLAMP_TO_EDGE filter_min: FILTER_MODE_MIN_DEFAULT filter_mag: FILTER_MODE_MAG_DEFAULT }\n";
        vectorRuntimeMaterial += "samplers { name: \"band_texture\" wrap_u: WRAP_MODE_CLAMP_TO_EDGE wrap_v: WRAP_MODE_CLAMP_TO_EDGE filter_min: FILTER_MODE_MIN_NEAREST filter_mag: FILTER_MODE_MAG_NEAREST }\n";
        addFile("/builtins/fonts/font-df.material", runtimeMaterial);
        addFile("/builtins/fonts/font-vector.material", vectorRuntimeMaterial);
        addFile("/builtins/fonts/label-vector.material", vectorRuntimeMaterial);
        addFile("/builtins/fonts/label-df.material", runtimeMaterial);
    }

    @Test
    public void testAuthoredStylesAreCompiled() throws Exception {
        String source = "font: \"/Tuffy.ttf\"\nmaterial: \"/test.material\"\nsize: 16\n"
                + "render_mode: MODE_MULTI_LAYER\n"
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
    public void testLegacyRenderModeDoesNotDisableDefaultStyleEffects() throws Exception {
        for (String outputFormat : new String[] { "TYPE_BITMAP", "TYPE_DISTANCE_FIELD" }) {
            for (String renderMode : new String[] { "", "render_mode: MODE_SINGLE_LAYER\n" }) {
                String source = "font: \"/Tuffy.ttf\"\nmaterial: \"/test.material\"\nsize: 16\ncharacters: \"A\"\n"
                        + "output_format: " + outputFormat + "\n" + renderMode
                        + "outline_width: 1.375\noutline_alpha: 0.3725\nshadow_alpha: 0.6235\n"
                        + "shadow_x: 2.125\nshadow_y: -1.625\nshadow_blur: 1\n";
                FontMap compiled = getFontMap(build("/single-layer.font", source));
                assertEquals(7, compiled.getLayerMask());
                assertTrue(compiled.getStyles(0).getFlags() != 0);
                assertEquals(1.375f, compiled.getStyles(0).getOutlineWidth(), 0.0f);
                assertEquals(0.3725f, compiled.getStyles(0).getOutlineAlpha(), 0.0f);
                assertEquals(0.6235f, compiled.getStyles(0).getShadowAlpha(), 0.0f);
                assertEquals(1.375f, compiled.getOutlineWidth(), 0.0f);
                assertEquals(1, compiled.getShadowBlur());
            }
        }
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
        src.append("material: \"/test-vector.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_BITMAP\n");
        src.append("render_mode: MODE_SINGLE_LAYER\n");
        src.append("characters: \"ABC\"\n");
        src.append("all_chars: true\n");
        src.append("shadow_blur: 3\n");
        src.append("shadow_alpha: 1\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("sdf_material: \"/test2.material\"\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test-vector.materialc"));
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertEquals(FontTextureFormat.TYPE_DISTANCE_FIELD, fontMap.getOutputFormat());
        assertEquals(FontRenderMode.MODE_MULTI_LAYER, fontMap.getRenderMode());
        assertEquals("ABC", fontMap.getCharacters());
        assertTrue(!fontMap.getAllChars());
        assertEquals("/Tuffy.ttf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
        assertEquals(16, fontMap.getSize());
        assertTrue(!fontMap.hasAntialias());
    }

    @Test
    public void testStaticVectorFontBakesCurvesWithoutCopyingSourceFont() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("runtime: false\n");
        src.append("characters: \"A\"\n");

        addFile("/static-vector.font", src.toString());
        getProject().setInputs(Collections.singletonList("/static-vector.font"));
        List<TaskResult> results = getProject().build(Progress.discarding(), "build");
        FontMap fontMap = null;
        GlyphBank glyphBank = null;
        for (TaskResult result : results) {
            assertTrue(result.getMessage(), result.isOk());
            for (IResource output : result.getTask().getOutputs()) {
                assertTrue("Static vector font copied source font to build output: " + output.getPath(),
                           !output.getPath().endsWith(".ttf"));
                Message message = ParseUtil.parse(output);
                if (message instanceof FontMap)
                    fontMap = (FontMap)message;
                else if (message instanceof GlyphBank)
                    glyphBank = (GlyphBank)message;
            }
        }

        assertTrue(fontMap != null);
        assertTrue(fontMap.getFont().isEmpty());
        assertTrue(fontMap.getGlyphBank().endsWith(".glyph_bankc"));
        assertTrue(fontMap.getCharacters().isEmpty());
        assertEquals(Fontc.VECTOR_REFERENCE_SIZE, fontMap.getSize());
        assertTrue(glyphBank != null);
        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_VECTOR,
                     glyphBank.getImageFormat());
        assertEquals(1, glyphBank.getGlyphsCount());
        assertTrue(glyphBank.getGlyphData().isEmpty());

        GlyphBank.Glyph glyph = glyphBank.getGlyphs(0);
        assertEquals(0, glyph.getGlyphDataSize());
        assertEquals((int)'A', glyph.getCharacter());
        assertEquals(11 * 8 * Float.BYTES, glyph.getVectorDataSize());
        ByteBuffer curves = ByteBuffer.wrap(glyphBank.getVectorData().toByteArray())
            .order(ByteOrder.nativeOrder());
        curves.position((int)glyph.getVectorDataOffset());
        float[] expectedFirstCurve = {
            0.49957448f, 0.83379120f,
            0.41276595f, 0.63015109f,
            0.32595745f, 0.42651099f,
            -3.13543630f, -0.98687697f
        };
        for (float expected : expectedFirstCurve)
            assertEquals(expected, curves.getFloat(), 0.00001f);
    }

    @Test
    public void testVectorAndSdfFontsHaveDifferentGlyphBankHashes() {
        FontDesc sdf = FontDesc.newBuilder()
            .setFont("/Tuffy.ttf")
            .setMaterial("/test.material")
            .setSize(Fontc.VECTOR_REFERENCE_SIZE)
            .setCharacters("A")
            .setVectorFontMode(VectorFontMode.VECTOR_FONT_MODE_SDF)
            .build();
        FontDesc vector = sdf.toBuilder()
            .setVectorFontMode(VectorFontMode.VECTOR_FONT_MODE_VECTOR)
            .build();

        assertNotEquals(Fontc.FontDescToHash(sdf), Fontc.FontDescToHash(vector));
    }

    @Test
    public void testStaticVectorEffectChangesGlyphBankHash() {
        FontDesc withoutEffectImage = FontDesc.newBuilder()
            .setFont("/Tuffy.ttf")
            .setMaterial("/test-vector.material")
            .setSize(Fontc.VECTOR_REFERENCE_SIZE)
            .setCharacters("A")
            .setOutlineWidth(2.0f)
            .setOutlineAlpha(0.0f)
            .setVectorFontMode(VectorFontMode.VECTOR_FONT_MODE_VECTOR)
            .setRuntime(false)
            .build();
        FontDesc withEffectImage = withoutEffectImage.toBuilder()
            .setOutlineAlpha(1.0f)
            .build();

        assertNotEquals(Fontc.FontDescToHash(withoutEffectImage),
                        Fontc.FontDescToHash(withEffectImage));

        FontDesc withoutShadowImage = withoutEffectImage.toBuilder()
            .setOutlineWidth(0.0f)
            .setShadowAlpha(1.0f)
            .setShadowX(0.0f)
            .build();
        FontDesc withShadowImage = withoutShadowImage.toBuilder()
            .setShadowX(1.0f)
            .build();
        assertNotEquals(Fontc.FontDescToHash(withoutShadowImage),
                        Fontc.FontDescToHash(withShadowImage));

        FontDesc shadowWithOutline = withShadowImage.toBuilder().setOutlineWidth(4.0f).setOutlineAlpha(1.0f).build();
        FontDesc shadowWithoutOutline = shadowWithOutline.toBuilder().setOutlineAlpha(0.0f).build();
        assertNotEquals(Fontc.FontDescToHash(shadowWithOutline), Fontc.FontDescToHash(shadowWithoutOutline));

        FontDesc invisibleShadow = withShadowImage.toBuilder().setShadowAlpha(0.0f).build();
        assertEquals(Fontc.FontDescToHash(withoutShadowImage), Fontc.FontDescToHash(invisibleShadow));
    }

    @Test
    public void testStaticVectorFontBakesOutlineBitmap() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("runtime: false\n");
        src.append("characters: \"A\"\n");
        src.append("size: 32\n");
        src.append("outline_width: 2\n");
        src.append("outline_alpha: 1\n");

        List<Message> buildResults = build("/static-vector-outline.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = getGlyphBank(buildResults);

        assertTrue(fontMap != null);
        assertEquals(32, fontMap.getSize());
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(fontMap.getVectorBitmapEffects());
        assertTrue(fontMap.getFont().isEmpty());
        assertEquals(3, glyphBank.getGlyphChannels());
        assertTrue(glyphBank.getVectorBitmapEffects());
        assertTrue(glyphBank != null);
        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_VECTOR,
                     glyphBank.getImageFormat());
        assertTrue(!glyphBank.getVectorData().isEmpty());
        assertTrue(!glyphBank.getGlyphData().isEmpty());
        assertTrue(glyphBank.getGlyphs(0).getVectorDataSize() > 0);
        assertTrue(glyphBank.getGlyphs(0).getGlyphDataSize() > 0);
    }

    @Test
    public void testStaticVectorFontBakesShadowBitmap() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("runtime: false\n");
        src.append("characters: \"A\"\n");
        src.append("size: 32\n");
        src.append("outline_width: 4\n");
        src.append("outline_alpha: 1\n");

        src.append("shadow_blur: 16\nshadow_alpha: 1\nshadow_x: 4\n");

        List<Message> buildResults = build("/static-vector-shadow.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = getGlyphBank(buildResults);

        assertTrue(fontMap != null);
        assertEquals(32, fontMap.getSize());
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(fontMap.getVectorBitmapEffects());
        assertTrue(fontMap.getFont().isEmpty());
        assertEquals(3, glyphBank.getGlyphChannels());
        assertTrue(glyphBank.getVectorBitmapEffects());
        assertTrue(glyphBank != null);
        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_VECTOR,
                     glyphBank.getImageFormat());
        assertTrue(!glyphBank.getVectorData().isEmpty());
        assertTrue(!glyphBank.getGlyphData().isEmpty());
        assertTrue(glyphBank.getGlyphs(0).getVectorDataSize() > 0);
        assertTrue(glyphBank.getGlyphs(0).getGlyphDataSize() > 0);
    }

    @Test
    public void testTTFSdfRuntime() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 24\n");
        src.append("runtime: true\n");
        src.append("characters: \"Prewarm\"\n");
        src.append("all_chars: true\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals(FontTextureFormat.TYPE_DISTANCE_FIELD, fontMap.getOutputFormat());
        assertEquals(FontRenderMode.MODE_SINGLE_LAYER, fontMap.getRenderMode());
        assertEquals(24, fontMap.getSize());
        assertEquals("Prewarm", fontMap.getCharacters());
        assertTrue(!fontMap.getAllChars());
        assertEquals("/Tuffy.ttf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
    }

    @Test
    public void testInvisibleVectorEffectsDoNotBakeSdf() throws Exception {
        String font = "font: \"/Tuffy.ttf\"\nmaterial: \"/test-vector.material\"\n" +
            "vector_font_mode: VECTOR_FONT_MODE_VECTOR\nruntime: false\ncharacters: \"A\"\n" +
            "outline_width: 2\noutline_alpha: 0\nshadow_blur: 3\nshadow_x: 2\nshadow_alpha: 0\n";
        List<Message> results = build("/invisible-effects.font", font);
        FontMap fontMap = getFontMap(results);
        GlyphBank glyphBank = getGlyphBank(results);
        assertEquals(16, fontMap.getSize());
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(glyphBank.getGlyphData().isEmpty());
        assertTrue(!glyphBank.getVectorData().isEmpty());
    }

    @Test
    public void testVectorEffectsUseAuthoredSizeForStaticAndDynamic() throws Exception {
        for (boolean runtime : new boolean[] {false, true}) {
            for (String effect : new String[] {"outline_width: 2\noutline_alpha: 1\n", "shadow_x: 2\nshadow_alpha: 1\n", "shadow_blur: 3\nshadow_alpha: 1\n"}) {
                String source = "font: \"/Tuffy.ttf\"\nmaterial: \"/test-vector.material\"\n" +
                    "vector_font_mode: VECTOR_FONT_MODE_VECTOR\ncharacters: \"A\"\nruntime: " + runtime + "\n" + effect;
                try {
                    build("/effect.font", source);
                    fail("Vector effects must not infer a generation size from consumers");
                } catch (CompileExceptionError e) {
                    assertTrue(e.getMessage(), e.getMessage().contains("positive Size"));
                }
                FontMap fontMap = getFontMap(build("/effect.font", source + "size: 37\n"));
                assertEquals(37, fontMap.getSize());
                assertEquals(runtime, fontMap.getGlyphBank().isEmpty());
            }
        }
    }

    @Test
    public void testLabelVectorEffectsRequireFontSupport() throws Exception {
        String font = "font: \"/Tuffy.ttf\"\nmaterial: \"/test-vector.material\"\nvector_font_mode: VECTOR_FONT_MODE_VECTOR\ncharacters: \"A\"\n";
        addFile("/effects.font", font);
        String label = "size {}\nfont: \"/effects.font\"\nmaterial: \"/test-vector.material\"\nfont_size: 64\n";
        build("/effects.label", label + "text: \"Plain\"\n");
        for (String override : new String[] {"outline { x: 1 }", "shadow { w: 0.5 }", "text: \"<outline>A</outline>\"", "text: \"<shadow>A</shadow>\""}) {
            try {
                build("/effects.label", label + override);
                fail("Missing Vector font effect must fail: " + override);
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage(), e.getMessage().contains(".font resource"));
                assertTrue(e.getMessage(), e.getMessage().startsWith("The font has no "));
            }
        }
        addFile("/effects.font", font + "size: 37\noutline_width: 2\noutline_alpha: 1\nshadow_x: 2\nshadow_alpha: 1\n");
        FontMap fontMap = getFontMap(build("/effects.label", label + "text: \"<outline><shadow>A</shadow></outline>\"\noutline { x: 1 }\nshadow { w: 0.5 }"));
        assertEquals(37, fontMap.getSize());
    }

    @Test
    public void testVectorEffectTagsAreLiteralWhenRichTextIsExcluded() throws Exception {
        getProject().setOption("platform", "arm64-macos");
        getProject().setOption("architectures", "arm64-macos");
        getProject().getProjectProperties().putStringValue("native_extension", "app_manifest", "plain.appmanifest");
        addFile("/plain.appmanifest", "platforms:\n  arm64-osx:\n    context:\n      excludeLibs: [font_richtext]\n      libs: [font_richtext_null]\n");
        getProject().configurePreBuildProjectOptions();
        assertEquals("false", getProject().option("font-rich-text", "true"));
        String font = "font: \"/Tuffy.ttf\"\nmaterial: \"/test-vector.material\"\nvector_font_mode: VECTOR_FONT_MODE_VECTOR\n";
        addFile("/plain.font", font);
        String label = "size {}\nfont: \"/plain.font\"\nmaterial: \"/test-vector.material\"\ntext: \"<outline><shadow>A</shadow></outline>\"\n";
        // Exercise the content builders without requesting a custom engine build.
        addFile("/plain.label", label);
        Task labelTask = getProject().createTask(getProject().getResource("/plain.label"), ProtoBuilders.LabelDescBuilder.class);
        labelTask.getBuilder().build(labelTask);
        addFile("/plain.gui", "material: \"/test.material\"\nfonts { name: \"font\" font: \"/plain.font\" }\n" +
            "nodes { type: TYPE_TEXT id: \"text\" font: \"font\" text: \"<outline><shadow>A</shadow></outline>\" }\n");
        Task guiTask = getProject().createTask(getProject().getResource("/plain.gui"), GuiBuilder.class);
        guiTask.getBuilder().build(guiTask);
        try {
            addFile("/override.label", label + "outline { x: 1 }\n");
            Task overrideTask = getProject().createTask(getProject().getResource("/override.label"), ProtoBuilders.LabelDescBuilder.class);
            overrideTask.getBuilder().build(overrideTask);
            fail("Node colors must still be validated without rich text");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains(".font resource"));
        }
    }

    @Test
    public void testGuiVectorEffectsValidateResolvedLayoutsAndTemplates() throws Exception {
        String font = "font: \"/Tuffy.ttf\"\nmaterial: \"/test-vector.material\"\nvector_font_mode: VECTOR_FONT_MODE_VECTOR\ncharacters: \"A\"\n";
        addFile("/effects.font", font);
        String scene = "material: \"/test.material\"\nfonts { name: \"font\" font: \"/effects.font\" }\n";
        String node = "nodes { type: TYPE_TEXT id: \"text\" font: \"font\" text: \"A\" }\n";
        build("/effects.gui", scene + node);
        for (String override : new String[] {"outline { x: 1 }", "shadow_alpha: 0.5", "text: \"<outline>A</outline>\"", "text: \"<shadow>A</shadow>\""}) {
            try {
                build("/effects.gui", scene + "nodes { type: TYPE_TEXT id: \"text\" font: \"font\" " + override + " }\n");
                fail("Missing GUI font effect must fail: " + override);
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage(), e.getMessage().contains(".font resource"));
            }
        }
        try {
            build("/effects.gui", scene + node + "layouts { name: \"wide\" nodes { id: \"text\" outline_alpha: 0.5 overridden_fields: 31 } }\n");
            fail("GUI layout overrides must be validated");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains(".font resource"));
        }
        addFile("/template.gui", scene + node);
        String parent = "material: \"/test.material\"\nnodes { type: TYPE_TEMPLATE id: \"instance\" template: \"/template.gui\" }\n" +
            "nodes { type: TYPE_TEXT id: \"instance/text\" template_node_child: true shadow_alpha: 0.5 overridden_fields: 32 }\n";
        try {
            build("/parent.gui", parent);
            fail("GUI template overrides must be validated");
        } catch (CompileExceptionError e) {
            assertTrue(e.getMessage(), e.getMessage().contains(".font resource"));
        }
        addFile("/effects.font", font + "size: 37\nshadow_x: 2\nshadow_alpha: 1\n");
        build("/parent.gui", parent);
    }

    @Test
    public void testExplicitStaticSdfOverridesLegacyRuntimeGeneration() throws Exception {
        getProject().setOption("font-runtime-generation", "true");

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("runtime: false\n");
        src.append("characters: \"A\"\n");

        FontMap fontMap = getFontMap(build("/static-sdf.font", src.toString()));
        assertTrue(fontMap.getFont().isEmpty());
        assertTrue(!fontMap.getGlyphBank().isEmpty());
        assertTrue(fontMap.getCharacters().isEmpty());
    }

    @Test
    public void testEffectiveFontSettingsMatrix() {
        String[] extensions = {"ttf", "otf", "fnt"};
        VectorFontMode[] modes = {VectorFontMode.VECTOR_FONT_MODE_SDF, VectorFontMode.VECTOR_FONT_MODE_VECTOR};
        Boolean[] runtimeOptions = {null, false, true};
        boolean[] booleans = {false, true};

        for (String extension : extensions) {
            for (VectorFontMode mode : modes) {
                for (Boolean runtime : runtimeOptions) {
                    for (boolean legacyRuntime : booleans) {
                        for (boolean allChars : booleans) {
                            FontDesc.Builder builder = FontDesc.newBuilder()
                                .setFont("/source." + extension)
                                .setMaterial("/test.material")
                                .setSize(24)
                                .setCharacters("A")
                                .setAllChars(allChars)
                                .setVectorFontMode(mode);
                            if (runtime != null)
                                builder.setRuntime(runtime);

                            FontDesc effective = FontBuilder.getEffectiveFontDesc(builder.build(), legacyRuntime);
                            String context = extension + ", " + mode + ", runtime=" + runtime +
                                ", legacy=" + legacyRuntime + ", allChars=" + allChars;
                            boolean bitmap = extension.equals("fnt");
                            boolean vector = !bitmap && mode == VectorFontMode.VECTOR_FONT_MODE_VECTOR;
                            boolean dynamic = !bitmap &&
                                (runtime != null ? runtime : vector || legacyRuntime);

                            assertEquals(context, dynamic, effective.getRuntime());
                            assertEquals(context, !dynamic && allChars, effective.getAllChars());
                            assertEquals(context, bitmap ? FontTextureFormat.TYPE_BITMAP : FontTextureFormat.TYPE_DISTANCE_FIELD,
                                         effective.getOutputFormat());
                            assertEquals(context, vector ? FontRenderMode.MODE_MULTI_LAYER : FontRenderMode.MODE_SINGLE_LAYER,
                                         effective.getRenderMode());
                            assertEquals(context, vector, effective.getVectorFontMode() == VectorFontMode.VECTOR_FONT_MODE_VECTOR);
                            assertEquals(context, vector ? Fontc.VECTOR_REFERENCE_SIZE : 24, effective.getSize());
                        }
                    }
                }
            }
        }
    }

    @Test
    public void testTTFDefaultSdfMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("size: 16\n");
        src.append("shadow_blur: 1\n");
        src.append("shadow_alpha: 1\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(fontMap.getVectorBitmapEffects());
    }

    @Test
    public void testTTFZeroBlurDoesNotUseSdfEffectMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("size: 16\n");
        src.append("shadow_blur: 0\n");
        src.append("shadow_alpha: 1\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("sdf_material: \"/test2.material\"\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertTrue(fontMap.getSdfMaterial().isEmpty());
    }

    @Test
    public void testTTFOutlineUsesSharedVectorMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("size: 16\n");
        src.append("outline_width: 2\n");
        src.append("outline_alpha: 1\n");
        src.append("shadow_blur: 0\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(fontMap.getVectorBitmapEffects());
    }

    @Test
    public void testTTFLabelOutlineUsesSharedVectorMaterial() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/Tuffy.ttf\"\n");
        src.append("material: \"/builtins/fonts/label-vector.material\"\n");
        src.append("size: 16\n");
        src.append("outline_width: 2\n");
        src.append("outline_alpha: 1\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));

        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertTrue(fontMap.getVectorBitmapEffects());
    }

    @Test
    public void testLegacyGlyphBankWireFormat() throws Exception {
        // Produced by the former com.dynamo.render.proto.Font.GlyphBank schema.
        byte[] bitmapGlyphBank = new byte[] { 0x50, 0x00 };
        byte[] distanceFieldGlyphBank = new byte[] { 0x50, 0x01 };

        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_BITMAP,
                     GlyphBank.parseFrom(bitmapGlyphBank).getImageFormat());
        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_DISTANCE_FIELD,
                     GlyphBank.parseFrom(distanceFieldGlyphBank).getImageFormat());
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
        assertTrue(fontMap.getCharacters().isEmpty());
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
    public void testVectorOTF() throws Exception {
        addFile("/Test.otf", getFile("/Tuffy.ttf"));

        StringBuilder src = new StringBuilder();
        src.append("font: \"/Test.otf\"\n");
        src.append("material: \"/test-vector.material\"\n");
        src.append("size: 48\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");

        FontMap fontMap = getFontMap(build("/test.font", src.toString()));
        assertEquals("/Test.otf", fontMap.getFont());
        assertTrue(fontMap.getGlyphBank().isEmpty());
        assertEquals(Fontc.VECTOR_REFERENCE_SIZE, fontMap.getSize());
    }

    @Test
    public void testFNT() throws Exception {

        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("size: 16\n");
        src.append("output_format: TYPE_DISTANCE_FIELD\n");
        src.append("render_mode: MODE_MULTI_LAYER\n");
        src.append("characters: \"ABC\"\n");
        src.append("shadow_blur: 3\n");
        src.append("sdf_material: \"/test2.material\"\n");
        List<Message> buildResults = build("/test.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = getGlyphBank(buildResults);

        assertEquals(fontMap.getMaterial(), ResourceUtil.minifyPath("/test.materialc"));
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertEquals(FontTextureFormat.TYPE_BITMAP, fontMap.getOutputFormat());
        assertEquals(FontRenderMode.MODE_SINGLE_LAYER, fontMap.getRenderMode());
        assertEquals(32, fontMap.getSize());
        assertTrue(fontMap.getCharacters().isEmpty());
        assertTrue(fontMap.getFont().isEmpty());
        assertTrue(!fontMap.getGlyphBank().isEmpty());
        assertEquals(3, glyphBank.getGlyphsCount());
    }

    @Test
    public void testFNTIgnoresStaleVectorAndDynamicSettings() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("vector_font_mode: VECTOR_FONT_MODE_VECTOR\n");
        src.append("runtime: true\n");
        src.append("sdf_material: \"/test2.material\"\n");

        List<Message> buildResults = build("/stale-bmfont.font", src.toString());
        FontMap fontMap = getFontMap(buildResults);
        GlyphBank glyphBank = getGlyphBank(buildResults);

        assertEquals(FontTextureFormat.TYPE_BITMAP, fontMap.getOutputFormat());
        assertTrue(fontMap.getFont().isEmpty());
        assertTrue(!fontMap.getGlyphBank().isEmpty());
        assertTrue(fontMap.getSdfMaterial().isEmpty());
        assertEquals(com.dynamo.font.proto.GlyphBankProto.FontTextureFormat.TYPE_BITMAP,
                     glyphBank.getImageFormat());
    }

    @Test
    public void testFNTAllChars() throws Exception {
        StringBuilder src = new StringBuilder();
        src.append("font: \"/bmfont.fnt\"\n");
        src.append("material: \"/test.material\"\n");
        src.append("characters: \"A\"\n");
        src.append("all_chars: true\n");

        GlyphBank glyphBank = getGlyphBank(build("/test.font", src.toString()));
        assertTrue(glyphBank.getGlyphsCount() > 1);
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

        try {
            Task task = getProject().createTask(getProject().getResource("/invalid.font"), GlyphBankBuilder.class);
            task.getBuilder().build(task);
            fail("Expected malformed BMFont data to produce a CompileExceptionError");
        } catch (CompileExceptionError e) {
            assertEquals("invalid.fnt", e.getResource().getPath());
            assertTrue(!e.getMessage().isEmpty());
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
