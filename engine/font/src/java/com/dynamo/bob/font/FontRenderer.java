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

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;

import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererCreate;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererDestroy;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererFreeGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererFreeRenderResult;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererGenerateGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererMeasure;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererRender;

import java.io.File;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.ref.Cleaner;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.dynamo.bob.font.generated.FontRendererFFM;
import com.dynamo.bob.font.generated.FontRendererGlyph;
import com.dynamo.bob.font.generated.FontRendererLayout;
import com.dynamo.bob.font.generated.FontRendererParams;
import com.dynamo.bob.font.generated.FontRendererRenderResult;

/**
 * Java 25 FFM wrapper around the native Defold font renderer.
 *
 * <p>Used by Bob when compiling font resources and by the Editor when measuring
 * and rendering distance-field font previews.</p>
 */
public final class FontRenderer implements AutoCloseable {
    public static final int LAYER_FACE = FontRendererFFM.FONT_RENDERER_LAYER_FACE();
    public static final int LAYER_OUTLINE = FontRendererFFM.FONT_RENDERER_LAYER_OUTLINE();
    public static final int LAYER_SHADOW = FontRendererFFM.FONT_RENDERER_LAYER_SHADOW();
    public static final float DEFAULT_SDF_BASE_PADDING = 3.0f;
    public static final int DEFAULT_SDF_EDGE_VALUE = 191;

    private static final Cleaner CLEANER = Cleaner.create();
    private static final String LIBRARY_NAME = "fontc_shared";

    static {
        boolean loaded = false;
        ClassLoader classLoader = FontRenderer.class.getClassLoader();
        try {
            Class<?> bob = classLoader.loadClass("com.dynamo.bob.Bob");
            Method getSharedLib = bob.getMethod("getSharedLib", String.class);
            File library = (File)getSharedLib.invoke(null, LIBRARY_NAME);
            System.load(library.getAbsolutePath());
            loaded = true;
        } catch (ReflectiveOperationException | RuntimeException | LinkageError ignored) {
        }
        if (!loaded)
            System.loadLibrary(LIBRARY_NAME);
    }

    public static final class Params {
        public float size;
        public int cacheWidth;
        public int cacheHeight;
        public int cacheCellPadding = 1;
        public float sdfBasePadding = DEFAULT_SDF_BASE_PADDING;
        public int sdfEdgeValue = DEFAULT_SDF_EDGE_VALUE;
        public float sdfSpread = 1.0f;
        public float sdfOutline;
        public float sdfShadow;
        public float outlineWidth;
        public float shadowBlur;
        public float shadowX;
        public float shadowY;
        public int layerMask = LAYER_FACE;
    }

    public static final class Layout {
        public final float width;
        public final float height;
        public final int lineCount;
        public final float maxAscent;
        public final float maxDescent;

        private Layout(MemorySegment values) {
            width = FontRendererLayout.m_Width(values);
            height = FontRendererLayout.m_Height(values);
            lineCount = FontRendererLayout.m_LineCount(values);
            maxAscent = FontRendererLayout.m_MaxAscent(values);
            maxDescent = FontRendererLayout.m_MaxDescent(values);
        }
    }

    public static final class TextureUpdate {
        public final int x;
        public final int y;
        public final int width;
        public final int height;
        public final int channels;
        public final ByteBuffer pixels;

        private TextureUpdate(MemorySegment values) {
            x = FontRendererRenderResult.m_TextureX(values);
            y = FontRendererRenderResult.m_TextureY(values);
            width = FontRendererRenderResult.m_TextureWidth(values);
            height = FontRendererRenderResult.m_TextureHeight(values);
            channels = FontRendererRenderResult.m_TextureChannels(values);
            pixels = copyNativeBytes(FontRendererRenderResult.m_TexturePixels(values),
                    FontRendererRenderResult.m_TexturePixelCount(values));
        }
    }

    public static final class RenderResult {
        public final ByteBuffer vertices;
        public final int vertexCount;
        public final long atlasVersion;
        public final TextureUpdate textureUpdate;

        private RenderResult(MemorySegment values) {
            vertices = copyNativeBytes(FontRendererRenderResult.m_Vertices(values),
                    FontRendererRenderResult.m_VertexByteCount(values));
            vertexCount = FontRendererRenderResult.m_VertexCount(values);
            atlasVersion = FontRendererRenderResult.m_AtlasVersion(values);
            textureUpdate = FontRendererRenderResult.m_HasTextureUpdate(values) == 0 ? null : new TextureUpdate(values);
        }
    }

    /** A copied glyph image and the metrics produced by the engine font library. */
    public static final class GeneratedGlyph {
        public final int glyphIndex;
        public final int width;
        public final int height;
        public final int channels;
        public final float advance;
        public final float leftBearing;
        public final float ascent;
        public final float descent;
        public final ByteBuffer pixels;

