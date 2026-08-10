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

import static com.dynamo.bob.font.generated.FontRendererFFM.FontcBeginBatch;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcCreate;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcDestroy;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcDecodeImage;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcFreeGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcFreeImage;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcFreeTexture;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGenerateGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGenerateTexture;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGetGlyphMetrics;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGetSupportedGlyphMetrics;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGetVertexBufferSize;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcGetVertices;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcHash;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcMeasure;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcSetProperties;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontcSetText;

import java.io.File;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.ref.Cleaner;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.dynamo.bob.font.generated.FontRendererFFM;
import com.dynamo.bob.font.generated.FontcGlyph;
import com.dynamo.bob.font.generated.FontcGlyphMetrics;
import com.dynamo.bob.font.generated.FontcImage;
import com.dynamo.bob.font.generated.FontcLayout;
import com.dynamo.bob.font.generated.FontcParams;
import com.dynamo.bob.font.generated.FontcProperties;
import com.dynamo.bob.font.generated.FontcTexture;

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
    private static final Object NATIVE_SESSION_LIFECYCLE_LOCK = new Object();
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
        public boolean outputBitmap;
        public boolean antialias = true;
        public boolean hasOutline;
        public boolean hasShadow;
        public boolean useTextShaping;
    }

    public static final class Properties {
        public boolean lineBreak;
        public float width;
        public float height;
        public float leading;
        public float tracking;
        public int align;
        public int verticalAlign;
        public float[] faceColor;
        public float[] outlineColor;
        public float[] shadowColor;
        public float sdfScale;
    }

    public static final class Layout {
        public final float width;
        public final float height;
        public final int lineCount;
        public final float maxAscent;
        public final float maxDescent;

        private Layout(MemorySegment values) {
            width = FontcLayout.m_Width(values);
            height = FontcLayout.m_Height(values);
            lineCount = FontcLayout.m_LineCount(values);
            maxAscent = FontcLayout.m_MaxAscent(values);
            maxDescent = FontcLayout.m_MaxDescent(values);
        }
    }

    public static final class Texture {
        public final long atlasVersion;
        public final int x;
        public final int y;
        public final int width;
        public final int height;
        public final int channels;
        public final ByteBuffer pixels;

        private Texture(MemorySegment values) {
            atlasVersion = FontcTexture.m_AtlasVersion(values);
            x = FontcTexture.m_X(values);
            y = FontcTexture.m_Y(values);
            width = FontcTexture.m_Width(values);
            height = FontcTexture.m_Height(values);
            channels = FontcTexture.m_Channels(values);
            int pixelCount = FontcTexture.m_PixelCount(values);
            pixels = pixelCount == 0 ? null : copyNativeBytes(FontcTexture.m_Pixels(values), pixelCount);
        }
    }

    /** Caller-owned allocation requirements for the retained text's vertices. */
    public static final class VertexBufferRequirements {
        public final int vertexCount;
        public final int byteCount;

        private VertexBufferRequirements(int vertexCount, int byteCount) {
            this.vertexCount = vertexCount;
            this.byteCount = byteCount;
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
            glyphIndex = FontcGlyph.m_GlyphIndex(values);
            width = FontcGlyph.m_Width(values);
            height = FontcGlyph.m_Height(values);
            channels = FontcGlyph.m_Channels(values);
            advance = FontcGlyph.m_Advance(values);
            leftBearing = FontcGlyph.m_LeftBearing(values);
            ascent = FontcGlyph.m_Ascent(values);
            descent = FontcGlyph.m_Descent(values);
            pixels = copyNativeBytes(FontcGlyph.m_Pixels(values), FontcGlyph.m_PixelCount(values));
        }
    }

    /** Copied metrics for a Unicode codepoint supported by the loaded font. */
    public static final class GlyphMetrics {
        public final int codepoint;
        public final int glyphIndex;
        public final int width;
        public final int height;
        public final float advance;
        public final float leftBearing;
        public final float ascent;
        public final float descent;

        private GlyphMetrics(MemorySegment values) {
            codepoint = FontcGlyphMetrics.m_Codepoint(values);
            glyphIndex = FontcGlyphMetrics.m_GlyphIndex(values);
            width = FontcGlyphMetrics.m_Width(values);
            height = FontcGlyphMetrics.m_Height(values);
            advance = FontcGlyphMetrics.m_Advance(values);
            leftBearing = FontcGlyphMetrics.m_LeftBearing(values);
            ascent = FontcGlyphMetrics.m_Ascent(values);
            descent = FontcGlyphMetrics.m_Descent(values);
        }
    }

    public static final class DecodedImage {
        public final int width;
        public final int height;
        public final int channels;
        public final ByteBuffer pixels;

        private DecodedImage(MemorySegment values) {
            width = FontcImage.m_Width(values);
            height = FontcImage.m_Height(values);
            channels = FontcImage.m_Channels(values);
            pixels = copyNativeBytes(FontcImage.m_Pixels(values), FontcImage.m_PixelCount(values));
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
                synchronized (NATIVE_SESSION_LIFECYCLE_LOCK) {
                    FontcDestroy(handle);
                }
                handle = MemorySegment.NULL;
            }
        }
    }

    private final State state;
    private final Cleaner.Cleanable cleanable;
    private long atlasVersion;

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
            MemorySegment nativeParams = FontcParams.allocate(arena);
            writeParams(nativeParams, params);
            MemorySegment handlePointer = arena.allocate(FontRendererFFM.HFontRenderer);
            int result;
            synchronized (NATIVE_SESSION_LIFECYCLE_LOCK) {
                result = FontcCreate(arena.allocateFrom(name),
                        arena.allocateFrom(JAVA_BYTE, fontBytes), fontBytes.length, nativeParams, handlePointer);
            }
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
            MemorySegment result = FontcLayout.allocate(arena);
            int status = FontcMeasure(requireHandle(), arena.allocateFrom(JAVA_INT, codepoints),
                    codepoints.length, flag(lineBreak), width, leading, tracking, result);
            checkResult(status, "Native text shaping failed");
            return new Layout(result);
        }
    }

    /**
     * Generates a glyph bitmap and metrics using the same native font implementation as the engine runtime.
     *
     * <p>Used by Bob when compiling glyph banks. The Editor uses {@link #generateTexture} and
     * {@link #getVertices} instead.</p>
     *
     * @param codepoint Unicode codepoint to generate
     * @return copied glyph bitmap and metrics
     * @throws IllegalArgumentException if {@code codepoint} is not a valid Unicode codepoint
     */
    public synchronized GeneratedGlyph generateGlyph(int codepoint) {
        if (!Character.isValidCodePoint(codepoint))
            throw new IllegalArgumentException("Invalid Unicode codepoint");
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontcGlyph.allocate(arena);
            int status = FontcGenerateGlyph(requireHandle(), codepoint, result);
            checkResult(status, "Native glyph generation failed");
            try {
                return new GeneratedGlyph(result);
            } finally {
                FontcFreeGlyph(result);
            }
        }
    }

    /** Returns metrics for one Unicode codepoint without generating its glyph image. */
    synchronized GlyphMetrics getGlyphMetrics(int codepoint) {
        if (!Character.isValidCodePoint(codepoint))
            throw new IllegalArgumentException("Invalid Unicode codepoint");
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontcGlyphMetrics.allocate(arena);
            checkResult(FontcGetGlyphMetrics(requireHandle(), codepoint, result),
                    "Unable to get native glyph metrics");
            return new GlyphMetrics(result);
        }
    }

    /**
     * Returns metrics for every Unicode codepoint supported by the loaded font.
     *
     * <p>The native implementation enumerates the font cmap once and does not generate
     * glyph images. Bob uses the codepoints when compiling {@code all_chars}; the Editor
     * also uses the returned metrics directly for metadata-only compilation.</p>
     */
    public synchronized GlyphMetrics[] getSupportedGlyphMetrics() {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment glyphCount = arena.allocate(JAVA_INT);
            checkResult(FontcGetSupportedGlyphMetrics(requireHandle(), MemorySegment.NULL, 0, glyphCount),
                    "Unable to query supported native glyph metrics");
            int capacity = glyphCount.get(JAVA_INT, 0);
            if (capacity < 0)
                throw new IllegalStateException("Native supported glyph count exceeds the Java array size limit");

            MemorySegment nativeMetrics = FontcGlyphMetrics.allocateArray(capacity, arena);
            checkResult(FontcGetSupportedGlyphMetrics(requireHandle(), nativeMetrics, capacity, glyphCount),
                    "Unable to get supported native glyph metrics");
            int count = glyphCount.get(JAVA_INT, 0);
            if (count < 0 || count > capacity)
                throw new IllegalStateException("Invalid native supported glyph count");

            GlyphMetrics[] metrics = new GlyphMetrics[count];
            for (int i = 0; i < count; ++i)
                metrics[i] = new GlyphMetrics(FontcGlyphMetrics.asSlice(nativeMetrics, i));
            return metrics;
        }
    }

    /** Decodes an image through the engine image library for Bob's bitmap-font compiler. */
    public static DecodedImage decodeImage(byte[] imageBytes) {
        if (imageBytes == null)
            throw new NullPointerException();
        if (imageBytes.length == 0)
            throw new IllegalArgumentException("Empty image");
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontcImage.allocate(arena);
            checkResult(FontcDecodeImage(arena.allocateFrom(JAVA_BYTE, imageBytes), imageBytes.length, result),
                    "Native image decoding failed");
            try {
                return new DecodedImage(result);
            } finally {
                FontcFreeImage(result);
            }
        }
    }

    /**
     * Retains layout and rendering properties for subsequent texture and vertex generation.
     *
     * <p>Used by the Editor before generating each distance-field text entry.</p>
     */
    public synchronized void setProperties(Properties properties) {
        if (properties == null || properties.faceColor == null ||
                properties.outlineColor == null || properties.shadowColor == null)
            throw new NullPointerException();
        if (properties.faceColor.length != 4 || properties.outlineColor.length != 4 || properties.shadowColor.length != 4)
            throw new IllegalArgumentException("Invalid color array size");

        try (Arena arena = Arena.ofConfined()) {
            MemorySegment values = FontcProperties.allocate(arena);
            FontcProperties.m_FaceColor(values, arena.allocateFrom(JAVA_FLOAT, properties.faceColor));
            FontcProperties.m_OutlineColor(values, arena.allocateFrom(JAVA_FLOAT, properties.outlineColor));
            FontcProperties.m_ShadowColor(values, arena.allocateFrom(JAVA_FLOAT, properties.shadowColor));
            FontcProperties.m_Width(values, properties.width);
            FontcProperties.m_Height(values, properties.height);
            FontcProperties.m_Leading(values, properties.leading);
            FontcProperties.m_Tracking(values, properties.tracking);
            FontcProperties.m_SdfScale(values, properties.sdfScale);
            FontcProperties.m_LineBreak(values, flag(properties.lineBreak));
            FontcProperties.m_Align(values, properties.align);
            FontcProperties.m_VerticalAlign(values, properties.verticalAlign);
            checkResult(FontcSetProperties(requireHandle(), values), "Unable to set native font renderer properties");
        }
    }

    /**
     * Retains the text used by subsequent texture and vertex generation.
     *
     * <p>Used by the Editor when preparing each distance-field text entry.</p>
     *
     * @param text text to shape and render
     */
    public synchronized void setText(String text) {
        if (text == null)
            throw new NullPointerException("text");
        int[] codepoints = text.codePoints().toArray();
        try (Arena arena = Arena.ofConfined()) {
            checkResult(FontcSetText(requireHandle(), arena.allocateFrom(JAVA_INT, codepoints), codepoints.length),
                    "Unable to set native font renderer text");
        }
    }

    /**
     * Returns a stable hash of the retained properties and text.
     *
     * <p>Exposed as Editor API for clients that cache output derived from the retained state.</p>
     */
    public synchronized long hash() {
        return FontcHash(requireHandle());
    }

    /**
     * Returns the atlas version observed during the latest texture generation.
     *
     * <p>Used by the Editor to skip unchanged texture-generation batches.</p>
     */
    public synchronized long atlasVersion() {
        requireHandle();
        return atlasVersion;
    }

    /**
     * Starts a render batch and protects glyphs used in it from atlas eviction.
     *
     * <p>Used by the Editor before generating textures for all text entries in a render batch.</p>
     */
    public synchronized void beginBatch() {
        checkResult(FontcBeginBatch(requireHandle()), "Unable to begin native font render batch");
    }

    /**
     * Shapes the retained text, populates the glyph atlas, and returns any required texture update.
     *
     * <p>Used by the Editor before requesting vertices so atlas coordinates remain stable.</p>
     */
    public synchronized Texture generateTexture(long knownAtlasVersion) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment texture = FontcTexture.allocate(arena);
            int status = FontcGenerateTexture(requireHandle(), knownAtlasVersion, texture);
            checkResult(status, "Native font texture generation failed");
            try {
                Texture result = new Texture(texture);
                atlasVersion = result.atlasVersion;
                return result;
            } finally {
                FontcFreeTexture(texture);
            }
        }
    }

    /**
     * Calculates the memory required for the retained text's native vertices.
     *
     * <p>Used by the Editor to preallocate a vertex buffer for the complete render batch.</p>
     * The returned size remains valid when only vertex attributes or transforms change.
     */
    public synchronized VertexBufferRequirements getVertexBufferRequirements() {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment vertexCount = arena.allocate(JAVA_INT);
            MemorySegment vertexBufferSize = arena.allocate(JAVA_INT);
            int status = FontcGetVertexBufferSize(requireHandle(), vertexCount, vertexBufferSize);
            checkResult(status, "Unable to calculate native font vertex buffer size");

            int count = vertexCount.get(JAVA_INT, 0);
            long unsignedBufferSize = Integer.toUnsignedLong(vertexBufferSize.get(JAVA_INT, 0));
            if (unsignedBufferSize > Integer.MAX_VALUE)
                throw new IllegalStateException("Native font vertex buffer exceeds the Java buffer size limit");
            return new VertexBufferRequirements(count, (int)unsignedBufferSize);
        }
    }

    /**
     * Writes native vertices directly into a caller-provided buffer at its current position.
     *
     * <p>Used by the Editor after preallocating a vertex buffer for the complete render batch.</p>
     *
     * @param worldTransform column-major 4-by-4 transform applied by native code during vertex generation
     * @param vertexBuffer direct destination buffer
     * @param requirements required size previously returned by {@link #getVertexBufferRequirements()}
     */
    public synchronized void getVertices(float[] worldTransform, ByteBuffer vertexBuffer, VertexBufferRequirements requirements) {
        if (worldTransform == null || vertexBuffer == null || requirements == null)
            throw new NullPointerException();
        if (worldTransform.length != 16)
            throw new IllegalArgumentException("Invalid world transform array size");
        if (!vertexBuffer.isDirect())
            throw new IllegalArgumentException("Native font vertex buffer must be direct");
        if (vertexBuffer.remaining() < requirements.byteCount)
            throw new IllegalArgumentException("Native font vertex buffer is too small");

        try (Arena arena = Arena.ofConfined()) {
            int status = FontcGetVertices(requireHandle(), arena.allocateFrom(JAVA_FLOAT, worldTransform),
                    MemorySegment.ofBuffer(vertexBuffer), requirements.byteCount);
            checkResult(status, "Native font vertex generation failed");
            vertexBuffer.position(vertexBuffer.position() + requirements.byteCount);
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
        FontcParams.m_Size(values, params.size);
        FontcParams.m_AtlasWidth(values, params.cacheWidth);
        FontcParams.m_AtlasHeight(values, params.cacheHeight);
        FontcParams.m_CellPadding(values, params.cacheCellPadding);
        FontcParams.m_SdfBasePadding(values, params.sdfBasePadding);
        FontcParams.m_SdfEdgeValue(values, params.sdfEdgeValue);
        FontcParams.m_SdfSpread(values, params.sdfSpread);
        FontcParams.m_SdfOutline(values, params.sdfOutline);
        FontcParams.m_SdfShadow(values, params.sdfShadow);
        FontcParams.m_OutlineWidth(values, params.outlineWidth);
        FontcParams.m_ShadowBlur(values, params.shadowBlur);
        FontcParams.m_ShadowX(values, params.shadowX);
        FontcParams.m_ShadowY(values, params.shadowY);
        FontcParams.m_LayerMask(values, params.layerMask);
        FontcParams.m_OutputBitmap(values, flag(params.outputBitmap));
        FontcParams.m_Antialias(values, flag(params.antialias));
        FontcParams.m_HasOutline(values, flag(params.hasOutline));
        FontcParams.m_HasShadow(values, flag(params.hasShadow));
        FontcParams.m_UseTextShaping(values, flag(params.useTextShaping));
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
