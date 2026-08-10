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

package com.dynamo.bob.font;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

import org.junit.Test;
import org.junit.internal.TextListener;
import org.junit.runner.JUnitCore;
import org.junit.runner.Result;

public class FontRendererTest {
    private static final int VERTEX_STRIDE = 96;
    private static final float[] IDENTITY = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    private static final float[] WHITE = {1.0f, 1.0f, 1.0f, 1.0f};

    private static final class TestVertices {
        private final ByteBuffer vertices;
        private final int vertexCount;

        private TestVertices(ByteBuffer vertices, int vertexCount) {
            this.vertices = vertices;
            this.vertexCount = vertexCount;
        }
    }

    private static TestVertices getVertices(FontRenderer renderer, float[] worldTransform) {
        FontRenderer.VertexBufferRequirements requirements = renderer.getVertexBufferRequirements();
        ByteBuffer vertices = ByteBuffer.allocateDirect(requirements.byteCount).order(ByteOrder.nativeOrder());
        renderer.getVertices(worldTransform, vertices, requirements);
        vertices.flip();
        return new TestVertices(vertices, requirements.vertexCount);
    }

    private static FontRenderer createRenderer(float size) throws Exception {
        return createRenderer(size, 512, 512, false);
    }

    private static FontRenderer createRenderer(float size, int cacheWidth, int cacheHeight) throws Exception {
        return createRenderer(size, cacheWidth, cacheHeight, false);
    }

    private static FontRenderer createRenderer(float size, int cacheWidth, int cacheHeight, boolean useTextShaping) throws Exception {
        return createRenderer("/NotoSans-Regular.ttf", size, cacheWidth, cacheHeight, useTextShaping);
    }

    private static FontRenderer createRenderer(String fontResource, float size, int cacheWidth, int cacheHeight, boolean useTextShaping) throws Exception {
        byte[] fontBytes;
        try (InputStream input = FontRendererTest.class.getResourceAsStream(fontResource)) {
            assertNotNull(input);
            fontBytes = input.readAllBytes();
        }
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = size;
        params.cacheWidth = cacheWidth;
        params.cacheHeight = cacheHeight;
        params.useTextShaping = useTextShaping;
        return new FontRenderer(fontResource, fontBytes, params);
    }

    @Test
    public void testLayoutSelection() throws Exception {
        try (FontRenderer legacy = createRenderer(32.0f, 512, 512, false);
             FontRenderer skribidi = createRenderer(32.0f, 512, 512, true)) {
            float legacyWidth = legacy.measure("AV", false, 0.0f, 1.0f, 0.0f).width;
            float skribidiWidth = skribidi.measure("AV", false, 0.0f, 1.0f, 0.0f).width;
            assertTrue(skribidiWidth < legacyWidth);
        }
    }

    @Test
    public void testArabicTextShaping() throws Exception {
        try (FontRenderer legacy = createRenderer("/NotoSansArabic-Regular.ttf", 32.0f, 512, 512, false);
             FontRenderer skribidi = createRenderer("/NotoSansArabic-Regular.ttf", 32.0f, 512, 512, true)) {
            FontRenderer.Properties properties = properties(100.0f, 1.0f, 0);
            legacy.setProperties(properties);
            skribidi.setProperties(properties);
            legacy.setText("لكن");
            skribidi.setText("لكن");
            legacy.beginBatch();
            skribidi.beginBatch();
            FontRenderer.Texture legacyTexture = legacy.generateTexture(0);
            FontRenderer.Texture skribidiTexture = skribidi.generateTexture(0);
            assertTrue(!legacyTexture.pixels.equals(skribidiTexture.pixels));
            assertEquals(18, getVertices(legacy, IDENTITY).vertexCount);
            assertEquals(24, getVertices(skribidi, IDENTITY).vertexCount);
        }
    }

    private static FontRenderer.Properties properties(float height, float leading, int verticalAlign) {
        FontRenderer.Properties properties = new FontRenderer.Properties();
        properties.height = height;
        properties.leading = leading;
        properties.verticalAlign = verticalAlign;
        properties.faceColor = WHITE;
        properties.outlineColor = WHITE;
        properties.shadowColor = WHITE;
        properties.sdfScale = 1.0f;
        return properties;
    }

