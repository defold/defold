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

import java.io.ByteArrayOutputStream;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Set;
import java.util.TreeSet;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionException;
import java.util.concurrent.ForkJoinPool;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.font.BMFont.BMFontFormatException;
import com.dynamo.bob.font.BMFont.Char;
import com.dynamo.bob.font.FontRenderer.DecodedImage;
import com.dynamo.bob.font.FontRenderer.GeneratedGlyph;
import com.dynamo.bob.font.FontRenderer.GlyphMetrics;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.pipeline.Texc;
import com.dynamo.bob.pipeline.TexcLibraryJni;
import com.dynamo.bob.pipeline.TextureGeneratorException;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.StringUtil;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Font.FontMap;
import com.dynamo.render.proto.Font.FontRenderMode;
import com.dynamo.render.proto.Font.FontTextureFormat;
import com.dynamo.render.proto.Font.GlyphBank;
import com.google.protobuf.ByteString;
import com.google.protobuf.TextFormat;
import org.apache.commons.lang3.StringUtils;

public class Fontc {
    public static final char[] ASCII_7BIT;
    static {
        int start = 32;
        int end = 126;
        ASCII_7BIT = new char[end - start + 1];
        for (int i = start; i <= end; ++i)
            ASCII_7BIT[i - start] = (char)i;
    }

    /** Font metadata and optional uncompressed glyph output returned by {@link #compileForEditor} for Editor previews. */
    public static final class EditorFontMap {
        public final FontMap fontMap;
        public final GlyphBank glyphBank;
        public final int[] glyphCellWidths;
        public final int[] glyphCellHeights;

        private EditorFontMap(FontMap fontMap, GlyphBank glyphBank, int[] glyphCellWidths, int[] glyphCellHeights) {
            this.fontMap = fontMap;
            this.glyphBank = glyphBank;
            this.glyphCellWidths = glyphCellWidths;
            this.glyphCellHeights = glyphCellHeights;
        }
    }

    private static final class Glyph {
        int character;
        int width;
        float advance;
        float leftBearing;
        int ascent;
        int descent;
        int sourceX;
        int sourceY;
        int dataOffset;
        int dataSize;
        int pixelHeight;
        int pixelChannels;
        ByteBuffer pixels;
    }

    static final float sdfEdge = 0.75f;
    static final int LAYER_FACE = 0x1;
    static final int LAYER_OUTLINE = 0x2;
    static final int LAYER_SHADOW = 0x4;
    private static final int PARALLEL_GLYPH_COUNT = 256;

    private FontDesc fontDesc;
    private GlyphBank.Builder glyphBankBuilder;
    private ArrayList<Glyph> glyphs = new ArrayList<Glyph>();
    private BMFont bmfont;

    public Fontc() {
    }

    public GlyphBank getGlyphBank() {
        return glyphBankBuilder.build();
    }

    public static long FontDescToHash(FontDesc desc) {
        String result = "" + desc.getFont() + desc.getSize() + desc.getAntialias() + desc.getOutlineWidth() +
            desc.getShadowBlur() + desc.getCharacters() + desc.getOutputFormat() + desc.getAllChars() +
            desc.getCacheWidth() + desc.getCacheHeight() + desc.getRenderMode();
        return MurmurHash.hash64(result);
    }

    public static int GetFontMapLayerMask(FontDesc fontDesc) {
        int mask = LAYER_FACE;
        if (fontDesc.getRenderMode() == FontRenderMode.MODE_MULTI_LAYER) {
            if (fontDesc.getOutlineAlpha() > 0.0f && fontDesc.getOutlineWidth() > 0.0f)
                mask |= LAYER_OUTLINE;
            if (fontDesc.getShadowAlpha() > 0.0f && fontDesc.getAlpha() > 0.0f)
                mask |= LAYER_SHADOW;
        }
        return mask;
    }

    public static int GetFontMapPadding(FontDesc fontDesc) {
        if (isBitmapFont(fontDesc))
            return 0;
        return Math.round(getNativeSdfPadding(fontDesc));
    }

    public static float GetFontMapSdfSpread(FontDesc fontDesc) {
        return getNativeSdfPadding(fontDesc);
    }

