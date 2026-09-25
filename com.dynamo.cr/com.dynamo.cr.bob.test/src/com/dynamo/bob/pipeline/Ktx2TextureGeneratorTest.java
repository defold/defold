// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License");
// you may obtain a copy of the License at https://www.defold.com/license
package com.dynamo.bob.pipeline;

import static org.junit.Assert.*;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.List;
import java.util.zip.Deflater;

import org.junit.BeforeClass;
import org.junit.Test;

import com.defold.extension.pipeline.texture.TextureCompression;
import com.defold.extension.pipeline.texture.TextureCompressorBasisU;
import com.defold.extension.pipeline.texture.TextureCompressorParams;
import com.defold.extension.pipeline.texture.TextureCompressorPreset;
import com.defold.extension.pipeline.texture.TextureCompressorUncompressed;
import com.dynamo.bob.pipeline.Texc.FlipAxis;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.CompressionType;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.google.protobuf.TextFormat;

public class Ktx2TextureGeneratorTest {
    @BeforeClass
    public static void registerCompressors() {
        TextureCompression.registerCompressor(new TextureCompressorBasisU());
    }

    public static byte[] fixture(String name) throws IOException {
        try (InputStream stream = Ktx2TextureGeneratorTest.class.getResourceAsStream("ktx2/" + name + ".ktx2")) {
            return stream.readAllBytes();
        }
    }

    private static TextureProfile profile(boolean basis, boolean mips, boolean premultiply, int maxSize,
                                           boolean recompress, boolean regenerate) {
        return TextureProfile.newBuilder().setName("KTX2")
                .addPlatforms(PlatformProfile.newBuilder().setOs(PlatformProfile.OS.OS_ID_GENERIC)
                    .setMipmaps(mips).setPremultiplyAlpha(premultiply).setMaxTextureSize(maxSize)
                    .setRecompress(recompress).setRegenerateMipmaps(regenerate)
                    .addFormats(TextureFormatAlternative.newBuilder().setFormat(TextureFormat.TEXTURE_FORMAT_RGBA)
                        .setCompressionLevel(TextureFormatAlternative.CompressionLevel.FAST)
                        .setCompressionType(basis ? CompressionType.COMPRESSION_TYPE_BASIS_UASTC : CompressionType.COMPRESSION_TYPE_DEFAULT)))
                .build();
    }

    private static TextureGenerator.GenerateResult generate(byte[] data, TextureProfile profile, boolean compress) throws Exception {
        return TextureGenerator.generate(data, profile, compress, EnumSet.noneOf(FlipAxis.class));
    }

    // A small, explicit container builder keeps orientation, channel and alpha tests lossless.
    public static byte[] raw(int width, int height, int channels, boolean srgb, boolean premultiplied,
                             String orientation, byte[]... levels) {
        byte[] metadata = orientation == null ? new byte[0]
                : ("KTXorientation\0" + orientation + "\0").getBytes(StandardCharsets.US_ASCII);
        int metadataSize = metadata.length == 0 ? 0 : 4 + ((metadata.length + 3) & ~3);
        int dfdOffset = 80 + 24 * levels.length;
        int dfdSize = 28 + channels * 16;
        int dataOffset = dfdOffset + dfdSize + metadataSize;
        int size = dataOffset;
        for (byte[] level : levels) size += level.length;
        ByteBuffer b = ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
        b.put(new byte[] {(byte)0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, (byte)0xbb, 13, 10, 26, 10});
        b.putInt(12, new int[] {9, 16, 23, 37}[channels - 1] + (srgb ? 6 : 0));
        b.putInt(16, 1).putInt(20, width).putInt(24, height).putInt(36, 1).putInt(40, levels.length);
        b.putInt(48, dfdOffset).putInt(52, dfdSize);
        if (metadataSize > 0) {
            b.putInt(56, dfdOffset + dfdSize).putInt(60, metadataSize);
            b.position(dfdOffset + dfdSize);
            b.putInt(metadata.length).put(metadata);
        }
        b.putInt(dfdOffset, dfdSize).putShort(dfdOffset + 8, (short)2).putShort(dfdOffset + 10, (short)(dfdSize - 4));
        b.put(dfdOffset + 12, (byte)1).put(dfdOffset + 13, (byte)1)
                .put(dfdOffset + 14, (byte)(srgb ? 2 : 1)).put(dfdOffset + 15, (byte)(premultiplied ? 1 : 0))
                .put(dfdOffset + 20, (byte)channels);
        for (int c = 0; c < channels; ++c) {
            int sample = dfdOffset + 28 + 16 * c;
            b.putShort(sample, (short)(c * 8)).put(sample + 2, (byte)7)
                .put(sample + 3, (byte)(c == 3 ? 15 : c)).putInt(sample + 12, 255);
        }
        for (int i = 0; i < levels.length; ++i) {
            b.putLong(80 + 24 * i, dataOffset).putLong(88 + 24 * i, levels[i].length).putLong(96 + 24 * i, levels[i].length);
            b.position(dataOffset);
            b.put(levels[i]);
            dataOffset += levels[i].length;
        }
        return b.array();
    }