    @Test
    public void testMeasureRenderAndAtlasSynchronization() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.Layout layout = renderer.measure("Ag\nHello", false, 0.0f, 1.0f, 0.0f);
            assertEquals(2, layout.lineCount);
            assertTrue(layout.width > 0.0f);
            assertTrue(layout.height > 0.0f);

            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText("Ag");
            renderer.beginBatch();
            FontRenderer.Texture firstTexture = renderer.generateTexture(0);
            assertEquals(firstTexture.atlasVersion, renderer.atlasVersion());
            TestVertices firstVertices = getVertices(renderer, IDENTITY);
            assertEquals(12, firstVertices.vertexCount);
            assertEquals(firstVertices.vertexCount * VERTEX_STRIDE, firstVertices.vertices.remaining());
            assertNotNull(firstTexture.pixels);
            assertEquals(0, firstTexture.x);
            assertEquals(0, firstTexture.y);
            assertEquals(512, firstTexture.width);
            assertEquals(512, firstTexture.height);

            FontRenderer.VertexBufferRequirements requirements = renderer.getVertexBufferRequirements();
            ByteBuffer combinedVertices = ByteBuffer.allocateDirect(requirements.byteCount + Long.BYTES).order(ByteOrder.nativeOrder());
            combinedVertices.putLong(0x123456789abcdef0L);
            renderer.getVertices(IDENTITY, combinedVertices, requirements);
            assertEquals(Long.BYTES + requirements.byteCount, combinedVertices.position());
            assertEquals(0x123456789abcdef0L, combinedVertices.getLong(0));
            ByteBuffer appendedVertices = combinedVertices.duplicate().order(ByteOrder.nativeOrder());
            appendedVertices.limit(combinedVertices.position());
            appendedVertices.position(Long.BYTES);
            assertEquals(firstVertices.vertices, appendedVertices);

            FontRenderer.Texture currentTexture = renderer.generateTexture(firstTexture.atlasVersion);
            TestVertices currentVertices = getVertices(renderer, IDENTITY);
            assertEquals(firstTexture.atlasVersion, currentTexture.atlasVersion);
            assertNull(currentTexture.pixels);
            assertEquals(firstVertices.vertices, currentVertices.vertices);

            float[] translatedTransform = IDENTITY.clone();
            translatedTransform[12] = 10.0f;
            translatedTransform[13] = -5.0f;
            TestVertices translatedVertices = getVertices(renderer, translatedTransform);
            ByteBuffer original = firstVertices.vertices.duplicate().order(ByteOrder.nativeOrder());
            ByteBuffer translated = translatedVertices.vertices.duplicate().order(ByteOrder.nativeOrder());
            assertEquals(original.getFloat(0) + 10.0f, translated.getFloat(0), 0.001f);
            assertEquals(original.getFloat(4) - 5.0f, translated.getFloat(4), 0.001f);