    public static float GetFontMapSdfOutline(FontDesc fontDesc) {
        return calculateNativeSdfLimit(getNativeSdfPadding(fontDesc), fontDesc.getOutlineWidth());
    }

    public static float GetFontMapSdfShadow(FontDesc fontDesc) {
        if (fontDesc.getShadowBlur() == 0)
            return 1.0f;
        return calculateNativeSdfLimit(getNativeSdfPadding(fontDesc), fontDesc.getShadowBlur());
    }

    private static boolean isBitmapFont(FontDesc fontDesc) {
        return StringUtil.toLowerCase(fontDesc.getFont()).endsWith("fnt");
    }

    private static float getNativeSdfPadding(FontDesc fontDesc) {
        float shadowBlur = fontDesc.getShadowAlpha() > 0.0f ? fontDesc.getShadowBlur() : 0.0f;
        return FontRenderer.DEFAULT_SDF_BASE_PADDING + fontDesc.getOutlineWidth() + shadowBlur;
    }

    private static float calculateNativeSdfLimit(float padding, float width) {
        float baseEdge = sdfEdge * 255.0f;
        return (baseEdge - (FontRenderer.DEFAULT_SDF_EDGE_VALUE / padding) * width) / 255.0f;
    }

    private ArrayList<Integer> getRequestedCharacters() {
        ArrayList<Integer> characters = new ArrayList<Integer>();
        if (!fontDesc.getAllChars()) {
            String requested = fontDesc.getCharacters();
            if (!StringUtils.isEmpty(requested)) {
                requested.codePoints().forEach(characters::add);
            } else {
                for (char c : ASCII_7BIT)
                    characters.add((int)c);
                fontDesc.getExtraCharacters().codePoints().forEach(characters::add);
            }
            Set<Integer> sorted = new TreeSet<Integer>(characters);
            characters = new ArrayList<Integer>(sorted);
        }
        return characters;
    }

    private void buildBMFont(InputStream fontStream) throws IOException {
        bmfont = new BMFont();
        try {
            bmfont.parse(fontStream);
        } catch (BMFontFormatException e) {
            throw new IOException(e.getMessage(), e);
        }
        int maxAscent = 0;
        int maxDescent = 0;
        for (int i = 0; i < bmfont.charArray.size(); ++i) {
            Char source = bmfont.charArray.get(i);
            Glyph glyph = new Glyph();
            glyph.ascent = bmfont.base - (int)source.yoffset;
            glyph.descent = source.height - glyph.ascent;
            glyph.sourceX = source.x;
            glyph.sourceY = source.y;
            glyph.character = source.id;
            glyph.advance = source.xadvance;
            glyph.leftBearing = source.xoffset;
            glyph.width = source.width;
            glyph.pixelHeight = source.height;
            glyphs.add(glyph);
            maxAscent = Math.max(maxAscent, glyph.ascent);
            maxDescent = Math.max(maxDescent, glyph.descent);
        }
        glyphBankBuilder.setMaxAscent(maxAscent).setMaxDescent(maxDescent);
    }

    private GeneratedGlyph[] generateSupportedGlyphs(byte[] fontBytes, FontRenderer.Params params, GlyphMetrics[] metrics) throws IOException {
        GeneratedGlyph[] generatedGlyphs = new GeneratedGlyph[metrics.length];
        int workerCount = metrics.length < PARALLEL_GLYPH_COUNT
                          ? 1
                          : Math.min(metrics.length, Math.max(1, ForkJoinPool.getCommonPoolParallelism()));
        if (workerCount == 1) {
            try (FontRenderer renderer = new FontRenderer(fontDesc.getFont(), fontBytes, params)) {
                for (int i = 0; i < metrics.length; ++i)
                    generatedGlyphs[i] = renderer.generateGlyph(metrics[i].codepoint);
            } catch (RuntimeException e) {
                throw new IOException("Native glyph generation failed: " + e.getMessage(), e);
            }
            return generatedGlyphs;
        }

        CompletableFuture<?>[] tasks = new CompletableFuture<?>[workerCount];
        for (int workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            final int firstGlyphIndex = workerIndex;
            tasks[workerIndex] = CompletableFuture.runAsync(() -> {
                try (FontRenderer renderer = new FontRenderer(fontDesc.getFont(), fontBytes, params)) {
                    for (int glyphIndex = firstGlyphIndex; glyphIndex < metrics.length; glyphIndex += workerCount) {
                        int codePoint = metrics[glyphIndex].codepoint;
                        try {
                            generatedGlyphs[glyphIndex] = renderer.generateGlyph(codePoint);
                        } catch (RuntimeException e) {
                            throw new RuntimeException(String.format("Native glyph generation failed for U+%04X: %s", codePoint, e.getMessage()), e);
                        }
                    }
                }
            });
        }
        try {
            CompletableFuture.allOf(tasks).join();
        } catch (CompletionException e) {
            Throwable cause = e.getCause();
            throw new IOException(cause.getMessage(), cause);
        }
        return generatedGlyphs;
    }