    private static byte[] solid(int pixels, int r, int g, int b, int a) {
        byte[] result = new byte[pixels * 4];
        for (int i = 0; i < result.length; i += 4) {
            result[i] = (byte)r; result[i + 1] = (byte)g; result[i + 2] = (byte)b; result[i + 3] = (byte)a;
        }
        return result;
    }

    @Test
    public void channelsAndOrientation() throws Exception {
        byte[] rg = {10, 20, 30, 40, 50, 60, 70, 80};
        byte[] source = raw(2, 2, 2, false, false, null, rg);
        TextureProfile profile = profile(false, false, true, 0, false, false);
        assertArrayEquals(new byte[] {10,20,0,-1,30,40,0,-1,50,60,0,-1,70,80,0,-1}, generate(source, profile, false).imageDatas.get(0));
        byte[] flipped = TextureGenerator.generate(source, profile, false).imageDatas.get(0);
        assertArrayEquals(new byte[] {50,60,0,-1,70,80,0,-1,10,20,0,-1,30,40,0,-1}, flipped);
        source = raw(2, 2, 2, false, false, "lu", rg);
        assertArrayEquals(new byte[] {30,40,0,-1,10,20,0,-1,70,80,0,-1,50,60,0,-1}, TextureGenerator.generate(source, profile, false).imageDatas.get(0));
        for (boolean srgb : new boolean[] {false, true}) {
            byte[] r = raw(1, 1, 1, srgb, false, null, new byte[] {37});
            assertArrayEquals(new byte[] {37,0,0,-1}, generate(r, profile, false).imageDatas.get(0));
        }
    }

    @Test
    public void linearAndSrgbEncodingPreservesTransferFunction() throws Exception {
        byte[] base = solid(16, 128, 128, 255, 64);
        byte[] authored = solid(4, 64, 128, 192, 32);
        for (boolean srgb : new boolean[] {false, true}) {
            byte[] source = raw(4, 4, 4, srgb, false, null, base, authored);
            TextureGenerator.GenerateResult uncompressed = generate(source, profile(false, true, false, 0, true, false), false);
            assertArrayEquals(base, uncompressed.imageDatas.get(0));
            assertArrayEquals(authored, uncompressed.imageDatas.get(1));
            assertArrayEquals(solid(1, 64, 128, 192, 32), uncompressed.imageDatas.get(2));

            TextureGenerator.GenerateResult compressed = generate(source, profile(true, true, false, 0, true, false), true);
            assertEquals(3, compressed.imageDatas.size());
            for (byte[] mip : compressed.imageDatas) {
                assertEquals('s', mip[0]);
                assertEquals('B', mip[1]);
                // basis_file_header::m_flags, including cBASISHeaderFlagSRGB (16).
                int flags = ByteBuffer.wrap(mip).order(ByteOrder.LITTLE_ENDIAN).getShort(21);
                assertEquals(srgb, (flags & 16) != 0);
            }
        }
    }