            renderer.setText("");
            TestVertices emptyVertices = getVertices(renderer, IDENTITY);
            assertEquals(0, emptyVertices.vertexCount);
            assertEquals(0, emptyVertices.vertices.remaining());
        }
    }

    @Test
    public void testMultilineBaselineSpacing() throws Exception {
        float leading = 1.2f;
        try (FontRenderer renderer = createRenderer(24.0f)) {
            FontRenderer.Layout layout = renderer.measure("A\nA\nA", false, 0.0f, leading, 0.0f);
            assertEquals(3, layout.lineCount);

            float boxHeight = 120.0f;
            renderer.setProperties(properties(boxHeight, leading, 0));
            renderer.setText("A\nA\nA");
            renderer.beginBatch();
            FontRenderer.Texture texture = renderer.generateTexture(0);
            TestVertices top = getVertices(renderer, IDENTITY);
            assertEquals(18, top.vertexCount);
            ByteBuffer vertices = top.vertices.duplicate().order(ByteOrder.nativeOrder());
            float firstLineY = vertices.getFloat(4);
            float secondLineY = vertices.getFloat(6 * VERTEX_STRIDE + 4);
            float lineHeight = layout.height / (1.0f + leading * (layout.lineCount - 1));
            assertEquals(lineHeight * leading, firstLineY - secondLineY, 0.001f);

            assertNotNull(texture.pixels);
            renderer.setProperties(properties(boxHeight, leading, 1));
            TestVertices middle = getVertices(renderer, IDENTITY);
            renderer.setProperties(properties(boxHeight, leading, 2));
            TestVertices bottom = getVertices(renderer, IDENTITY);
            float middleFirstLineY = middle.vertices.duplicate().order(ByteOrder.nativeOrder()).getFloat(4);
            float bottomFirstLineY = bottom.vertices.duplicate().order(ByteOrder.nativeOrder()).getFloat(4);
            assertEquals((layout.height - boxHeight) * 0.5f, middleFirstLineY - firstLineY, 0.001f);
            assertEquals(layout.height - boxHeight, bottomFirstLineY - firstLineY, 0.001f);
        }
    }

    @Test
    public void testIncrementalAtlasUpdate() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText("Wg");
            renderer.beginBatch();
            FontRenderer.Texture initialTexture = renderer.generateTexture(0);
            assertNotNull(initialTexture.pixels);

            renderer.setText(".");
            renderer.beginBatch();
            FontRenderer.Texture incrementalTexture = renderer.generateTexture(initialTexture.atlasVersion);
            assertTrue(incrementalTexture.atlasVersion > initialTexture.atlasVersion);
            assertNotNull(incrementalTexture.pixels);
            assertTrue(incrementalTexture.width > 0);
            assertTrue(incrementalTexture.height > 0);
            assertTrue(incrementalTexture.x + incrementalTexture.width <= 512);
            assertTrue(incrementalTexture.y + incrementalTexture.height <= 512);
            assertEquals(incrementalTexture.width * incrementalTexture.height * incrementalTexture.channels,
                    incrementalTexture.pixels.remaining());
            assertEquals(6, getVertices(renderer, IDENTITY).vertexCount);
        }
    }

    @Test
    public void testAtlasEvictsGlyphsFromPreviousBatch() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f, 40, 40)) {
            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText(".");
            renderer.beginBatch();
            FontRenderer.Texture initialTexture = renderer.generateTexture(0);
            assertNotNull(initialTexture.pixels);
            assertEquals(6, getVertices(renderer, IDENTITY).vertexCount);

            renderer.setText("W");
            renderer.beginBatch();
            FontRenderer.Texture replacementTexture = renderer.generateTexture(initialTexture.atlasVersion);
            assertTrue(replacementTexture.atlasVersion > initialTexture.atlasVersion);
            assertNotNull(replacementTexture.pixels);
            assertEquals(6, getVertices(renderer, IDENTITY).vertexCount);

            renderer.setText(".");
            renderer.beginBatch();
            FontRenderer.Texture restoredTexture = renderer.generateTexture(replacementTexture.atlasVersion);
            assertNotNull(restoredTexture.pixels);
            assertEquals(6, getVertices(renderer, IDENTITY).vertexCount);
        }
    }

    @Test
    public void testRendererStateHash() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.Properties initialProperties = properties(100.0f, 1.0f, 0);
            renderer.setProperties(initialProperties);
            long propertiesHash = renderer.hash();
            renderer.setProperties(initialProperties);
            assertEquals(propertiesHash, renderer.hash());

            renderer.setText("Hello");
            long helloHash = renderer.hash();
            assertTrue(propertiesHash != helloHash);
            renderer.setText("Hello");
            assertEquals(helloHash, renderer.hash());
            renderer.setText("World");
            assertTrue(helloHash != renderer.hash());

            FontRenderer.Properties changedProperties = properties(100.0f, 1.1f, 0);
            renderer.setProperties(changedProperties);
            assertTrue(helloHash != renderer.hash());
        }
    }

    @Test
    public void testVertexBufferRequirementsSurviveAttributeChanges() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText("Hello");
            renderer.beginBatch();
            renderer.generateTexture(0);

            FontRenderer.VertexBufferRequirements requirements = renderer.getVertexBufferRequirements();
            FontRenderer.Properties changedProperties = properties(100.0f, 1.0f, 0);
            changedProperties.faceColor = new float[] {0.5f, 0.25f, 0.75f, 1.0f};
            renderer.setProperties(changedProperties);

            ByteBuffer vertices = ByteBuffer.allocateDirect(requirements.byteCount).order(ByteOrder.nativeOrder());
            renderer.getVertices(IDENTITY, vertices, requirements);
            assertEquals(requirements.byteCount, vertices.position());
        }
    }

    @Test
    public void testGenerateGlyph() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.GeneratedGlyph glyph = renderer.generateGlyph('A');
            assertTrue(glyph.width > 0);
            assertTrue(glyph.height > 0);
            assertEquals(1, glyph.channels);
            assertEquals(glyph.width * glyph.height * glyph.channels, glyph.pixels.remaining());
        }
    }

    @Test
    public void testGenerateMissingGlyphDoesNotRasterizeGlyphZero() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.GeneratedGlyph glyph = renderer.generateGlyph(Character.MAX_CODE_POINT);
            assertEquals(0, glyph.glyphIndex);
            assertEquals(0, glyph.width);
            assertEquals(0, glyph.height);
            assertEquals(0, glyph.pixels.remaining());
        }
    }

    @Test
    public void testGetSupportedGlyphMetrics() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.GlyphMetrics[] metrics = renderer.getSupportedGlyphMetrics();
            assertTrue(metrics.length > 0);

            FontRenderer.GlyphMetrics capitalA = null;
            int previousCodepoint = -1;
            for (FontRenderer.GlyphMetrics glyph : metrics) {
                assertTrue(glyph.codepoint > previousCodepoint);
                assertTrue(glyph.glyphIndex > 0);
                assertTrue(Character.isValidCodePoint(glyph.codepoint));
                if (glyph.codepoint == 'A')
                    capitalA = glyph;
                previousCodepoint = glyph.codepoint;
            }
            assertNotNull(capitalA);

            FontRenderer.GlyphMetrics direct = renderer.getGlyphMetrics('A');
            assertEquals(capitalA.codepoint, direct.codepoint);
            assertEquals(capitalA.glyphIndex, direct.glyphIndex);
            assertEquals(capitalA.width, direct.width);
            assertEquals(capitalA.height, direct.height);
            assertEquals(capitalA.advance, direct.advance, 0.0f);
            assertEquals(capitalA.leftBearing, direct.leftBearing, 0.0f);
            assertEquals(capitalA.ascent, direct.ascent, 0.0f);
            assertEquals(capitalA.descent, direct.descent, 0.0f);

            FontRenderer.GeneratedGlyph generated = renderer.generateGlyph('A');
            assertEquals(generated.glyphIndex, direct.glyphIndex);
            assertEquals(generated.width, direct.width);
            assertEquals(generated.height, direct.height);
        }
    }

    @Test
    public void testGetMissingGlyphMetrics() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.GlyphMetrics metrics = renderer.getGlyphMetrics(Character.MAX_CODE_POINT);
            assertEquals(Character.MAX_CODE_POINT, metrics.codepoint);
            assertEquals(0, metrics.glyphIndex);
            assertEquals(0, metrics.width);
            assertEquals(0, metrics.height);
        }
    }

    @Test
    public void testConcurrentSessionLifecycle() throws Exception {
        byte[] fontBytes;
        try (InputStream input = FontRendererTest.class.getResourceAsStream("/NotoSans-Regular.ttf")) {
            assertNotNull(input);
            fontBytes = input.readAllBytes();
        }
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = 32.0f;
        params.cacheWidth = 1;
        params.cacheHeight = 1;

        ExecutorService executor = Executors.newFixedThreadPool(8);
        try {
            List<Future<?>> futures = new ArrayList<>();
            for (int taskIndex = 0; taskIndex < 32; ++taskIndex) {
                futures.add(executor.submit(() -> {
                    try (FontRenderer renderer = new FontRenderer("NotoSans-Regular.ttf", fontBytes, params)) {
                        assertTrue(renderer.generateGlyph('A').width > 0);
                    }
                }));
            }
            for (Future<?> future : futures)
                future.get();
        } finally {
            executor.shutdownNow();
        }
    }

    @Test
    public void testUnavailableGlyphsDoNotAbortRendering() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f, 1, 1)) {
            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText("ABC");
            renderer.beginBatch();
            FontRenderer.Texture texture = renderer.generateTexture(0);
            TestVertices vertices = getVertices(renderer, IDENTITY);
            assertNotNull(texture.pixels);
            assertEquals(0, vertices.vertexCount);
            assertEquals(0, vertices.vertices.remaining());
        }
    }

    @Test
    public void testRejectsAtlasPixelCountOverflow() throws Exception {
        byte[] fontBytes;
        try (InputStream input = FontRendererTest.class.getResourceAsStream("/NotoSans-Regular.ttf")) {
            assertNotNull(input);
            fontBytes = input.readAllBytes();
        }
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = 32.0f;
        params.cacheWidth = 65535;
        params.cacheHeight = 65535;
        params.shadowBlur = 1.0f;
        try (FontRenderer ignored = new FontRenderer("NotoSans-Regular.ttf", fontBytes, params)) {
            fail("Expected oversized atlas to be rejected");
        } catch (IllegalArgumentException expected) {
        }
    }

    public static void main(String[] args) {
        JUnitCore junit = new JUnitCore();
        junit.addListener(new TextListener(System.out));
        Result result = junit.run(FontRendererTest.class);
        if (!result.wasSuccessful()) {
            System.exit(1);
        }
    }
}