    private void buildNativeTTF(FontRenderer renderer, boolean copyPixels, byte[] fontBytes, FontRenderer.Params params) throws IOException {
        ArrayList<Integer> characters = getRequestedCharacters();
        int nativeGlyphChannels = params.outputBitmap
                                  ? (params.hasOutline || params.hasShadow ? 3 : 1)
                                  : (params.shadowBlur > 0.0f ? 3 : 1);
        GlyphMetrics[] supportedMetrics;
        try {
            supportedMetrics = fontDesc.getAllChars() ? renderer.getSupportedGlyphMetrics() : null;
        } catch (RuntimeException e) {
            throw new IOException("Unable to enumerate supported native glyphs: " + e.getMessage(), e);
        }
        int count = supportedMetrics == null ? characters.size() : supportedMetrics.length;
        GeneratedGlyph[] generatedGlyphs = copyPixels && supportedMetrics != null
                                           ? generateSupportedGlyphs(fontBytes, params, supportedMetrics)
                                           : null;
        float maxAdvance = 0.0f;
        int maxWidth = 0;
        for (int i = 0; i < count; ++i) {
            GlyphMetrics metrics = supportedMetrics == null ? null : supportedMetrics[i];
            int codePoint = metrics == null ? characters.get(i) : metrics.codepoint;
            GeneratedGlyph generated = generatedGlyphs == null ? null : generatedGlyphs[i];
            try {
                if (generated == null && metrics == null) {
                    if (copyPixels)
                        generated = renderer.generateGlyph(codePoint);
                    else
                        metrics = renderer.getGlyphMetrics(codePoint);
                }
            } catch (RuntimeException e) {
                throw new IOException(String.format("Native glyph generation failed for U+%04X: %s", codePoint, e.getMessage()), e);
            }
            if ((generated != null && generated.glyphIndex == 0) || (metrics != null && metrics.glyphIndex == 0))
                continue;
            Glyph glyph = new Glyph();
            glyph.character = codePoint;
            glyph.advance = generated == null ? metrics.advance : generated.advance;
            glyph.leftBearing = generated == null ? metrics.leftBearing : generated.leftBearing;
            glyph.width = generated == null ? metrics.width : generated.width;
            glyph.ascent = Math.round(generated == null ? metrics.ascent : generated.ascent);
            glyph.descent = Math.round(generated == null ? metrics.descent : generated.descent);
            glyph.pixelHeight = generated == null ? metrics.height : generated.height;
            glyph.pixelChannels = generated == null ? nativeGlyphChannels : generated.channels;
            glyph.pixels = generated == null ? null : generated.pixels;
            if (!(glyph.width == 0 && glyph.advance == 0.0f && codePoint >= 65000))
                glyphs.add(glyph);
            maxAdvance = Math.max(maxAdvance, glyph.advance);
            maxWidth = Math.max(maxWidth, glyph.width);
        }
        FontRenderer.Layout metrics = renderer.measure("", false, 0.0f, 1.0f, 0.0f);
        glyphBankBuilder.setMaxAscent(metrics.maxAscent).setMaxDescent(metrics.maxDescent)
            .setMaxAdvance(maxAdvance).setMaxWidth(maxWidth).setMaxHeight(metrics.maxAscent + metrics.maxDescent);
    }