        private GeneratedGlyph(MemorySegment values) {
            glyphIndex = FontRendererGlyph.m_GlyphIndex(values);
            width = FontRendererGlyph.m_Width(values);
            height = FontRendererGlyph.m_Height(values);
            channels = FontRendererGlyph.m_Channels(values);
            advance = FontRendererGlyph.m_Advance(values);
            leftBearing = FontRendererGlyph.m_LeftBearing(values);
            ascent = FontRendererGlyph.m_Ascent(values);
            descent = FontRendererGlyph.m_Descent(values);
            pixels = copyNativeBytes(FontRendererGlyph.m_Pixels(values), FontRendererGlyph.m_PixelCount(values));
        }
    }

    private static final class State implements Runnable {
        private MemorySegment handle;

        private State(MemorySegment handle) {
            this.handle = handle;
        }

        @Override
        public void run() {
            if (!handle.equals(MemorySegment.NULL)) {
                FontRendererDestroy(handle);
                handle = MemorySegment.NULL;
            }
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;

    /**
     * Creates a native renderer and its glyph atlas from the supplied font data.
     *
     * <p>Used by Bob and the Editor.</p>
     *
     * @param name name used to identify the font in native error messages
     * @param fontBytes complete contents of the font file
     * @param params font size, atlas dimensions, and rendering parameters
     * @throws NullPointerException if any argument is {@code null}
     * @throws IllegalArgumentException if the font data or parameters are invalid
     */
    public FontRenderer(String name, byte[] fontBytes, Params params) {
        if (name == null || fontBytes == null || params == null)
            throw new NullPointerException();
        if (fontBytes.length == 0 || params.size <= 0.0f || params.cacheWidth <= 0 || params.cacheHeight <= 0 ||
                params.cacheCellPadding < 0 || params.sdfBasePadding <= 0.0f || params.sdfSpread <= 0.0f ||
                params.sdfEdgeValue <= 0 || params.sdfEdgeValue > 255 ||
                (params.layerMask & ~(LAYER_FACE | LAYER_OUTLINE | LAYER_SHADOW)) != 0 ||
                (params.layerMask & LAYER_FACE) == 0)
            throw new IllegalArgumentException("Invalid native font renderer parameters");

        MemorySegment handle;
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment nativeParams = FontRendererParams.allocate(arena);
            writeParams(nativeParams, params);
            MemorySegment handlePointer = arena.allocate(FontRendererFFM.HFontRenderer);
            int result = FontRendererCreate(arena.allocateFrom(name),
                    arena.allocateFrom(JAVA_BYTE, fontBytes), fontBytes.length, nativeParams, handlePointer);
            if (result != FontRendererFFM.FONT_RENDERER_RESULT_OK())
                throw new IllegalArgumentException("Unable to create native font renderer for " + name +
                        " (native result " + result + ")");
            handle = handlePointer.get(ADDRESS, 0);
        }
        if (handle.equals(MemorySegment.NULL))
            throw new IllegalArgumentException("Unable to create native font renderer for " + name);
        state = new State(handle);
        cleanable = CLEANER.register(this, state);
    }

    /**
     * Shapes and measures text without including an empty line after a trailing newline.
     *
     * <p>Used by Bob to obtain font metrics and by the Editor for text layout.</p>
     *
     * @param text text to shape and measure
     * @param lineBreak whether the text may wrap at the supplied width
     * @param width maximum line width when line breaking is enabled
     * @param leading line-height multiplier
     * @param tracking additional spacing between glyphs
     * @return measured text layout
     */
    public synchronized Layout measure(String text, boolean lineBreak, float width, float leading, float tracking) {
        if (text == null)
            throw new NullPointerException("text");
        int[] codepoints = text.codePoints().toArray();
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontRendererLayout.allocate(arena);
            int status = FontRendererMeasure(requireHandle(), arena.allocateFrom(JAVA_INT, codepoints),
                    codepoints.length, flag(lineBreak), width, leading, tracking, result);
            checkResult(status, "Native text shaping failed");
            return new Layout(result);
        }
    }

