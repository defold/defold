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

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;

import java.awt.image.DataBuffer;
import java.awt.image.DataBufferByte;
import java.awt.image.DataBufferShort;
import java.awt.image.DataBufferUShort;
import java.awt.image.DataBufferInt;

import java.nio.ShortBuffer;
import java.nio.IntBuffer;

import java.io.ByteArrayInputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.EnumSet;
import java.util.List;

import javax.imageio.ImageIO;

import com.defold.extension.pipeline.texture.*;
import com.dynamo.bob.pipeline.Texc.FlipAxis;

import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

public class TextureGenerator {

    // specify what is maximum of threads TextureGenerator may use
    public static int maxThreads = 4; // set to getHalfThreads() in Project.java

    public static class GenerateResult {
        public TextureImage textureImage;
        public ArrayList<byte[]> imageDatas;
    }

    private static class DecodedImage {
        public boolean hdr;
        public boolean premultiplied;
        // KTX2 specifies its transfer function. Ordinary images retain legacy channel-value filtering.
        public boolean srgbFiltering;
        public TexcLibraryJni.Ktx2Texture ktx2;
        public String path;
        public int width;
        public int height;
        public int componentCount;
        public int texcPixelFormat;
        public int texcColorSpace;
        public byte[] data;
    }

    private static class TextureGenerationSettings {
        public TextureFormat textureFormat;
        public int outputPixelFormat;
        public String compressorName;
        public String compressorPresetName;
        public boolean generateMipMaps;
        public boolean recompress;
        public boolean regenerateMipmaps;
        public TexcLibraryJni.Ktx2Texture ktx2Encoder;
        public int maxTextureSize;
        public boolean premulAlpha;
        public boolean powerOfTwo;
        public boolean squarePVRTC;
        public boolean alignToCompressor;
        public boolean dither;
        public int compressionInputPixelFormat;
        public String createProfileName;
        public String resizeErrorMessage;
        public String flipErrorMessage;
        public String encodeErrorMessage;
    }

    private static final HashMap<TextureFormat, Integer> pixelFormatLUT = new HashMap<>();

