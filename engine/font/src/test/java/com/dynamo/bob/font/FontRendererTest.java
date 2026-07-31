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

    private static FontRenderer createRenderer(float size) throws Exception {
        return createRenderer(size, 512, 512);
    }

    private static FontRenderer createRenderer(float size, int cacheWidth, int cacheHeight) throws Exception {
        byte[] fontBytes;
        try (InputStream input = FontRendererTest.class.getResourceAsStream("/NotoSans-Regular.ttf")) {
            assertNotNull(input);
            fontBytes = input.readAllBytes();
        }
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = size;
        params.cacheWidth = cacheWidth;
        params.cacheHeight = cacheHeight;
        return new FontRenderer("NotoSans-Regular.ttf", fontBytes, params);
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
            FontRenderer.Vertices firstVertices = renderer.getVertices(IDENTITY);
            assertEquals(12, firstVertices.vertexCount);
            assertEquals(firstVertices.vertexCount * VERTEX_STRIDE, firstVertices.vertices.remaining());
            assertNotNull(firstTexture.pixels);
            assertEquals(0, firstTexture.x);
            assertEquals(0, firstTexture.y);
            assertEquals(512, firstTexture.width);
            assertEquals(512, firstTexture.height);

            FontRenderer.Texture currentTexture = renderer.generateTexture(firstTexture.atlasVersion);
            FontRenderer.Vertices currentVertices = renderer.getVertices(IDENTITY);
            assertEquals(firstTexture.atlasVersion, currentTexture.atlasVersion);
            assertNull(currentTexture.pixels);
            assertEquals(firstVertices.vertices, currentVertices.vertices);

            float[] translatedTransform = IDENTITY.clone();
            translatedTransform[12] = 10.0f;
            translatedTransform[13] = -5.0f;
            FontRenderer.Vertices translatedVertices = renderer.getVertices(translatedTransform);
            ByteBuffer original = firstVertices.vertices.duplicate().order(ByteOrder.nativeOrder());
            ByteBuffer translated = translatedVertices.vertices.duplicate().order(ByteOrder.nativeOrder());
            assertEquals(original.getFloat(0) + 10.0f, translated.getFloat(0), 0.001f);
            assertEquals(original.getFloat(4) - 5.0f, translated.getFloat(4), 0.001f);

            renderer.setText("");
            FontRenderer.Vertices emptyVertices = renderer.getVertices(IDENTITY);
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
            FontRenderer.Vertices top = renderer.getVertices(IDENTITY);
            assertEquals(18, top.vertexCount);
            ByteBuffer vertices = top.vertices.duplicate().order(ByteOrder.nativeOrder());
            float firstLineY = vertices.getFloat(4);
            float secondLineY = vertices.getFloat(6 * VERTEX_STRIDE + 4);
            float lineHeight = layout.height / (1.0f + leading * (layout.lineCount - 1));
            assertEquals(lineHeight * leading, firstLineY - secondLineY, 0.001f);

            assertNotNull(texture.pixels);
            renderer.setProperties(properties(boxHeight, leading, 1));
            FontRenderer.Vertices middle = renderer.getVertices(IDENTITY);
            renderer.setProperties(properties(boxHeight, leading, 2));
            FontRenderer.Vertices bottom = renderer.getVertices(IDENTITY);
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
            assertEquals(6, renderer.getVertices(IDENTITY).vertexCount);
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
            assertEquals(6, renderer.getVertices(IDENTITY).vertexCount);

            renderer.setText("W");
            renderer.beginBatch();
            FontRenderer.Texture replacementTexture = renderer.generateTexture(initialTexture.atlasVersion);
            assertTrue(replacementTexture.atlasVersion > initialTexture.atlasVersion);
            assertNotNull(replacementTexture.pixels);
            assertEquals(6, renderer.getVertices(IDENTITY).vertexCount);

            renderer.setText(".");
            renderer.beginBatch();
            FontRenderer.Texture restoredTexture = renderer.generateTexture(replacementTexture.atlasVersion);
            assertNotNull(restoredTexture.pixels);
            assertEquals(6, renderer.getVertices(IDENTITY).vertexCount);
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
    public void testUnavailableGlyphsDoNotAbortRendering() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f, 1, 1)) {
            renderer.setProperties(properties(100.0f, 1.0f, 0));
            renderer.setText("ABC");
            renderer.beginBatch();
            FontRenderer.Texture texture = renderer.generateTexture(0);
            FontRenderer.Vertices vertices = renderer.getVertices(IDENTITY);
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