    /**
     * Generates a glyph bitmap and metrics using the same native font implementation as the engine runtime.
     *
     * <p>Used by Bob when compiling glyph banks. The Editor renders complete text through {@link #render}
     * instead.</p>
     *
     * @param codepoint Unicode codepoint to generate
     * @return copied glyph bitmap and metrics
     * @throws IllegalArgumentException if {@code codepoint} is not a valid Unicode codepoint
     */
    public synchronized GeneratedGlyph generateGlyph(int codepoint) {
        if (!Character.isValidCodePoint(codepoint))
            throw new IllegalArgumentException("Invalid Unicode codepoint");
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontRendererGlyph.allocate(arena);
            int status = FontRendererGenerateGlyph(requireHandle(), codepoint, result);
            checkResult(status, "Native glyph generation failed");
            try {
                return new GeneratedGlyph(result);
            } finally {
                FontRendererFreeGlyph(result);
            }
        }
    }

    /**
     * Shapes text and produces render vertices and any required atlas update.
     *
     * <p>Used by the Editor for distance-field font previews. Bob generates individual glyphs through
     * {@link #generateGlyph}.</p>
     *
     * @param text text to render
     * @param lineBreak whether the text may wrap at the supplied width
     * @param width render-box width
     * @param height render-box height
     * @param leading line-height multiplier
     * @param tracking additional spacing between glyphs
     * @param align horizontal text alignment value
     * @param verticalAlign vertical text alignment value
     * @param transform column-major 4-by-4 transform matrix
     * @param faceColor face color as four floats
     * @param outlineColor outline color as four floats
     * @param shadowColor shadow color as four floats
     * @param sdfScale distance-field scale
     * @param knownAtlasVersion atlas version already held by the caller
     * @return copied vertex data, atlas version, and optional texture update
     */
    public synchronized RenderResult render(String text, boolean lineBreak, float width, float height,
                                            float leading, float tracking, int align, int verticalAlign,
                                            float[] transform, float[] faceColor, float[] outlineColor,
                                            float[] shadowColor, float sdfScale, long knownAtlasVersion) {
        if (text == null || transform == null || faceColor == null || outlineColor == null || shadowColor == null)
            throw new NullPointerException();
        if (transform.length != 16 || faceColor.length != 4 || outlineColor.length != 4 || shadowColor.length != 4)
            throw new IllegalArgumentException("Invalid transform or color array size");
        int[] codepoints = text.codePoints().toArray();
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontRendererRenderResult.allocate(arena);
            int status = FontRendererRender(requireHandle(), arena.allocateFrom(JAVA_INT, codepoints),
                    codepoints.length, flag(lineBreak), width, height, leading, tracking,
                    align, verticalAlign, arena.allocateFrom(JAVA_FLOAT, transform), arena.allocateFrom(JAVA_FLOAT, faceColor),
                    arena.allocateFrom(JAVA_FLOAT, outlineColor), arena.allocateFrom(JAVA_FLOAT, shadowColor),
                    sdfScale, knownAtlasVersion, result);
            checkResult(status, "Native font rendering failed");
            try {
                return new RenderResult(result);
            } finally {
                FontRendererFreeRenderResult(result);
            }
        }
    }

    /**
     * Releases the native renderer and its atlas.
     *
     * <p>Used directly by Bob through try-with-resources. Editor instances also retain a Cleaner fallback.</p>
     */
    @Override
    public synchronized void close() {
        cleanable.clean();
    }

    private MemorySegment requireHandle() {
        if (state.handle.equals(MemorySegment.NULL))
            throw new IllegalStateException("Native font renderer is closed");
        return state.handle;
    }

    private static void writeParams(MemorySegment values, Params params) {
        FontRendererParams.m_Size(values, params.size);
        FontRendererParams.m_AtlasWidth(values, params.cacheWidth);
        FontRendererParams.m_AtlasHeight(values, params.cacheHeight);
        FontRendererParams.m_CellPadding(values, params.cacheCellPadding);
        FontRendererParams.m_SdfBasePadding(values, params.sdfBasePadding);
        FontRendererParams.m_SdfEdgeValue(values, params.sdfEdgeValue);
        FontRendererParams.m_SdfSpread(values, params.sdfSpread);
        FontRendererParams.m_SdfOutline(values, params.sdfOutline);
        FontRendererParams.m_SdfShadow(values, params.sdfShadow);
        FontRendererParams.m_OutlineWidth(values, params.outlineWidth);
        FontRendererParams.m_ShadowBlur(values, params.shadowBlur);
        FontRendererParams.m_ShadowX(values, params.shadowX);
        FontRendererParams.m_ShadowY(values, params.shadowY);
        FontRendererParams.m_LayerMask(values, params.layerMask);
    }

    private static int flag(boolean value) {
        return value ? 1 : 0;
    }

    private static void checkResult(int result, String message) {
        if (result != FontRendererFFM.FONT_RENDERER_RESULT_OK())
            throw new IllegalStateException(message + " (native result " + result + ")");
    }

    private static ByteBuffer copyNativeBytes(MemorySegment address, int byteCount) {
        ByteBuffer copy = ByteBuffer.allocateDirect(byteCount).order(ByteOrder.nativeOrder());
        if (byteCount != 0)
            copy.put(address.reinterpret(Integer.toUnsignedLong(byteCount)).asByteBuffer());
        return copy.flip();
    }
}
