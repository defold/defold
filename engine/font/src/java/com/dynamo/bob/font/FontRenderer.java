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

import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererBeginBatch;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererCreate;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererDestroy;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererDecodeImage;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererFreeGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererFreeImage;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererFreeTexture;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererGenerateGlyph;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererGenerateTexture;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererGetVertexBufferSize;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererGetVertices;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererHash;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererMeasure;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererSetProperties;
import static com.dynamo.bob.font.generated.FontRendererFFM.FontRendererSetText;

import java.io.File;
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.ref.Cleaner;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import com.dynamo.bob.font.generated.FontRendererFFM;
import com.dynamo.bob.font.generated.FontRendererGlyph;
import com.dynamo.bob.font.generated.FontRendererImage;
import com.dynamo.bob.font.generated.FontRendererLayout;
import com.dynamo.bob.font.generated.FontRendererParams;
import com.dynamo.bob.font.generated.FontRendererProperties;
import com.dynamo.bob.font.generated.FontTexture;

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
        public boolean outputBitmap;
        public boolean antialias = true;
        public boolean hasOutline;
        public boolean hasShadow;
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
            width = FontRendererLayout.m_Width(values);
            height = FontRendererLayout.m_Height(values);
            lineCount = FontRendererLayout.m_LineCount(values);
            maxAscent = FontRendererLayout.m_MaxAscent(values);
            maxDescent = FontRendererLayout.m_MaxDescent(values);
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
            atlasVersion = FontTexture.m_AtlasVersion(values);
            x = FontTexture.m_X(values);
            y = FontTexture.m_Y(values);
            width = FontTexture.m_Width(values);
            height = FontTexture.m_Height(values);
            channels = FontTexture.m_Channels(values);
            int pixelCount = FontTexture.m_PixelCount(values);
            pixels = pixelCount == 0 ? null : copyNativeBytes(FontTexture.m_Pixels(values), pixelCount);
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

    public static final class DecodedImage {
        public final int width;
        public final int height;
        public final int channels;
        public final ByteBuffer pixels;

        private DecodedImage(MemorySegment values) {
            width = FontRendererImage.m_Width(values);
            height = FontRendererImage.m_Height(values);
            channels = FontRendererImage.m_Channels(values);
            pixels = copyNativeBytes(FontRendererImage.m_Pixels(values), FontRendererImage.m_PixelCount(values));
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

    /** Decodes an image through the engine image library for Bob's bitmap-font compiler. */
    public static DecodedImage decodeImage(byte[] imageBytes) {
        if (imageBytes == null)
            throw new NullPointerException();
        if (imageBytes.length == 0)
            throw new IllegalArgumentException("Empty image");
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment result = FontRendererImage.allocate(arena);
            checkResult(FontRendererDecodeImage(arena.allocateFrom(JAVA_BYTE, imageBytes), imageBytes.length, result),
                    "Native image decoding failed");
            try {
                return new DecodedImage(result);
            } finally {
                FontRendererFreeImage(result);
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
            MemorySegment values = FontRendererProperties.allocate(arena);
            FontRendererProperties.m_FaceColor(values, arena.allocateFrom(JAVA_FLOAT, properties.faceColor));
            FontRendererProperties.m_OutlineColor(values, arena.allocateFrom(JAVA_FLOAT, properties.outlineColor));
            FontRendererProperties.m_ShadowColor(values, arena.allocateFrom(JAVA_FLOAT, properties.shadowColor));
            FontRendererProperties.m_Width(values, properties.width);
            FontRendererProperties.m_Height(values, properties.height);
            FontRendererProperties.m_Leading(values, properties.leading);
            FontRendererProperties.m_Tracking(values, properties.tracking);
            FontRendererProperties.m_SdfScale(values, properties.sdfScale);
            FontRendererProperties.m_LineBreak(values, flag(properties.lineBreak));
            FontRendererProperties.m_Align(values, properties.align);
            FontRendererProperties.m_VerticalAlign(values, properties.verticalAlign);
            checkResult(FontRendererSetProperties(requireHandle(), values), "Unable to set native font renderer properties");
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
            checkResult(FontRendererSetText(requireHandle(), arena.allocateFrom(JAVA_INT, codepoints), codepoints.length),
                    "Unable to set native font renderer text");
        }
    }

    /**
     * Returns a stable hash of the retained properties and text.
     *
     * <p>Exposed as Editor API for clients that cache output derived from the retained state.</p>
     */
    public synchronized long hash() {
        return FontRendererHash(requireHandle());
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
        checkResult(FontRendererBeginBatch(requireHandle()), "Unable to begin native font render batch");
    }

    /**
     * Shapes the retained text, populates the glyph atlas, and returns any required texture update.
     *
     * <p>Used by the Editor before requesting vertices so atlas coordinates remain stable.</p>
     */
    public synchronized Texture generateTexture(long knownAtlasVersion) {
        try (Arena arena = Arena.ofConfined()) {
            MemorySegment texture = FontTexture.allocate(arena);
            int status = FontRendererGenerateTexture(requireHandle(), knownAtlasVersion, texture);
            checkResult(status, "Native font texture generation failed");
            try {
                Texture result = new Texture(texture);
                atlasVersion = result.atlasVersion;
                return result;
            } finally {
                FontRendererFreeTexture(texture);
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
            int status = FontRendererGetVertexBufferSize(requireHandle(), vertexCount, vertexBufferSize);
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
            int status = FontRendererGetVertices(requireHandle(), arena.allocateFrom(JAVA_FLOAT, worldTransform),
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
        FontRendererParams.m_OutputBitmap(values, flag(params.outputBitmap));
        FontRendererParams.m_Antialias(values, flag(params.antialias));
        FontRendererParams.m_HasOutline(values, flag(params.hasOutline));
        FontRendererParams.m_HasShadow(values, flag(params.hasShadow));
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