    @Test
    public void linearDataMipmapsDoNotApplyGamma() throws Exception {
        byte[] pixels = {64, 32, (byte)192, 0, (byte)192, (byte)224, 64, (byte)128,
                        64, 32, (byte)192, 0, (byte)192, (byte)224, 64, (byte)128};
        byte[] source = raw(2, 2, 4, false, false, null, pixels);
        TextureGenerator.GenerateResult generated = generate(source, profile(false, true, false, 0, false, true), false);
        assertArrayEquals(pixels, generated.imageDatas.get(0));
        assertArrayEquals(solid(1, 128, 128, 128, 64), generated.imageDatas.get(1));
        TextureGenerator.GenerateResult resized = generate(source, profile(false, false, false, 1, false, false), false);
        assertArrayEquals(generated.imageDatas.get(1), resized.imageDatas.get(0));
    }

    @Test
    public void srgbMipmapsFilterInLinearLight() throws Exception {
        byte[] pixels = {0, 0, 0, 0, -1, -1, -1, (byte)128,
                        0, 0, 0, 0, -1, -1, -1, (byte)128};
        byte[] source = raw(2, 2, 4, true, false, null, pixels, solid(1, 32, 64, 96, 128));
        TextureGenerator.GenerateResult regenerated = generate(source, profile(false, true, false, 0, false, true), false);
        assertArrayEquals(pixels, regenerated.imageDatas.get(0));
        byte[] mip = regenerated.imageDatas.get(1);
        for (int channel = 0; channel < 3; ++channel) {
            // sRGB encoding of 0.5 linear intensity is about 188, not 128.
            assertEquals(188, mip[channel] & 0xff, 1);
        }
        assertEquals(64, mip[3] & 0xff); // Alpha is always linear.

        byte[] baseOnly = raw(2, 2, 4, true, false, null, pixels);
        TextureGenerator.GenerateResult resized = generate(baseOnly, profile(false, false, false, 1, false, false), false);
        assertArrayEquals(mip, resized.imageDatas.get(0));

        byte[] partial = raw(4, 4, 4, true, false, null, solid(16, 32, 64, 96, 128), pixels);
        TextureGenerator.GenerateResult completed = generate(partial, profile(false, true, false, 0, false, false), false);
        assertArrayEquals(pixels, completed.imageDatas.get(1));
        assertArrayEquals(mip, completed.imageDatas.get(2));
    }

    @Test
    public void srgbPremultipliedFilteringKeepsTransparentEdges() throws Exception {
        for (boolean premultiplied : new boolean[] {false, true}) {
            byte transparentWhite = premultiplied ? 0 : (byte)255;
            byte[] pixels = {-1, -1, -1, -1, transparentWhite, transparentWhite, transparentWhite, 0,
                            -1, -1, -1, -1, transparentWhite, transparentWhite, transparentWhite, 0};
            byte[] expected = solid(1, 128, 128, 128, 128);
            byte[] source = raw(2, 2, 4, true, premultiplied, null, pixels, solid(1, 0, 0, 0, 0));
            TextureGenerator.GenerateResult regenerated = generate(source, profile(false, true, true, 0, false, true), false);
            assertArrayEquals(expected, regenerated.imageDatas.get(1));

            source = raw(2, 2, 4, true, premultiplied, null, pixels);
            TextureGenerator.GenerateResult resized = generate(source, profile(false, false, true, 1, false, false), false);
            assertArrayEquals(expected, resized.imageDatas.get(0));

            source = raw(4, 4, 4, true, premultiplied, null, solid(16, 0, 0, 0, 0), pixels);
            TextureGenerator.GenerateResult completed = generate(source, profile(false, true, true, 0, false, false), false);
            assertArrayEquals(regenerated.imageDatas.get(0), completed.imageDatas.get(1));
            assertArrayEquals(expected, completed.imageDatas.get(2));
        }
    }