    static {
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_LUMINANCE, Texc.PixelFormat.PF_L8.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB, Texc.PixelFormat.PF_R8G8B8.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA, Texc.PixelFormat.PF_R8G8B8A8.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1, Texc.PixelFormat.PF_RGB_PVRTC_2BPPV1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, Texc.PixelFormat.PF_RGB_PVRTC_4BPPV1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1, Texc.PixelFormat.PF_RGBA_PVRTC_2BPPV1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1, Texc.PixelFormat.PF_RGBA_PVRTC_4BPPV1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_ETC1, Texc.PixelFormat.PF_RGB_ETC1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_16BPP, Texc.PixelFormat.PF_R5G6B5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP, Texc.PixelFormat.PF_R4G4B4A4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA, Texc.PixelFormat.PF_L8A8.getValue());

        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ETC2, Texc.PixelFormat.PF_RGBA_ETC2.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_BC1, Texc.PixelFormat.PF_RGB_BC1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_BC3, Texc.PixelFormat.PF_RGBA_BC3.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_R_BC4, Texc.PixelFormat.PF_R_BC4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RG_BC5, Texc.PixelFormat.PF_RG_BC5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_BC7, Texc.PixelFormat.PF_RGBA_BC7.getValue());

        // ASTC formats
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_4X4, Texc.PixelFormat.PF_RGBA_ASTC_4x4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5X5, Texc.PixelFormat.PF_RGBA_ASTC_5x5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_5X4, Texc.PixelFormat.PF_RGBA_ASTC_5x4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6X5, Texc.PixelFormat.PF_RGBA_ASTC_6x5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_6X6, Texc.PixelFormat.PF_RGBA_ASTC_6x6.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X5, Texc.PixelFormat.PF_RGBA_ASTC_8x5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X6, Texc.PixelFormat.PF_RGBA_ASTC_8x6.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_8X8, Texc.PixelFormat.PF_RGBA_ASTC_8x8.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X5, Texc.PixelFormat.PF_RGBA_ASTC_10x5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X6, Texc.PixelFormat.PF_RGBA_ASTC_10x6.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X8, Texc.PixelFormat.PF_RGBA_ASTC_10x8.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_10X10, Texc.PixelFormat.PF_RGBA_ASTC_10x10.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12X10, Texc.PixelFormat.PF_RGBA_ASTC_12x10.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_12X12, Texc.PixelFormat.PF_RGBA_ASTC_12x12.getValue());
    }

    private static BufferedImage convertImage(BufferedImage origImage, int type) {
        BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), type);
        Graphics2D g2d = image.createGraphics();
        g2d.drawImage(origImage, 0, 0, null);
        g2d.dispose();
        return image;
    }

    private static TextureFormat pickUncompressedFormat(TextureFormat targetFormat) {
        switch (targetFormat) {
            // Luminance formats
            case TEXTURE_FORMAT_R_BC4:
                return TextureFormat.TEXTURE_FORMAT_LUMINANCE;

            // Alpha formats
            case TEXTURE_FORMAT_RG_BC5:
                return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;

            // RGB formats
            case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case TEXTURE_FORMAT_RGB_ETC1:
            case TEXTURE_FORMAT_RGB_16BPP:
            case TEXTURE_FORMAT_RGB_BC1:
                return TextureFormat.TEXTURE_FORMAT_RGB;

            // RGBA formats
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            case TEXTURE_FORMAT_RGBA_16BPP:
            case TEXTURE_FORMAT_RGBA_ETC2:
            case TEXTURE_FORMAT_RGBA_ASTC_4X4:
            case TEXTURE_FORMAT_RGBA_BC3:
            case TEXTURE_FORMAT_RGBA_BC7:
            case TEXTURE_FORMAT_RGBA_ASTC_5X4:
            case TEXTURE_FORMAT_RGBA_ASTC_5X5:
            case TEXTURE_FORMAT_RGBA_ASTC_6X5:
            case TEXTURE_FORMAT_RGBA_ASTC_6X6:
            case TEXTURE_FORMAT_RGBA_ASTC_8X5:
            case TEXTURE_FORMAT_RGBA_ASTC_8X6:
            case TEXTURE_FORMAT_RGBA_ASTC_8X8:
            case TEXTURE_FORMAT_RGBA_ASTC_10X5:
            case TEXTURE_FORMAT_RGBA_ASTC_10X6:
            case TEXTURE_FORMAT_RGBA_ASTC_10X8:
            case TEXTURE_FORMAT_RGBA_ASTC_10X10:
            case TEXTURE_FORMAT_RGBA_ASTC_12X10:
            case TEXTURE_FORMAT_RGBA_ASTC_12X12:
                return TextureFormat.TEXTURE_FORMAT_RGBA;
            default: break;
        }
        return targetFormat;
    }

    // pickOptimalFormat will try to pick a texture format with the same number of channels as componentCount,
    // while still using a texture format within the same "family".
    private static TextureFormat pickOptimalFormat(int componentCount, TextureFormat targetFormat) {

        switch (targetFormat) {

            // Force down to luminance if only 1 input component
            case TEXTURE_FORMAT_RGB -> {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                else if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;
                return TextureFormat.TEXTURE_FORMAT_RGB;
            }
            case TEXTURE_FORMAT_RGBA -> {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                else if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;
                else if (componentCount == 3)
                    return TextureFormat.TEXTURE_FORMAT_RGB;

                return TextureFormat.TEXTURE_FORMAT_RGBA;
            }


            // PVRTC with 4 channels
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1 -> {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
            }
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 -> {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            }
            case TEXTURE_FORMAT_RGBA_16BPP -> {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_16BPP;
                return TextureFormat.TEXTURE_FORMAT_RGBA_16BPP;
            }
            case TEXTURE_FORMAT_RGBA_ETC2 -> {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_ETC2;
            }
            case TEXTURE_FORMAT_RGBA_BC3 -> {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_BC3;
            }
            case TEXTURE_FORMAT_RGBA_BC7 -> {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_R_BC4;
                else if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_RG_BC5;
                else if (componentCount == 3)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_BC7;
            }
        }

        return targetFormat;
    }

    private static ByteBuffer getByteBuffer(BufferedImage bi) {
        ByteBuffer byteBuffer;
        DataBuffer dataBuffer = bi.getRaster().getDataBuffer();

        if (dataBuffer instanceof DataBufferByte) { // This is the usual case, where data is simply wrapped
            byte[] pixelData = ((DataBufferByte) dataBuffer).getData();
            byteBuffer = ByteBuffer.wrap(pixelData);
        } else if (dataBuffer instanceof DataBufferUShort) {
            short[] pixelData = ((DataBufferUShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        } else if (dataBuffer instanceof DataBufferShort) {
            short[] pixelData = ((DataBufferShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        } else if (dataBuffer instanceof DataBufferInt) {
            int[] pixelData = ((DataBufferInt) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 4);
            byteBuffer.asIntBuffer().put(IntBuffer.wrap(pixelData));
        } else {
            throw new IllegalArgumentException("Not implemented for data buffer type: " + dataBuffer.getClass());
        }

        return byteBuffer;
    }

    private static ITextureCompressor getDefaultTextureCompressor() {
        return TextureCompression.getCompressor(TextureCompressorUncompressed.TextureCompressorName);
    }

    private static boolean isFloatTextureFormat(TextureFormat textureFormat) {
        switch (textureFormat) {
            case TEXTURE_FORMAT_RGBA16F:
            case TEXTURE_FORMAT_RGBA32F:
                return true;
            default:
                return false;
        }
    }

    private static boolean supportsASTCTextureFormat(TextureFormat textureFormat) {
        ITextureCompressor textureCompressor = TextureCompression.getCompressor(TextureCompressorASTC.TextureCompressorName);
        return textureCompressor != null && textureCompressor.supportsTextureFormat(textureFormat);
    }

    private static boolean isHDRTextureFormat(TextureFormat textureFormat) {
        return isFloatTextureFormat(textureFormat) || supportsASTCTextureFormat(textureFormat);
    }

    private static int getHDRPixelFormat(TextureFormat textureFormat) throws TextureGeneratorException {
        switch (textureFormat) {
            case TEXTURE_FORMAT_RGBA16F:
                return Texc.PixelFormat.PF_RGBA16F.getValue();
            case TEXTURE_FORMAT_RGBA32F:
                return Texc.PixelFormat.PF_RGBA32F.getValue();
            default:
                Integer pixelFormat = pixelFormatLUT.get(textureFormat);
                if (supportsASTCTextureFormat(textureFormat) && pixelFormat != null) {
                    return pixelFormat;
                }
                throw new TextureGeneratorException("HDR textures require a float or ASTC texture format.");
        }
    }

    private static DecodedImage createDecodedImage(Texc.Image hdrImage) {
        DecodedImage source = new DecodedImage();
        source.hdr = true;
        source.path = hdrImage.path;
        source.width = hdrImage.width;
        source.height = hdrImage.height;
        source.componentCount = 4;
        source.texcPixelFormat = hdrImage.pixelFormat.getValue();
        source.texcColorSpace = hdrImage.colorSpace.getValue();
        source.data = hdrImage.data;
        return source;
    }

    private static TextureGenerationSettings createTextureGenerationSettings(DecodedImage source,
        TextureFormat textureFormat, String compressorName, String compressorPresetName,
        boolean generateMipMaps, int maxTextureSize, boolean premulAlpha) throws TextureGeneratorException {

        TextureGenerationSettings settings = new TextureGenerationSettings();
        settings.compressorName = compressorName;
        settings.compressorPresetName = compressorPresetName;
        settings.generateMipMaps = generateMipMaps;
        settings.maxTextureSize = maxTextureSize;

        if (source.hdr) {
            if (!isHDRTextureFormat(textureFormat)) {
                throw new TextureGeneratorException("HDR textures require RGBA16F, RGBA32F, or ASTC texture formats.");
            }

            settings.textureFormat = textureFormat;
            settings.outputPixelFormat = getHDRPixelFormat(textureFormat);
            settings.premulAlpha = false;
            settings.powerOfTwo = false;
            settings.squarePVRTC = false;
            settings.alignToCompressor = supportsASTCTextureFormat(textureFormat);
            settings.dither = false;
            settings.compressionInputPixelFormat = Texc.PixelFormat.PF_RGBA32F.getValue();
            settings.createProfileName = "Create HDR Texture";
            settings.resizeErrorMessage = "could not resize HDR texture";
            settings.flipErrorMessage = "could not flip HDR texture on ";
            settings.encodeErrorMessage = "could not encode HDR texture";
        } else {
            textureFormat = textureFormatToSupportedTextureFormat(textureFormat);
            Integer pixelFormat = pixelFormatLUT.get(textureFormat);
            if (pixelFormat == null) {
                throw new TextureGeneratorException("Invalid texture format.");
            }

            settings.textureFormat = textureFormat;
            settings.outputPixelFormat = pixelFormat;
            settings.premulAlpha = premulAlpha;
            settings.powerOfTwo = true;
            settings.squarePVRTC = true;
            settings.alignToCompressor = true;
            settings.dither = true;
            settings.compressionInputPixelFormat = Texc.PixelFormat.PF_R8G8B8A8.getValue();
            settings.createProfileName = "CreateTexture";
            settings.resizeErrorMessage = "could not resize texture to POT";
            settings.flipErrorMessage = "could not flip on ";
            settings.encodeErrorMessage = "could not encode";
        }

        return settings;
    }

    private static List<byte[]> generateFromDecodedImage(TextureImage.Image.Builder builder, DecodedImage source, TextureGenerationSettings settings, EnumSet<FlipAxis> flipAxis, int firstMipLevel) throws TextureGeneratorException {

        if (source.ktx2 != null) {
            try {
                return generateFromKtx2(builder, source, settings, flipAxis);
            } catch (IOException e) {
                throw new TextureGeneratorException(e.getMessage());
            }
        }

        Logger logger = Logger.getLogger(TextureGenerator.class.getName());

        ITextureCompressor textureCompressor = TextureCompression.getCompressor(settings.compressorName);

        if (textureCompressor == null) {
            if (!settings.compressorName.equals(TextureCompressorUncompressed.TextureCompressorName)) {
                logger.warning(String.format("Texture compressor '%s' not found, using the default texture compressor.", settings.compressorName));
            }
            textureCompressor = getDefaultTextureCompressor();
            settings.compressorPresetName = TextureCompressorUncompressed.TextureCompressorUncompressedPresetName;
        }

        TextureCompressorPreset textureCompressorPreset = TextureCompression.getPreset(settings.compressorPresetName);
        if (textureCompressorPreset == null) {
            throw new TextureGeneratorException("Texture compressor preset '" + settings.compressorPresetName + "' not found.");
        }

        if (!textureCompressor.supportsTextureFormat(settings.textureFormat)) {
            throw new TextureGeneratorException("Texture compressor doesn't support the texture format " + settings.textureFormat);
        }

        if (!textureCompressor.supportsTextureCompressorPreset(textureCompressorPreset)) {
            throw new TextureGeneratorException("Texture compressor doesn't support the texture compressor preset " + settings.compressorPresetName);
        }

        TimeProfiler.start(settings.createProfileName);
        long textureImage = TexcLibraryJni.CreateImage(source.path, source.width, source.height, source.texcPixelFormat, source.texcColorSpace, source.data);
        TimeProfiler.stop();
        if (textureImage == 0) {
            throw new TextureGeneratorException("Failed to create texture");
        }

        try {
            int[] dimensions = textureDimensions(source, settings, textureCompressor);
            int newWidth = dimensions[0];
            int newHeight = dimensions[1];

            if (settings.premulAlpha && !source.premultiplied) {
                TimeProfiler.start("PreMultiplyAlpha");
                if (!TexcLibraryJni.PreMultiplyAlpha(textureImage)) {
                    throw new TextureGeneratorException("could not premultiply alpha");
                }
                TimeProfiler.stop();
            }

            if (source.width != newWidth || source.height != newHeight) {
                TimeProfiler.start("Resize");
                long resizedTextureImage = TexcLibraryJni.Resize(textureImage, newWidth, newHeight, source.srgbFiltering, settings.premulAlpha);
                if (resizedTextureImage == 0) {
                    throw new TextureGeneratorException(settings.resizeErrorMessage);
                }
                TexcLibraryJni.DestroyImage(textureImage);
                textureImage = resizedTextureImage;
                TimeProfiler.stop();
            }

            for (Texc.FlipAxis flip : flipAxis) {
                TimeProfiler.start("FlipAxis");
                if (!TexcLibraryJni.Flip(textureImage, flip.getValue())) {
                    throw new TextureGeneratorException(settings.flipErrorMessage + flip);
                }
                TimeProfiler.stop();
            }

            if (settings.dither && (settings.outputPixelFormat == Texc.PixelFormat.PF_R4G4B4A4.getValue() || settings.outputPixelFormat == Texc.PixelFormat.PF_R5G6B5.getValue())) {
                TimeProfiler.start("Dither");
                if (!TexcLibraryJni.Dither(textureImage, settings.outputPixelFormat)) {
                    throw new TextureGeneratorException("could not dither image");
                }
                TimeProfiler.stop();
            }

            builder.setWidth(newWidth)
                    .setHeight(newHeight)
                    .setOriginalWidth(source.width)
                    .setOriginalHeight(source.height)
                    .setFormat(settings.textureFormat);

            List<byte[]> imageDatas = new ArrayList<>();
            int offset = 0;
            int mipMapLevel = firstMipLevel;

            List<Long> mipImages = GenerateImages(textureImage, newWidth, newHeight, settings.generateMipMaps, source.srgbFiltering, settings.premulAlpha);
            TimeProfiler.start("textureCompressor.compress");
            TimeProfiler.addData("mips count", mipImages.size());

            for (Long mipImage : mipImages) {
                byte[] uncompressed = TexcLibraryJni.GetData(mipImage);
                int mipWidth = TexcLibraryJni.GetWidth(mipImage);
                int mipHeight = TexcLibraryJni.GetHeight(mipImage);
                String paramsName = "MipMap_" + mipMapLevel;

                TextureCompressorParams params = new TextureCompressorParams(paramsName, mipMapLevel, mipWidth, mipHeight, 0, source.componentCount, settings.compressionInputPixelFormat, settings.outputPixelFormat, source.texcColorSpace);
                byte[] encodedMipData;
                if (settings.ktx2Encoder == null) {
                    encodedMipData = textureCompressor.compress(textureCompressorPreset, params, uncompressed);
                } else {
                    try {
                        encodedMipData = settings.ktx2Encoder.encodeMip(mipWidth, mipHeight, uncompressed);
                    } catch (IOException e) {
                        throw new TextureGeneratorException(e.getMessage());
                    }
                }
                if (encodedMipData.length == 0) {
                    throw new TextureGeneratorException(settings.encodeErrorMessage);
                }

                imageDatas.add(encodedMipData);
                builder.addMipMapOffset(offset);
                builder.addMipMapSize(encodedMipData.length);
                builder.addMipMapSizeCompressed(encodedMipData.length);
                builder.addMipMapDimensions(textureCompressor.getAlignedWidth(settings.textureFormat, mipWidth));
                builder.addMipMapDimensions(textureCompressor.getAlignedHeight(settings.textureFormat, mipHeight));

                offset += encodedMipData.length;
                mipMapLevel++;
            }

            TimeProfiler.stop();
            builder.setDataSize(offset);
            builder.setFormat(settings.textureFormat);

            for (Long mipImage : mipImages) {
                if (mipImage != textureImage) {
                    TexcLibraryJni.DestroyImage(mipImage);
                }
            }

            return imageDatas;
        } finally {
            TexcLibraryJni.DestroyImage(textureImage);
        }
    }

    private static int[] textureDimensions(DecodedImage source, TextureGenerationSettings settings, ITextureCompressor textureCompressor) {
        int newWidth = source.width;
        int newHeight = source.height;

        if (settings.powerOfTwo) {
            newWidth = TextureUtil.closestPOT(newWidth);
            newHeight = TextureUtil.closestPOT(newHeight);
        }

        if (settings.maxTextureSize > 0) {
            while (newWidth > settings.maxTextureSize || newHeight > settings.maxTextureSize) {
                newWidth = Math.max(newWidth / 2, 1);
                newHeight = Math.max(newHeight / 2, 1);
            }

            assert(newWidth <= settings.maxTextureSize && newHeight <= settings.maxTextureSize);
        }

        if (settings.squarePVRTC &&
            (newHeight != newWidth) &&
            (settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 ||
            settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1 ||
            settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 ||
            settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1)) {

            Logger.getLogger(TextureGenerator.class.getName()).warning("PVR compressed texture is not square and will be resized.");

            newWidth = Math.max(newWidth, newHeight);
            newHeight = newWidth;
        }

        if (settings.alignToCompressor) {
            newWidth = textureCompressor.getAlignedWidth(settings.textureFormat, newWidth);
            newHeight = textureCompressor.getAlignedHeight(settings.textureFormat, newHeight);
        }
        return new int[] { newWidth, newHeight };
    }

    /** Tests the KTX2 signature without loading the native texture compiler. */
    public static boolean isKtx2(byte[] data) {
        byte[] magic = {(byte)0xab, 0x4b, 0x54, 0x58, 0x20, 0x32, 0x30, (byte)0xbb, 0x0d, 0x0a, 0x1a, 0x0a};
        if (data.length < magic.length) return false;
        for (int i = 0; i < magic.length; ++i) {
            if (data[i] != magic[i]) return false;
        }
        return true;
    }

    private static DecodedImage decodeKtx2Mip(DecodedImage source, int level, boolean premultiplyAlpha) throws IOException {
        TexcLibraryJni.Ktx2Texture texture = source.ktx2;
        DecodedImage decoded = new DecodedImage();
        decoded.path = source.path;
        decoded.width = Math.max(1, texture.width >> level);
        decoded.height = Math.max(1, texture.height >> level);
        decoded.componentCount = source.componentCount;
        decoded.texcPixelFormat = source.texcPixelFormat;
        decoded.texcColorSpace = source.texcColorSpace;
        decoded.srgbFiltering = texture.srgb;
        decoded.premultiplied = texture.premultiplied;
        decoded.data = texture.decodeMip(level);
        if (decoded.premultiplied && !premultiplyAlpha) {
            for (int i = 0; i < decoded.data.length; i += 4) {
                int alpha = decoded.data[i + 3] & 0xff;
                for (int c = 0; c < 3; ++c) {
                    int value = decoded.data[i + c] & 0xff;
                    decoded.data[i + c] = (byte)(alpha == 0 ? 0 : Math.min(255, (value * 255 + alpha / 2) / alpha));
                }
            }
            decoded.premultiplied = false;
        }
        return decoded;
    }

    private static List<byte[]> generateFromKtx2(TextureImage.Image.Builder builder, DecodedImage source,
                                                TextureGenerationSettings settings, EnumSet<FlipAxis> flipAxis)
            throws TextureGeneratorException, IOException {
        TexcLibraryJni.Ktx2Texture texture = source.ktx2;
        ITextureCompressor compressor = TextureCompression.getCompressor(settings.compressorName);
        TextureCompressorPreset preset = TextureCompression.getPreset(settings.compressorPresetName);
        if (compressor == null || preset == null || !compressor.supportsTextureFormat(settings.textureFormat)
                || !compressor.supportsTextureCompressorPreset(preset)) {
            throw new TextureGeneratorException("Invalid KTX2 texture compressor, preset, or output format.");
        }
        int[] dimensions = textureDimensions(source, settings, compressor);
        int firstLevel = -1;
        for (int level = 0; level < texture.levelCount; ++level) {
            if (Math.max(1, texture.width >> level) == dimensions[0]
                    && Math.max(1, texture.height >> level) == dimensions[1]) {
                firstLevel = level;
                break;
            }
        }
        EnumSet<FlipAxis> flips = ktx2FlipAxes(texture, flipAxis);
        if (firstLevel < 0) {
            DecodedImage decoded = decodeKtx2Mip(source, 0, settings.premulAlpha);
            List<byte[]> result = generateFromDecodedImage(builder, decoded, settings, flips, 0);
            builder.setOriginalWidth(source.width).setOriginalHeight(source.height);
            return result;
        }

        boolean preserve = texture.canRepack && !settings.recompress && flips.isEmpty()
                && (texture.channels == 3 || texture.premultiplied == settings.premulAlpha)
                && (settings.ktx2Encoder != null || (texture.vkFormat == 0 && texture.supercompression != 1
                    && settings.compressorName.equals(TextureCompressorBasisU.TextureCompressorName)))
                && (settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGB || settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA)
                && (texture.channels != 4 || settings.textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA);
        builder.setWidth(dimensions[0]).setHeight(dimensions[1])
                .setOriginalWidth(source.width).setOriginalHeight(source.height).setFormat(settings.textureFormat);
        List<byte[]> result = new ArrayList<>();
        int offset = 0;
        int lastLevel = settings.generateMipMaps && !settings.regenerateMipmaps ? texture.levelCount - 1 : firstLevel;
        for (int level = firstLevel; level <= lastLevel; ++level) {
            int width = Math.max(1, texture.width >> level);
            int height = Math.max(1, texture.height >> level);
            boolean completeTail = settings.generateMipMaps && level == lastLevel && (width > 1 || height > 1);
            TextureImage.Image.Builder mipBuilder = TextureImage.Image.newBuilder();
            List<byte[]> mipData;
            if (preserve) {
                byte[] bytes = texture.repackMip(level);
                mipData = new ArrayList<>();
                mipData.add(bytes);
                mipBuilder.addMipMapDimensions(width).addMipMapDimensions(height);
            } else {
                TextureGenerationSettings mipSettings = createTextureGenerationSettings(source, settings.textureFormat,
                        settings.compressorName, settings.compressorPresetName, completeTail, 0, settings.premulAlpha);
                // Authored mip sizes have already been validated against the target dimensions.
                mipSettings.powerOfTwo = false;
                mipSettings.alignToCompressor = false;
                mipSettings.ktx2Encoder = settings.ktx2Encoder;
                mipData = generateFromDecodedImage(mipBuilder, decodeKtx2Mip(source, level, settings.premulAlpha), mipSettings, flips, level - firstLevel);
            }
            for (int mip = 0; mip < mipData.size(); ++mip) {
                byte[] bytes = mipData.get(mip);
                result.add(bytes);
                builder.addMipMapOffset(offset).addMipMapSize(bytes.length).addMipMapSizeCompressed(bytes.length)
                        .addMipMapDimensions(mipBuilder.getMipMapDimensions(mip * 2))
                        .addMipMapDimensions(mipBuilder.getMipMapDimensions(mip * 2 + 1));
                offset += bytes.length;
            }
            if (preserve && completeTail) {
                // Generate only the missing descendants; never replace the last authored level.
                TextureGenerationSettings tailSettings = createTextureGenerationSettings(source, settings.textureFormat,
                        settings.compressorName, settings.compressorPresetName, true, 0, settings.premulAlpha);
                tailSettings.powerOfTwo = false;
                tailSettings.alignToCompressor = false;
                tailSettings.ktx2Encoder = settings.ktx2Encoder;
                TextureImage.Image.Builder tail = TextureImage.Image.newBuilder();
                List<byte[]> tailData = generateFromDecodedImage(tail, decodeKtx2Mip(source, level, settings.premulAlpha), tailSettings, flips, level - firstLevel);
                for (int mip = 1; mip < tailData.size(); ++mip) {
                    byte[] bytes = tailData.get(mip);
                    result.add(bytes);
                    builder.addMipMapOffset(offset).addMipMapSize(bytes.length).addMipMapSizeCompressed(bytes.length)
                            .addMipMapDimensions(tail.getMipMapDimensions(mip * 2))
                            .addMipMapDimensions(tail.getMipMapDimensions(mip * 2 + 1));
                    offset += bytes.length;
                }
            }
        }
        builder.setDataSize(offset);
        return result;
    }

    private static List<Long> GenerateImages(long image, int width, int height, boolean generateMipChain, boolean srgb, boolean premultiplied) throws TextureGeneratorException {
        TimeProfiler.start("GenerateImages");
        List<Long> images = new ArrayList<>();
        int baseWidth = TexcLibraryJni.GetWidth(image);
        int baseHeight = TexcLibraryJni.GetHeight(image);
        boolean baseMatches = baseWidth == width && baseHeight == height;

        // Use the provided image as mip0 if it matches the requested size; otherwise resize once to seed the chain.
        if (baseMatches) {
            images.add(image);
        } else {
            long resizedBase = TexcLibraryJni.Resize(image, width, height, srgb, premultiplied);
            if (resizedBase == 0) {
                throw new TextureGeneratorException("Failed to create mipmap 0");
            }
            images.add(resizedBase);
        }
        if (!generateMipChain) {
            TimeProfiler.stop();
            return images;
        }

        long prevImage = images.get(images.size() - 1);
        for (int mipLevel = 1, nextWidth = width / 2, nextHeight = height / 2; nextWidth > 0 || nextHeight > 0; mipLevel++, nextWidth /= 2, nextHeight /= 2) {
            int mipWidth = Math.max(nextWidth, 1);
            int mipHeight = Math.max(nextHeight, 1);
            long resizedImage;

            TimeProfiler.start("ResizeMipLevel" + mipLevel);
            resizedImage = TexcLibraryJni.Resize(prevImage, mipWidth, mipHeight, srgb, premultiplied);
            if (resizedImage == 0) {
                throw new TextureGeneratorException("Failed to create mipmap " + mipLevel);
            }
            TimeProfiler.stop();

            images.add(resizedImage);

            prevImage = resizedImage;
        }
        TimeProfiler.stop();
        return images;
    }

    private static DecodedImage createDecodedImage(BufferedImage image, ColorModel colorModel) {
        ByteBuffer byteBuffer = getByteBuffer(image);
        DecodedImage source = new DecodedImage();
        source.hdr = false;
        source.path = null;
        source.width = image.getWidth();
        source.height = image.getHeight();
        source.componentCount = colorModel.getNumComponents();
        source.texcPixelFormat = Texc.PixelFormat.PF_A8B8G8R8.getValue();
        source.texcColorSpace = Texc.ColorSpace.CS_SRGB.getValue();
        source.data = byteBuffer.array();
        return source;
    }

    // For convenience, some methods without the flipAxis and/or compress argument.
    // It will always try to flip on Y axis since this is the byte order that OpenGL expects for regular/most textures,
    // for those methods without this argument.
    public static GenerateResult generate(byte[] data, TextureProfile texProfile, boolean compress) throws TextureGeneratorException, IOException {
        return generate(data, texProfile, compress, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static GenerateResult generate(byte[] data, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException, IOException {
        if (isKtx2(data)) {
            try (TexcLibraryJni.Ktx2Texture texture = TexcLibraryJni.LoadKtx2(data)) {
                return generate(createDecodedImage(texture), texProfile, compress, flipAxis);
            }
        }
        if (TexcLibraryJni.IsHDR(data)) {
            TimeProfiler.start("Load HDR Texture");
            Texc.Image hdrImage = TexcLibraryJni.CreateImageFromBuffer(data);
            TimeProfiler.stop();
            if (hdrImage == null) {
                throw new TextureGeneratorException("Failed to load HDR texture");
            }
            return generate(createDecodedImage(hdrImage), texProfile, compress, flipAxis);
        }

        return generate(new ByteArrayInputStream(data), texProfile, compress, flipAxis);
    }

    private static DecodedImage createDecodedImage(TexcLibraryJni.Ktx2Texture texture) {
        DecodedImage source = new DecodedImage();
        source.path = "KTX2";
        source.width = texture.width;
        source.height = texture.height;
        // R and RG are data channels, not luminance and luminance-alpha.
        source.componentCount = texture.channels == 3 ? 3 : 4;
        source.texcPixelFormat = Texc.PixelFormat.PF_R8G8B8A8.getValue();
        source.texcColorSpace = (texture.srgb ? Texc.ColorSpace.CS_SRGB : Texc.ColorSpace.CS_LRGB).getValue();
        source.ktx2 = texture;
        return source;
    }

    private static EnumSet<FlipAxis> ktx2FlipAxes(TexcLibraryJni.Ktx2Texture texture, EnumSet<FlipAxis> flipAxis) {
        EnumSet<FlipAxis> flips = flipAxis.clone();
        if (texture.flipX && !flips.remove(FlipAxis.FLIP_AXIS_X)) flips.add(FlipAxis.FLIP_AXIS_X);
        if (texture.flipY && !flips.remove(FlipAxis.FLIP_AXIS_Y)) flips.add(FlipAxis.FLIP_AXIS_Y);
        return flips;
    }

    /** Previews one authored mip at its original size, with editor orientation and premultiplied alpha. */
    public static GenerateResult generateKtx2MipPreview(TexcLibraryJni.Ktx2Texture texture, int level)
            throws TextureGeneratorException, IOException {
        DecodedImage source = createDecodedImage(texture);
        TextureGenerationSettings settings = createTextureGenerationSettings(source, TextureFormat.TEXTURE_FORMAT_RGBA,
                TextureCompressorUncompressed.TextureCompressorName, TextureCompressorUncompressed.GetMigratedCompressionPreset(),
                false, 0, true);
        settings.powerOfTwo = false;
        settings.alignToCompressor = false;
        TextureImage.Image.Builder image = TextureImage.Image.newBuilder();
        GenerateResult result = new GenerateResult();
        result.imageDatas = new ArrayList<>(generateFromDecodedImage(image, decodeKtx2Mip(source, level, true), settings,
                ktx2FlipAxes(texture, EnumSet.of(FlipAxis.FLIP_AXIS_Y)), 0));
        result.textureImage = TextureImage.newBuilder().setType(TextureImage.Type.TYPE_2D).setCount(1)
                .addAlternatives(image).build();
        return result;
    }

    public static GenerateResult generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, null, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static GenerateResult generate(InputStream inputStream, TextureProfile texProfile) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, texProfile, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static GenerateResult generate(InputStream inputStream, TextureProfile texProfile, boolean compress) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        if (origImage == null) {
            throw new TextureGeneratorException("Unknown texture format.");
        }
        return generate(origImage, texProfile, compress, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static GenerateResult generate(InputStream inputStream, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, texProfile, compress, flipAxis);
    }

    private static GenerateResult generate(DecodedImage source, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException {
        TextureImage.Builder textureBuilder = TextureImage.newBuilder();
        GenerateResult result = new GenerateResult();
        result.imageDatas = new ArrayList<>();

        if (texProfile != null) {
            for (PlatformProfile platformProfile : texProfile.getPlatformsList()) {
                if (source.ktx2 != null && platformProfile.getKeepKtx2Format()) {
                    TextureImage.Image.Builder imageBuilder = TextureImage.Image.newBuilder();
                    TextureGenerationSettings settings = createTextureGenerationSettings(source,
                            pickOptimalFormat(source.componentCount, TextureFormat.TEXTURE_FORMAT_RGBA),
                            TextureCompressorUncompressed.TextureCompressorName,
                            TextureCompressorUncompressed.TextureCompressorUncompressedPresetName,
                            platformProfile.getMipmaps(), platformProfile.getMaxTextureSize(), platformProfile.getPremultiplyAlpha());
                    settings.powerOfTwo = false;
                    settings.alignToCompressor = false;
                    settings.recompress = platformProfile.getRecompress();
                    settings.regenerateMipmaps = platformProfile.getRegenerateMipmaps();
                    TexcLibraryJni.Ktx2Texture texture = source.ktx2;
                    if (compress && (texture.vkFormat == 0 || texture.vkFormat >= 145)) {
                        settings.ktx2Encoder = texture;
                    }
                    List<byte[]> imageDatas = generateFromDecodedImage(imageBuilder, source, settings, flipAxis, 0);
                    if (settings.ktx2Encoder != null && texture.vkFormat == 0) {
                        imageBuilder.setCompressionType(texture.supercompression == 1
                                ? TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_ETC1S
                                : TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC);
                    } else {
                        imageBuilder.setCompressionType(TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT);
                        if (settings.ktx2Encoder != null) imageBuilder.setFormat(TextureFormat.TEXTURE_FORMAT_RGBA_BC7);
                    }
                    textureBuilder.addAlternatives(imageBuilder);
                    result.imageDatas.addAll(imageDatas);
                    continue;
                }
                for (TextureFormatAlternative formatAlternative : platformProfile.getFormatsList()) {
                    TextureFormat textureFormat = formatAlternative.getFormat();

                    if (source.hdr) {
                        if (!isHDRTextureFormat(textureFormat)) {
                            continue;
                        }
                    } else {
                        // Prefer an output format matching the number of source channels.
                        textureFormat = pickOptimalFormat(source.componentCount, textureFormat);
                    }

                    String textureCompressor = formatAlternative.getCompressor();
                    String textureCompressorPreset = formatAlternative.getCompressorPreset();
                    TextureImage.CompressionType compressionType;

                    if (compress) {
                        if (textureCompressor.isEmpty()) {
                            compressionType = textureFormatToSupportedCompressionTypeOrDefault(textureFormat, formatAlternative.getCompressionType());
                        } else {
                            compressionType = textureCompressorToCompressionType(textureCompressor);
                        }

                        if (textureCompressorPreset.isEmpty()) {
                            textureCompressor = compressionTypeToTextureCompressor(compressionType);
                            textureCompressorPreset = compressionLevelToTextureCompressorPreset(compressionType, formatAlternative.getCompressionLevel());
                        }
                    } else {
                        if (source.hdr) {
                            if (supportsASTCTextureFormat(textureFormat)) {
                                textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA32F;
                            }
                        } else {
                            textureFormat = pickUncompressedFormat(textureFormat);
                        }
                        compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
                        textureCompressor = compressionTypeToTextureCompressor(compressionType);
                        textureCompressorPreset = compressionLevelToTextureCompressorPreset(compressionType, TextureFormatAlternative.CompressionLevel.NORMAL);
                    }

                    TextureImage.Image.Builder imageBuilder = TextureImage.Image.newBuilder();
                    TextureGenerationSettings settings = createTextureGenerationSettings(source,
                                                                                         textureFormat,
                                                                                         textureCompressor,
                                                                                         textureCompressorPreset,
                                                                                         platformProfile.getMipmaps(),
                                                                                         platformProfile.getMaxTextureSize(),
                                                                                         !source.hdr && platformProfile.getPremultiplyAlpha());
                    settings.recompress = platformProfile.getRecompress();
                    settings.regenerateMipmaps = platformProfile.getRegenerateMipmaps();
                    List<byte[]> imageDatas = generateFromDecodedImage(imageBuilder, source, settings, flipAxis, 0);
                    imageBuilder.setCompressionType(compressionType);
                    textureBuilder.addAlternatives(imageBuilder);
                    result.imageDatas.addAll(imageDatas);
                }
            }

            if (source.hdr && textureBuilder.getAlternativesCount() == 0) {
                throw new TextureGeneratorException("HDR textures require a texture profile with RGBA16F, RGBA32F, or ASTC texture formats.");
            }
        }

        if (textureBuilder.getAlternativesCount() == 0) {
            TextureFormat textureFormat = source.hdr
                    ? TextureFormat.TEXTURE_FORMAT_RGBA32F
                    : pickOptimalFormat(source.componentCount, TextureFormat.TEXTURE_FORMAT_RGBA);
            TextureImage.Image.Builder imageBuilder = TextureImage.Image.newBuilder();
            TextureGenerationSettings settings = createTextureGenerationSettings(source,
                                                                                 textureFormat,
                                                                                 TextureCompressorUncompressed.TextureCompressorName,
                                                                                 TextureCompressorUncompressed.TextureCompressorUncompressedPresetName,
                                                                                 true,
                                                                                 0,
                                                                                 !source.hdr);
            List<byte[]> imageDatas = generateFromDecodedImage(imageBuilder, source, settings, flipAxis, 0);
            imageBuilder.setCompressionType(TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT);
            textureBuilder.addAlternatives(imageBuilder);
            result.imageDatas.addAll(imageDatas);
        }

        textureBuilder.setCount(1).setType(Type.TYPE_2D);
        result.textureImage = textureBuilder.build();
        return result;
    }

    public static GenerateResult generate(BufferedImage origImage, TextureProfile texProfile, boolean compress) throws TextureGeneratorException, IOException {
        return generate(origImage, texProfile, compress, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    private static TextureImage.CompressionType textureFormatToSupportedCompressionTypeOrDefault(TextureImage.TextureFormat format, TextureImage.CompressionType defaultType) {
        // Issue 5753: Since we currently don't support pre compressed hardware textures, so we use UASTC instead
        if (format == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGB_ETC1) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
        } else if (format == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
        }
        return defaultType;
    }

    private static TextureImage.TextureFormat textureFormatToSupportedTextureFormat(TextureImage.TextureFormat format) {
        // Issue 5753: Since we currently don't support pre compressed hardware textures, so we use UASTC instead
        if (format == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGB_ETC1) {
            return TextureFormat.TEXTURE_FORMAT_RGB;
        } else if (format == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 || format == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1) {
            return TextureFormat.TEXTURE_FORMAT_RGBA;
        }
        return format;
    }

    private static String compressionTypeToTextureCompressor(TextureImage.CompressionType type) {
        return switch (type) {
            case COMPRESSION_TYPE_DEFAULT,
                 COMPRESSION_TYPE_WEBP -> TextureCompressorUncompressed.TextureCompressorName;
            case COMPRESSION_TYPE_BASIS_ETC1S,
                 COMPRESSION_TYPE_BASIS_UASTC,
                 COMPRESSION_TYPE_WEBP_LOSSY -> TextureCompressorBasisU.TextureCompressorName;
            case COMPRESSION_TYPE_ASTC -> TextureCompressorASTC.TextureCompressorName;
        };
    }

    private static String compressionLevelToTextureCompressorPreset(TextureImage.CompressionType type, TextureFormatAlternative.CompressionLevel level) {
        // Convert from basis to basis preset
        if (type == TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC || type == TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_ETC1S) {
            return TextureCompressorBasisU.GetMigratedCompressionPreset(level);
        } else if (type == TextureImage.CompressionType.COMPRESSION_TYPE_ASTC) {
            return TextureCompressorASTC.GetMigratedCompressionPreset(level);
        } else if (type == TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT) {
            return TextureCompressorUncompressed.GetMigratedCompressionPreset();
        }
        return null;
    }

    private static TextureImage.CompressionType textureCompressorToCompressionType(String compressor) {
        if (compressor.equals(TextureCompressorUncompressed.TextureCompressorName)) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
        } else if (compressor.equals(TextureCompressorBasisU.TextureCompressorName)) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
        } else if (compressor.equals(TextureCompressorASTC.TextureCompressorName)) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_ASTC;
        }
        // TODO: This shouldn't be needed eventually, but right now we need a compression type in the engine.
        return TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
    }

    // NOTE: This is being used to generate atlas and tilesource previews in the Editor, avoiding some of the extra
    // overhead of the generalized path, which helps since the editor currently blocks the UI during this operation
    public static GenerateResult generateAtlasPreview(BufferedImage image) throws TextureGeneratorException {
        int width = image.getWidth();
        int height = image.getHeight();
        byte[] inputBuffer = getByteBuffer(image).array();
        byte[] outputBuffer = new byte[width * height * 4];

        int res = TexcLibraryJni.CreatePreviewImage(width, height, inputBuffer, outputBuffer);
        if (res == -1) {
            throw new TextureGeneratorException("Failed to create texture");
        }

        var result = new GenerateResult();
        result.imageDatas = new ArrayList<>();
        result.imageDatas.add(outputBuffer);

        result.textureImage = TextureImage.newBuilder()
            .addAlternatives(TextureImage.Image.newBuilder()
                             .setWidth(width)
                             .setHeight(height)
                             .setOriginalWidth(width)
                             .setOriginalHeight(height)
                             .setFormat(TextureFormat.TEXTURE_FORMAT_RGBA)
                             .addMipMapOffset(0)
                             .addMipMapSize(outputBuffer.length)
                             .setDataSize(outputBuffer.length))
            .setType(Type.TYPE_2D)
            .setCount(1)
            .build();

        return result;
    }

    // Main TextureGenerator.generate method that has all required arguments and the expected BufferedImage type for origImage.
    // Used by the editor
    public static GenerateResult generate(BufferedImage origImage, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException {
        TimeProfiler.start("generateTexture");

        // Always convert to ABGR since the texc lib demands that for resizing etc.
        BufferedImage image;
        if (origImage.getType() != BufferedImage.TYPE_4BYTE_ABGR) {
            image = convertImage(origImage, BufferedImage.TYPE_4BYTE_ABGR);
        } else {
            image = origImage;
        }

        GenerateResult result = generate(createDecodedImage(image, origImage.getColorModel()), texProfile, compress, flipAxis);
        TimeProfiler.stop();
        return result;
    }

    public static void main(String[] args) throws IOException, TextureGeneratorException {
        System.setProperty("java.awt.headless", "true");

        // Install default texture compressors
        TextureCompression.registerCompressor(new TextureCompressorBasisU());
        TextureCompression.registerCompressor(new TextureCompressorASTC());

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
             BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {
            GenerateResult result = generate(is);
            TextureUtil.writeGenerateResultToOutputStream(result, os);
        }
    }
}