    private byte[] makeCellBytes(Glyph glyph, DecodedImage bitmapImage, int channels) {
        if (glyph.width == 0 || glyph.pixelHeight == 0)
            return new byte[0];
        int cellWidth = glyph.width + 2;
        int cellHeight = glyph.pixelHeight + 2;
        byte[] output = new byte[cellWidth * cellHeight * channels];
        ByteBuffer source = (bitmapImage == null ? glyph.pixels : bitmapImage.pixels).duplicate();
        int sourceWidth = bitmapImage == null ? glyph.width : bitmapImage.width;
        int sourceChannels = bitmapImage == null ? glyph.pixelChannels : bitmapImage.channels;
        int sourceX = bitmapImage == null ? 0 : glyph.sourceX;
        int sourceY = bitmapImage == null ? 0 : glyph.sourceY;
        for (int y = 0; y < glyph.pixelHeight; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                int sourceOffset = ((sourceY + y) * sourceWidth + sourceX + x) * sourceChannels;
                int targetOffset = ((y + 1) * cellWidth + x + 1) * channels;
                if (bitmapImage == null) {
                    for (int channel = 0; channel < channels; ++channel)
                        output[targetOffset + channel] = source.get(sourceOffset + channel);
                } else {
                    int red = source.get(sourceOffset) & 0xff;
                    int green = sourceChannels < 3 ? red : source.get(sourceOffset + 1) & 0xff;
                    int blue = sourceChannels < 3 ? red : source.get(sourceOffset + 2) & 0xff;
                    int alpha = sourceChannels == 2 ? source.get(sourceOffset + 1) & 0xff
                              : sourceChannels == 4 ? source.get(sourceOffset + 3) & 0xff
                              : 0xff;
                    output[targetOffset] = (byte)(red * alpha / 255);
                    output[targetOffset + 1] = (byte)(green * alpha / 255);
                    output[targetOffset + 2] = (byte)(blue * alpha / 255);
                    output[targetOffset + 3] = (byte)alpha;
                }
            }
        }
        return output;
    }

    private byte[] makeGlyphData(Glyph glyph, DecodedImage bitmapImage, int channels, boolean compressGlyphData) {
        byte[] uncompressed = makeCellBytes(glyph, bitmapImage, channels);
        if (!compressGlyphData || uncompressed.length == 0)
            return uncompressed;

        Texc.Buffer compressed = TexcLibraryJni.CompressBuffer(uncompressed);
        boolean useCompressed = compressed.isCompressed && compressed.data.length < uncompressed.length;
        byte[] payload = useCompressed ? compressed.data : uncompressed;
        byte[] output = new byte[payload.length + 1];
        output[0] = useCompressed ? (byte)1 : 0;
        System.arraycopy(payload, 0, output, 1, payload.length);
        return output;
    }

    private byte[][] makeGlyphData(DecodedImage bitmapImage, int channels, boolean compressGlyphData, int includeCount) throws IOException {
        byte[][] glyphData = new byte[includeCount][];
        int workerCount = bitmapImage != null || includeCount < PARALLEL_GLYPH_COUNT
                          ? 1
                          : Math.min(includeCount, Math.max(1, ForkJoinPool.getCommonPoolParallelism()));
        if (workerCount == 1) {
            for (int i = 0; i < includeCount; ++i)
                glyphData[i] = makeGlyphData(glyphs.get(i), bitmapImage, channels, compressGlyphData);
            return glyphData;
        }

        CompletableFuture<?>[] tasks = new CompletableFuture<?>[workerCount];
        for (int workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
            final int firstGlyphIndex = workerIndex;
            tasks[workerIndex] = CompletableFuture.runAsync(() -> {
                for (int glyphIndex = firstGlyphIndex; glyphIndex < includeCount; glyphIndex += workerCount)
                    glyphData[glyphIndex] = makeGlyphData(glyphs.get(glyphIndex), bitmapImage, channels, compressGlyphData);
            });
        }
        try {
            CompletableFuture.allOf(tasks).join();
        } catch (CompletionException e) {
            Throwable cause = e.getCause();
            throw new IOException("Failed to generate font texture: " + cause.getMessage(), cause);
        }
        return glyphData;
    }

    private void generateGlyphBank(boolean preview, String bitmapPath, InputStream bitmapStream, boolean compressGlyphData, boolean includeGlyphData) throws IOException, TextureGeneratorException {
        DecodedImage bitmapImage = null;
        int channels;
        if (bmfont != null) {
            String imageName = Paths.get(FilenameUtils.normalize(bmfont.page.get(0))).getFileName().toString();
            if (bitmapPath == null || bitmapStream == null)
                throw new IOException("Missing BMFont image resource: " + imageName);
            String providedImageName = Paths.get(FilenameUtils.normalize(bitmapPath)).getFileName().toString();
            if (!imageName.equals(providedImageName))
                throw new IOException("Expected BMFont image resource '" + imageName + "', got '" + bitmapPath + "'");
            bitmapImage = FontRenderer.decodeImage(bitmapStream.readAllBytes());
            channels = 4;
        } else {
            channels = 1;
            for (Glyph glyph : glyphs) {
                if (glyph.pixelChannels > 0) {
                    channels = glyph.pixelChannels;
                    break;
                }
            }
        }

        int cellWidth = 1;
        int maxAscent = 0;
        int maxDescent = 0;
        int cellMaxAscent = 0;
        for (Glyph glyph : glyphs) {
            cellWidth = Math.max(cellWidth, glyph.width + 2);
            maxAscent = Math.max(maxAscent, glyph.ascent);
            maxDescent = Math.max(maxDescent, glyph.descent);
            cellMaxAscent = Math.max(cellMaxAscent, glyph.ascent);
        }
        long cellHeightLong = Math.max(1L, (long)maxAscent + maxDescent + 2);
        if (cellHeightLong > Integer.MAX_VALUE)
            throw new IOException("Font cache cell height is too large");
        int cellHeight = (int)cellHeightLong;
        if (channels == 3)
            cellWidth = (cellWidth + 3) & ~3;

        int cacheWidth = fontDesc.getCacheWidth() > 0 ? fontDesc.getCacheWidth() : 1024;
        int columns = Math.max(1, cacheWidth / cellWidth);
        int cacheHeight = fontDesc.getCacheHeight();
        if (cacheHeight == 0) {
            int totalHeight = (int)Math.ceil((double)glyphs.size() / columns) * cellHeight;
            cacheHeight = Math.min(Integer.highestOneBit(Math.max(1, totalHeight - 1)) << 1, 2048);
        }
        int includeCount = preview ? Math.min(glyphs.size(), cacheHeight / cellHeight * columns) : glyphs.size();
        byte[][] generatedGlyphData = includeGlyphData
                                      ? makeGlyphData(bitmapImage, channels, compressGlyphData, includeCount)
                                      : null;
        int dataOffset = 0;
        for (int i = 0; i < includeCount; ++i) {
            Glyph glyph = glyphs.get(i);
            if (includeGlyphData) {
                byte[] bytes = generatedGlyphData[i];
                glyph.dataOffset = dataOffset;
                glyph.dataSize = bytes.length;
                dataOffset += glyph.dataSize;
            }
        }
        if (glyphs.isEmpty())
            throw new IOException("No character glyphs were included! Maybe turn on 'all_chars'?");

        ByteArrayOutputStream data = new ByteArrayOutputStream(dataOffset);
        if (includeGlyphData) {
            for (byte[] bytes : generatedGlyphData)
                data.writeBytes(bytes);
        }

        glyphBankBuilder.setGlyphPadding(1).setCacheWidth(cacheWidth).setCacheHeight(cacheHeight)
            .setGlyphData(ByteString.copyFrom(data.toByteArray())).setCacheCellWidth(cellWidth)
            .setCacheCellHeight(cellHeight).setGlyphChannels(channels).setCacheCellMaxAscent(cellMaxAscent);
        boolean monospaced = includeCount > 1;
        float advance = includeCount == 0 ? 0.0f : glyphs.get(0).advance;
        int padding = bmfont == null ? Math.round(getNativeSdfPadding(fontDesc)) : 0;
        for (int i = 0; i < includeCount; ++i) {
            Glyph glyph = glyphs.get(i);
            GlyphBank.Glyph.Builder output = GlyphBank.Glyph.newBuilder().setCharacter(glyph.character)
                .setWidth(glyph.width).setAdvance(glyph.advance).setLeftBearing(glyph.leftBearing)
                .setAscent(glyph.ascent).setDescent(glyph.descent)
                .setGlyphDataOffset(glyph.dataOffset).setGlyphDataSize(glyph.dataSize);
            if (preview)
                output.setX(i % columns * cellWidth).setY(i / columns * cellHeight);
            glyphBankBuilder.addGlyphs(output);
            monospaced &= advance == glyph.advance;
        }
        glyphBankBuilder.setIsMonospaced(monospaced).setPadding(padding);
    }

    /** Compiles a TrueType or OpenType font. The caller retains ownership of {@code fontStream}. */
    public void compile(InputStream fontStream, FontDesc fontDesc, boolean preview) throws TextureGeneratorException, IOException {
        compile(fontStream, fontDesc, preview, null, null, true, false);
    }

    /**
     * Compiles a font with an explicitly supplied BMFont image dependency.
     * The caller retains ownership of both streams.
     */
    public void compile(InputStream fontStream, FontDesc fontDesc, boolean preview, String bitmapPath, InputStream bitmapStream) throws TextureGeneratorException, IOException {
        compile(fontStream, fontDesc, preview, bitmapPath, bitmapStream, true, false);
    }

    private void compile(InputStream fontStream, FontDesc fontDesc, boolean preview, String bitmapPath, InputStream bitmapStream, boolean compressGlyphData, boolean metadataOnly) throws TextureGeneratorException, IOException {
        if ((bitmapPath == null) != (bitmapStream == null))
            throw new IllegalArgumentException("BMFont image path and stream must both be supplied");
        boolean bitmapFont = isBitmapFont(fontDesc);
        if (!bitmapFont && bitmapPath != null)
            throw new IllegalArgumentException("BMFont image input was supplied for a non-BMFont font");
        this.fontDesc = fontDesc;
        glyphs = new ArrayList<Glyph>();
        bmfont = null;
        glyphBankBuilder = GlyphBank.newBuilder().setImageFormat(fontDesc.getOutputFormat());
        if (bitmapFont) {
            buildBMFont(fontStream);
            generateGlyphBank(preview, bitmapPath, bitmapStream, compressGlyphData, true);
            return;
        }

        byte[] fontBytes = fontStream.readAllBytes();
        float nativePadding = getNativeSdfPadding(fontDesc);
        FontRenderer.Params params = new FontRenderer.Params();
        params.size = fontDesc.getSize();
        params.cacheWidth = 1;
        params.cacheHeight = 1;
        params.sdfBasePadding = FontRenderer.DEFAULT_SDF_BASE_PADDING;
        params.sdfEdgeValue = FontRenderer.DEFAULT_SDF_EDGE_VALUE;
        params.outlineWidth = fontDesc.getOutlineWidth();
        params.shadowBlur = fontDesc.getShadowAlpha() > 0.0f ? fontDesc.getShadowBlur() : 0.0f;
        params.outputBitmap = fontDesc.getOutputFormat() == FontTextureFormat.TYPE_BITMAP;
        params.antialias = fontDesc.getAntialias() != 0;
        params.hasOutline = fontDesc.getOutlineWidth() > 0.0f && fontDesc.getOutlineAlpha() > 0.0f;
        params.hasShadow = fontDesc.getShadowAlpha() > 0.0f;
        try (FontRenderer renderer = new FontRenderer(fontDesc.getFont(), fontBytes, params)) {
            buildNativeTTF(renderer, !metadataOnly, fontBytes, params);
        }
        if (fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD) {
            glyphBankBuilder.setSdfSpread(nativePadding)
                .setSdfOutline(calculateNativeSdfLimit(nativePadding, fontDesc.getOutlineWidth()))
                .setSdfShadow(fontDesc.getShadowBlur() == 0 ? 1.0f : calculateNativeSdfLimit(nativePadding, fontDesc.getShadowBlur()));
        }
        generateGlyphBank(preview, null, null, compressGlyphData, !metadataOnly);
    }

    /**
     * Public Editor API for compiling a font representation used by Editor previews.
     * Native distance-field previews return glyph metadata without the unused pixel payload.
     * All font-derived values are calculated here.
     * The caller retains ownership of both streams. The bitmap arguments must both be null for non-BMFont input.
     */
    public EditorFontMap compileForEditor(InputStream fontStream, FontDesc fontDesc, String bitmapPath, InputStream bitmapStream) throws TextureGeneratorException, IOException {
        boolean metadataOnly = fontDesc.getOutputFormat() == FontTextureFormat.TYPE_DISTANCE_FIELD && !isBitmapFont(fontDesc);
        compile(fontStream, fontDesc, false, bitmapPath, bitmapStream, false, metadataOnly);
        GlyphBank glyphBank = getGlyphBank();
        int count = glyphBank.getGlyphsCount();
        int[] widths = new int[count];
        int[] heights = new int[count];
        for (int i = 0; i < count; ++i) {
            Glyph glyph = glyphs.get(i);
            widths[i] = glyph.width == 0 ? 0 : glyph.width + 2;
            heights[i] = glyph.pixelHeight == 0 ? 0 : glyph.pixelHeight + 2;
        }
        FontMap fontMap = FontMap.newBuilder()
            .setMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getMaterial(), ".material", ".materialc"))
            .setSize(fontDesc.getSize()).setAntialias(fontDesc.getAntialias())
            .setShadowX(fontDesc.getShadowX()).setShadowY(fontDesc.getShadowY()).setShadowBlur(fontDesc.getShadowBlur())
            .setShadowAlpha(fontDesc.getShadowAlpha()).setAlpha(fontDesc.getAlpha())
            .setOutlineAlpha(fontDesc.getOutlineAlpha()).setOutlineWidth(fontDesc.getOutlineWidth())
            .setLayerMask(GetFontMapLayerMask(fontDesc)).setOutputFormat(fontDesc.getOutputFormat())
            .setRenderMode(fontDesc.getRenderMode()).setAllChars(fontDesc.getAllChars()).setCharacters(fontDesc.getCharacters())
            .setCacheWidth(glyphBank.getCacheWidth()).setCacheHeight(glyphBank.getCacheHeight())
            .setSdfSpread(glyphBank.getSdfSpread()).setSdfOutline(glyphBank.getSdfOutline())
            .setSdfShadow(glyphBank.getSdfShadow()).setPadding(glyphBank.getPadding()).build();
        return new EditorFontMap(fontMap, glyphBank, widths, heights);
    }

    /**
     * Public Editor API for compiling the compressed glyph bank written by an Editor font build target.
     * All font-derived values and glyph-data compression are handled by the Bob font compiler.
     * The caller retains ownership of both streams. The bitmap arguments must both be null for non-BMFont input.
     */
    public GlyphBank compileForEditorBuild(InputStream fontStream, FontDesc fontDesc, String bitmapPath, InputStream bitmapStream) throws TextureGeneratorException, IOException {
        compile(fontStream, fontDesc, false, bitmapPath, bitmapStream, true, false);
        return getGlyphBank();
    }

    private static void usage() {
        System.err.println("Usage: fontc fontfile outfile [basedir] [dynamic]");
        System.exit(1);
    }

    // Run with: java -cp bob.jar com.dynamo.bob.font.Fontc input.font output.fontc
    public static void main(String[] args) throws TextureGeneratorException {
        try {
            if (args.length < 1) {
                System.err.println("No input .font specified!");
                usage();
            }
            if (args.length < 2) {
                System.err.println("No output file specified!");
                usage();
            }

            File fontInput = new File(args[0]);
            String outfile = args[1];
            String basedir = args.length >= 3 ? args[2] : ".";
            boolean dynamic = args.length >= 4 && Boolean.parseBoolean(args[3]);

            FontDesc fontDesc;
            try (InputStreamReader reader = new InputStreamReader(new FileInputStream(fontInput))) {
                FontDesc.Builder builder = FontDesc.newBuilder();
                TextFormat.merge(reader, builder);
                fontDesc = builder.build();
            }
            if (fontDesc.getFont().isEmpty()) {
                System.err.println("No font specified in " + args[0] + ".");
                System.exit(1);
            }

            String fontInputFile = basedir + File.separator + fontDesc.getFont();
            File sourceFont = new File(fontInputFile);
            if (!sourceFont.isFile()) {
                System.err.printf("%s:0 error: is missing the dependent font-file '%s'%n", args[0], fontDesc.getFont());
                System.exit(1);
            }
            File material = new File(basedir + File.separator + fontDesc.getMaterial());
            if (!material.isFile()) {
                System.err.printf("%s:0 error: is missing the dependent material-file '%s'%n", args[0], fontDesc.getMaterial());
                System.exit(1);
            }

            Path sourceParent = sourceFont.toPath().toAbsolutePath().getParent();
            String bitmapPath = null;
            File bitmapFile = null;
            if (isBitmapFont(fontDesc)) {
                BMFont bitmapFont = new BMFont();
                try (InputStream bitmapFontStream = new BufferedInputStream(new FileInputStream(sourceFont))) {
                    bitmapFont.parse(bitmapFontStream);
                } catch (BMFontFormatException e) {
                    throw new IOException(e.getMessage(), e);
                }
                bitmapPath = bitmapFont.page.get(0);
                bitmapFile = sourceParent.resolve(bitmapPath).toFile();
            }

            Fontc fontc = new Fontc();
            try (InputStream fontStream = new BufferedInputStream(new FileInputStream(sourceFont));
                 InputStream bitmapStream = bitmapFile == null ? null : new BufferedInputStream(new FileInputStream(bitmapFile))) {
                fontc.compile(fontStream, fontDesc, false, bitmapPath, bitmapStream);
            }

            Path basedirPath = Paths.get(basedir).toAbsolutePath();
            FontMap.Builder fontMap = FontMap.newBuilder()
                .setMaterial(ResourceUtil.minifyPathAndReplaceExt(fontDesc.getMaterial(), ".material", ".materialc"));
            if (dynamic) {
                if (fontDesc.getOutputFormat() != FontTextureFormat.TYPE_DISTANCE_FIELD) {
                    System.err.printf("Dynamic fonts currently only support distance fields! '%s'%n", fontInput);
                    System.exit(1);
                }
                fontMap.setFont(fontDesc.getFont());
                Files.copy(sourceFont.toPath(), Paths.get(outfile).toAbsolutePath().getParent().resolve(sourceFont.getName()),
                           StandardCopyOption.REPLACE_EXISTING);
            } else {
                Path glyphBankPath = Paths.get(fontInput.getAbsolutePath().replace(".font", ".glyph_bankc"));
                String projectPath = "/" + basedirPath.relativize(glyphBankPath).toString().replace("\\", "/");
                fontMap.setGlyphBank(projectPath);
                try (FileOutputStream output = new FileOutputStream(outfile.replace(".fontc", ".glyph_bankc"))) {
                    fontc.getGlyphBank().writeTo(output);
                }
            }

            fontMap.setSize(fontDesc.getSize()).setAntialias(fontDesc.getAntialias())
                .setShadowX(fontDesc.getShadowX()).setShadowY(fontDesc.getShadowY())
                .setShadowBlur(fontDesc.getShadowBlur()).setShadowAlpha(fontDesc.getShadowAlpha())
                .setAlpha(fontDesc.getAlpha()).setOutlineAlpha(fontDesc.getOutlineAlpha())
                .setOutlineWidth(fontDesc.getOutlineWidth()).setLayerMask(GetFontMapLayerMask(fontDesc))
                .setOutputFormat(fontDesc.getOutputFormat()).setRenderMode(fontDesc.getRenderMode());
            try (FileOutputStream output = new FileOutputStream(outfile)) {
                fontMap.build().writeTo(output);
            }
        } catch (IOException e) {
            System.err.println(e.getMessage());
            System.exit(1);
        }
    }
}