    @Test
    public void srgbPremultipliedFilteringWeightsColorInLinearLight() throws Exception {
        for (boolean premultiplied : new boolean[] {false, true}) {
            byte white = premultiplied ? (byte)128 : (byte)255;
            byte[] pixels = {0, 0, 0, -1, white, white, white, (byte)128,
                            0, 0, 0, -1, white, white, white, (byte)128};
            byte[] source = raw(2, 2, 4, true, premultiplied, null, pixels);
            byte[] mip = generate(source, profile(false, true, true, 0, false, false), false).imageDatas.get(1);
            for (int channel = 0; channel < 3; ++channel) {
                // Linear intensity 128/383, encoded to sRGB and multiplied by mean alpha 191.5/255.
                assertEquals(117, mip[channel] & 0xff, 1);
            }
            assertEquals(192, mip[3] & 0xff, 1);
        }
    }

    @Test
    public void ordinaryImagesKeepChannelValueFiltering() throws Exception {
        BufferedImage image = new BufferedImage(2, 2, BufferedImage.TYPE_INT_ARGB);
        image.setRGB(0, 0, 2, 2, new int[] {0xff000000, 0xffffffff, 0xff000000, 0xffffffff}, 0, 2);
        TextureGenerator.GenerateResult result = TextureGenerator.generate(image, profile(false, true, false, 0, false, true), false);
        for (int channel = 0; channel < 3; ++channel) {
            assertEquals(128, result.imageDatas.get(1)[channel] & 0xff, 1);
        }
        assertEquals(255, result.imageDatas.get(1)[3] & 0xff);
    }

    // Bob discovers this class when scanning the test jar, so it needs a public no-argument constructor.
    public static class MipIndexCompressor extends TextureCompressorUncompressed {
        final List<Integer> levels = new ArrayList<>();

        @Override
        public String getName() { return "Ktx2MipIndexTest"; }

        @Override
        public byte[] compress(TextureCompressorPreset preset, TextureCompressorParams params, byte[] input) {
            levels.add(params.getMipMapLevel());
            return super.compress(preset, params, input);
        }
    }

    @Test
    public void compressorsReceiveOutputMipIndices() throws Exception {
        MipIndexCompressor compressor = new MipIndexCompressor();
        List<Integer> levels = compressor.levels;
        TextureCompression.registerCompressor(compressor);
        TextureProfile.Builder profile = profile(false, true, false, 0, false, false).toBuilder();
        profile.getPlatformsBuilder(0).getFormatsBuilder(0)
                .setCompressor(compressor.getName())
                .setCompressorPreset(TextureCompressorUncompressed.TextureCompressorUncompressedPresetName);
        byte[] source = raw(8, 4, 4, false, false, null, solid(32, 80, 40, 20, 255), solid(8, 20, 200, 40, 255));
        generate(source, profile.build(), true);
        assertEquals(Arrays.asList(0, 1, 2, 3), levels);

        levels.clear();
        profile.getPlatformsBuilder(0).setMaxTextureSize(4);
        generate(source, profile.build(), true);
        assertEquals(Arrays.asList(0, 1, 2), levels);

        levels.clear();
        profile.getPlatformsBuilder(0).setMaxTextureSize(0).setRegenerateMipmaps(true);
        generate(source, profile.build(), true);
        assertEquals(Arrays.asList(0, 1, 2, 3), levels);
    }

