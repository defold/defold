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
import static org.junit.Assert.assertTrue;

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
        byte[] fontBytes;
        try (InputStream input = FontRendererTest.class.getResourceAsStream("/NotoSans-Regular.ttf")) {
            assertNotNull(input);
            fontBytes = input.readAllBytes();
        }
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = size;
        params.cacheWidth = 512;
        params.cacheHeight = 512;
        return new FontRenderer("NotoSans-Regular.ttf", fontBytes, params);
    }

    @Test
    public void testMeasureRenderAndAtlasSynchronization() throws Exception {
        try (FontRenderer renderer = createRenderer(32.0f)) {
            FontRenderer.Layout layout = renderer.measure("Ag\nHello", false, 0.0f, 1.0f, 0.0f);
            assertEquals(2, layout.lineCount);
            assertTrue(layout.width > 0.0f);
            assertTrue(layout.height > 0.0f);

            FontRenderer.RenderResult first = renderer.render("Ag", false, 0.0f, 100.0f,
                                                                      1.0f, 0.0f, 0, 0, IDENTITY,
                                                                      WHITE, WHITE, WHITE, 1.0f, 0);
            assertEquals(12, first.vertexCount);
            assertEquals(first.vertexCount * VERTEX_STRIDE, first.vertices.remaining());
            assertNotNull(first.textureUpdate);
            assertEquals(0, first.textureUpdate.x);
            assertEquals(0, first.textureUpdate.y);
            assertEquals(512, first.textureUpdate.width);
            assertEquals(512, first.textureUpdate.height);

            FontRenderer.RenderResult current = renderer.render("Ag", false, 0.0f, 100.0f,
                                                                        1.0f, 0.0f, 0, 0, IDENTITY,
                                                                        WHITE, WHITE, WHITE, 1.0f,
                                                                        first.atlasVersion);
            assertEquals(first.atlasVersion, current.atlasVersion);
            assertEquals(first.vertices, current.vertices);
        }
    }

    @Test
    public void testMultilineBaselineSpacing() throws Exception {
        float leading = 1.2f;
        try (FontRenderer renderer = createRenderer(24.0f)) {
            FontRenderer.Layout layout = renderer.measure("A\nA\nA", false, 0.0f, leading, 0.0f);
            assertEquals(3, layout.lineCount);

            float boxHeight = 120.0f;
            FontRenderer.RenderResult top = renderer.render("A\nA\nA", false, 0.0f, boxHeight,
                                                                    leading, 0.0f, 0, 0, IDENTITY,
                                                                    WHITE, WHITE, WHITE, 1.0f, 0);
            assertEquals(18, top.vertexCount);
            ByteBuffer vertices = top.vertices.duplicate().order(ByteOrder.nativeOrder());
            float firstLineY = vertices.getFloat(4);
            float secondLineY = vertices.getFloat(6 * VERTEX_STRIDE + 4);
            float lineHeight = layout.height / (1.0f + leading * (layout.lineCount - 1));
            assertEquals(lineHeight * leading, firstLineY - secondLineY, 0.001f);

            FontRenderer.RenderResult middle = renderer.render("A\nA\nA", false, 0.0f, boxHeight,
                                                                       leading, 0.0f, 0, 1, IDENTITY,
                                                                       WHITE, WHITE, WHITE, 1.0f,
                                                                       top.atlasVersion);
            FontRenderer.RenderResult bottom = renderer.render("A\nA\nA", false, 0.0f, boxHeight,
                                                                       leading, 0.0f, 0, 2, IDENTITY,
                                                                       WHITE, WHITE, WHITE, 1.0f,
                                                                       middle.atlasVersion);
            float middleFirstLineY = middle.vertices.duplicate().order(ByteOrder.nativeOrder()).getFloat(4);
            float bottomFirstLineY = bottom.vertices.duplicate().order(ByteOrder.nativeOrder()).getFloat(4);
            assertEquals((layout.height - boxHeight) * 0.5f, middleFirstLineY - firstLineY, 0.001f);
            assertEquals(layout.height - boxHeight, bottomFirstLineY - firstLineY, 0.001f);
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

    public static void main(String[] args) {
        JUnitCore junit = new JUnitCore();
        junit.addListener(new TextListener(System.out));
        Result result = junit.run(FontRendererTest.class);
        if (!result.wasSuccessful()) {
            System.exit(1);
        }
    }
}