    @Test
    public void authoredMipPreviewsKeepOriginalSizeAndChannels() throws Exception {
        byte[] source = raw(3, 2, 2, false, false, null,
                new byte[] {10,20,30,40,50,60,70,80,90,100,110,120}, new byte[] {23,45});
        try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(source)) {
            TextureGenerator.GenerateResult base = TextureGenerator.generateKtx2MipPreview(texture, 0);
            assertEquals(3, base.textureImage.getAlternatives(0).getWidth());
            assertEquals(2, base.textureImage.getAlternatives(0).getHeight());
            assertEquals(1, base.imageDatas.size());
            assertArrayEquals(new byte[] {70,80,0,-1,90,100,0,-1,110,120,0,-1,10,20,0,-1,30,40,0,-1,50,60,0,-1}, base.imageDatas.get(0));
            TextureGenerator.GenerateResult mip = TextureGenerator.generateKtx2MipPreview(texture, 1);
            assertEquals(1, mip.textureImage.getAlternatives(0).getWidth());
            assertEquals(1, mip.textureImage.getAlternatives(0).getHeight());
            assertArrayEquals(new byte[] {23,45,0,-1}, mip.imageDatas.get(0));
        }
        source = raw(2, 2, 2, false, false, "lu", new byte[] {10,20,30,40,50,60,70,80});
        try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(source)) {
            assertArrayEquals(new byte[] {30,40,0,-1,10,20,0,-1,70,80,0,-1,50,60,0,-1},
                    TextureGenerator.generateKtx2MipPreview(texture, 0).imageDatas.get(0));
        }
    }

    @Test
    public void losslessRawWrappingAndSmallBC7Levels() throws Exception {
        byte[] pixels = solid(8, 17, 91, 130, 255);
        byte[] bytes = raw(4, 2, 4, true, false, null, pixels);
        int offset = (int)ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getLong(80);
        Deflater deflater = new Deflater();
        deflater.setInput(pixels);
        deflater.finish();
        byte[] compressed = new byte[128];
        int compressedSize = deflater.deflate(compressed);
        deflater.end();
        bytes = Arrays.copyOf(bytes, offset + compressedSize);
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putInt(44, 3).putLong(88, compressedSize);
        System.arraycopy(compressed, 0, bytes, offset, compressedSize);
        try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(bytes)) {
            assertEquals(43, texture.vkFormat);
            assertEquals(3, texture.supercompression);
        }
        assertArrayEquals(pixels, generate(bytes, profile(false, false, false, 0, false, false), false).imageDatas.get(0));
        // levelCount=0 is a valid request for generated mips on uncompressed formats.
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).putInt(40, 0);
        assertEquals(3, generate(bytes, profile(false, true, false, 0, false, false), false).imageDatas.size());
        bytes = fixture("bc7");
        ByteBuffer b = ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);
        offset = (int)b.getLong(80);
        b.putInt(20, 2).putInt(24, 1).putLong(88, 16).putLong(96, 16);
        try (TexcLibraryJni.Ktx2Texture image = TexcLibraryJni.LoadKtx2(Arrays.copyOf(bytes, offset + 16))) {
            assertEquals(8, image.decodeMip(0).length);
        }
    }

    @Test
    public void premultipliedAlphaIsNotAppliedTwice() throws Exception {
        byte[] pixels = solid(4, 40, 20, 10, 128);
        byte[] source = raw(2, 2, 4, false, true, null, pixels, solid(1, 20, 10, 5, 128));
        TextureGenerator.GenerateResult result = generate(source, profile(false, true, true, 0, false, false), false);
        assertArrayEquals(pixels, result.imageDatas.get(0));
        assertArrayEquals(solid(1, 20, 10, 5, 128), result.imageDatas.get(1));
        try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(source)) {
            assertArrayEquals(pixels, TextureGenerator.generateKtx2MipPreview(texture, 0).imageDatas.get(0));
            assertArrayEquals(solid(1, 20, 10, 5, 128), TextureGenerator.generateKtx2MipPreview(texture, 1).imageDatas.get(0));
        }
        assertArrayEquals(solid(4, 80, 40, 20, 128), generate(source, profile(false, false, false, 0, false, false), false).imageDatas.get(0));
        source = raw(1, 1, 4, false, false, null, solid(1, 200, 120, 60, 0));
        assertArrayEquals(solid(1, 0, 0, 0, 0), generate(source, profile(false, false, true, 0, false, false), false).imageDatas.get(0));
    }

    @Test
    public void authoredMipsResizingAndRegeneration() throws Exception {
        byte[] base = solid(32, 80, 40, 20, 255);
        byte[] authored = solid(8, 20, 200, 40, 255);
        byte[] source = raw(8, 4, 4, false, false, null, base, authored);
        TextureGenerator.GenerateResult result = generate(source, profile(false, true, false, 0, false, false), false);
        assertEquals(4, result.imageDatas.size());
        assertArrayEquals(base, result.imageDatas.get(0));
        assertArrayEquals(authored, result.imageDatas.get(1));
        assertArrayEquals(solid(2, 20, 200, 40, 255), result.imageDatas.get(2));
        assertArrayEquals(solid(1, 20, 200, 40, 255), result.imageDatas.get(3));
        result = generate(source, profile(false, true, false, 4, false, false), false);
        assertEquals(4, result.textureImage.getAlternatives(0).getWidth());
        assertEquals(8, result.textureImage.getAlternatives(0).getOriginalWidth());
        assertArrayEquals(authored, result.imageDatas.get(0));
        result = generate(source, profile(false, true, false, 0, false, true), false);
        assertArrayEquals(solid(8, 80, 40, 20, 255), result.imageDatas.get(1));
        assertEquals(1, generate(source, profile(false, false, false, 0, false, true), false).imageDatas.size());
        source = raw(3, 2, 4, false, false, null, solid(6, 80, 40, 20, 255), solid(1, 20, 200, 40, 255));
        result = generate(source, profile(false, true, false, 0, false, false), false);
        assertArrayEquals(solid(1, 80, 40, 20, 255), result.imageDatas.get(result.imageDatas.size() - 1));
    }

    @Test
    public void basisPreservationAndProfileOverrides() throws Exception {
        for (String name : new String[] {"uastc", "uastc-zstd", "uastc-zlib"}) {
            byte[] source = fixture(name);
            try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(source)) {
                TextureProfile profile = profile(true, true, false, 0, false, false);
                TextureGenerator.GenerateResult preserved = generate(source, profile, true);
                assertEquals(4, preserved.imageDatas.size());
                assertArrayEquals(texture.repackMip(0), preserved.imageDatas.get(0));
                assertArrayEquals(texture.repackMip(1), preserved.imageDatas.get(1));
                TextureGenerator.GenerateResult regenerated = generate(source, profile(true, true, false, 0, false, true), true);
                assertArrayEquals(texture.repackMip(0), regenerated.imageDatas.get(0));
                assertFalse(Arrays.equals(texture.repackMip(1), regenerated.imageDatas.get(1)));
                assertEquals(CompressionType.COMPRESSION_TYPE_BASIS_UASTC, preserved.textureImage.getAlternatives(0).getCompressionType());
                TextureGenerator.GenerateResult forced = generate(source, profile(true, true, false, 0, true, false), true);
                assertFalse(Arrays.equals(texture.repackMip(0), forced.imageDatas.get(0)));
                assertFalse(Arrays.equals(texture.repackMip(0), TextureGenerator.generate(source, profile, true).imageDatas.get(0)));
                assertArrayEquals(texture.repackMip(1), generate(source, profile(true, true, false, 4, false, false), true).imageDatas.get(0));
                assertEquals(CompressionType.COMPRESSION_TYPE_DEFAULT, generate(source, profile, false).textureImage.getAlternatives(0).getCompressionType());
                assertEquals(CompressionType.COMPRESSION_TYPE_DEFAULT, generate(source, null, true).textureImage.getAlternatives(0).getCompressionType());
            }
        }
    }

    @Test
    public void decodeAllCodecsAndRecompressBC7() throws Exception {
        for (String name : new String[] {"etc1s", "uastc", "uastc-zstd", "uastc-zlib", "bc7", "bc7-zlib"}) {
            byte[] source = fixture(name);
            try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(source)) {
                assertEquals(8, texture.width); assertEquals(4, texture.height);
                byte[] pixels = texture.decodeMip(0);
                assertEquals(128, pixels.length);
                assertTrue((pixels[28] & 255) > (pixels[0] & 255) + 100);
                assertTrue(name + ": green " + (pixels[1] & 255) + " -> " + (pixels[97] & 255), (pixels[97] & 255) > (pixels[1] & 255) + 30);
                assertEquals(255, pixels[3] & 255);
                TextureGenerator.GenerateResult result = generate(source, profile(true, true, false, 0, true, false), true);
                assertEquals(4, result.imageDatas.size());
                assertEquals(CompressionType.COMPRESSION_TYPE_BASIS_UASTC, result.textureImage.getAlternatives(0).getCompressionType());
                assertEquals('s', result.imageDatas.get(0)[0]); assertEquals('B', result.imageDatas.get(0)[1]);
            }
        }
    }

    @Test
    public void profileFieldsRoundTripAndDefaultToFalse() throws Exception {
        PlatformProfile.Builder defaults = PlatformProfile.newBuilder();
        TextFormat.merge("os: OS_ID_GENERIC mipmaps: true", defaults);
        assertFalse(defaults.getRecompress()); assertFalse(defaults.getRegenerateMipmaps());
        TextureProfile expected = profile(true, true, false, 0, true, true);
        TextureProfile.Builder parsed = TextureProfile.newBuilder();
        TextFormat.merge(TextFormat.printer().printToString(expected), parsed);
        assertEquals(expected, TextureProfile.parseFrom(parsed.build().toByteArray()));
    }

    @Test
    public void rejectMalformedContainersAndCompressedData() throws Exception {
        for (String name : new String[] {"uastc", "bc7"}) {
            byte[] invalidBlock = fixture(name);
            int offset = (int)ByteBuffer.wrap(invalidBlock).order(ByteOrder.LITTLE_ENDIAN).getLong(80);
            invalidBlock[offset] = (byte)(name.equals("uastc") ? 0x45 : 0); // Reserved UASTC mode / no BC7 mode.
            for (boolean compress : new boolean[] {true, false}) {
                try {
                    generate(invalidBlock, profile(true, false, false, 0, false, false), compress);
                    fail("Accepted invalid " + name + " block");
                } catch (TextureGeneratorException expected) {
                    assertTrue(expected.getMessage().contains("KTX2"));
                }
            }
        }
        for (String name : new String[] {"etc1s", "uastc", "uastc-zstd", "uastc-zlib", "bc7", "bc7-zlib"}) {
            byte[] bytes = fixture(name);
            for (int offset : new int[] {12, 28, 32, 36, 40, 48, 52, 80, 88}) {
                byte[] corrupt = bytes.clone();
                ByteBuffer.wrap(corrupt).order(ByteOrder.LITTLE_ENDIAN).putInt(offset, -1);
                try {
                    generate(corrupt, null, false);
                    fail("Accepted malformed " + name + " at " + offset);
                } catch (IOException | TextureGeneratorException expected) {
                    assertTrue(expected.getMessage().contains("KTX2"));
                }
            }
        }
        byte[] corrupt = fixture("uastc-zstd");
        int offset = (int)ByteBuffer.wrap(corrupt).order(ByteOrder.LITTLE_ENDIAN).getLong(80);
        corrupt[offset] = 0;
        try {
            generate(corrupt, null, false);
            fail("Accepted corrupt Zstd frame");
        } catch (TextureGeneratorException expected) {
            assertTrue(expected.getMessage().contains("Zstd"));
        }
    }
}
