// Copyright 2020-2024 The Defold Foundation
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
import com.dynamo.bob.Project;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.util.TimeProfiler;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.google.protobuf.ByteString;

public class TextureGenerator {

    // specify what is maximum of threads TextureGenerator may use
    public static int maxThreads = Project.getDefaultMaxCpuThreads();

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
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_4x4, Texc.PixelFormat.PF_RGBA_ASTC_4x4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_BC1, Texc.PixelFormat.PF_RGB_BC1.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_BC3, Texc.PixelFormat.PF_RGBA_BC3.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_R_BC4, Texc.PixelFormat.PF_R_BC4.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RG_BC5, Texc.PixelFormat.PF_RG_BC5.getValue());
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_BC7, Texc.PixelFormat.PF_RGBA_BC7.getValue());
    }

    private static BufferedImage convertImage(BufferedImage origImage, int type) {
        BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), type);
        Graphics2D g2d = image.createGraphics();
        g2d.drawImage(origImage, 0, 0, null);
        g2d.dispose();
        return image;
    }

    // pickOptimalFormat will try to pick a texture format with the same number of channels as componentCount,
    // while still using a texture format within the same "family".
    private static TextureFormat pickOptimalFormat(int componentCount, TextureFormat targetFormat) {

        switch (targetFormat) {

            // Force down to luminance if only 1 input component
            case TEXTURE_FORMAT_RGB: {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                else if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;
                return TextureFormat.TEXTURE_FORMAT_RGB;
            }

            case TEXTURE_FORMAT_RGBA: {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                else if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;
                else if (componentCount == 3)
                    return TextureFormat.TEXTURE_FORMAT_RGB;

                return TextureFormat.TEXTURE_FORMAT_RGBA;
            }

            // PVRTC with 4 channels
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
            }

            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            }

            case TEXTURE_FORMAT_RGBA_16BPP: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_16BPP;
                return TextureFormat.TEXTURE_FORMAT_RGBA_16BPP;
            }


            case TEXTURE_FORMAT_RGBA_ETC2: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_ETC2;
            }

            case TEXTURE_FORMAT_RGBA_ASTC_4x4: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_ASTC_4x4;
            }

            case TEXTURE_FORMAT_RGBA_BC3: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_BC1;
                return TextureFormat.TEXTURE_FORMAT_RGBA_BC3;
            }

            case TEXTURE_FORMAT_RGBA_BC7: {
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

    private static ByteBuffer getByteBuffer(BufferedImage bi)
    {
        ByteBuffer byteBuffer;
        DataBuffer dataBuffer = bi.getRaster().getDataBuffer();

        if (dataBuffer instanceof DataBufferByte) { // This is the usual case, where data is simply wrapped
            byte[] pixelData = ((DataBufferByte) dataBuffer).getData();
            byteBuffer = ByteBuffer.wrap(pixelData);
        }
        else if (dataBuffer instanceof DataBufferUShort) {
            short[] pixelData = ((DataBufferUShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        }
        else if (dataBuffer instanceof DataBufferShort) {
            short[] pixelData = ((DataBufferShort) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 2);
            byteBuffer.asShortBuffer().put(ShortBuffer.wrap(pixelData));
        }
        else if (dataBuffer instanceof DataBufferInt) {
            int[] pixelData = ((DataBufferInt) dataBuffer).getData();
            byteBuffer = ByteBuffer.allocate(pixelData.length * 4);
            byteBuffer.asIntBuffer().put(IntBuffer.wrap(pixelData));
        }
        else {
            throw new IllegalArgumentException("Not implemented for data buffer type: " + dataBuffer.getClass());
        }

        return byteBuffer;
    }

    private static ITextureCompressor getDefaultTextureCompressor() {
        ITextureCompressor defaultCompressor = TextureCompression.getCompressor("Default");
        if (defaultCompressor == null) {
            defaultCompressor = new TextureCompressorDefault();
            TextureCompression.registerCompressor(defaultCompressor);
        }
        return defaultCompressor;
    }

    private static List<Long> GenerateImages(long image, int width, int height, boolean generateMipChain) throws TextureGeneratorException {
        List<Long> images = new ArrayList<>();
        int mipWidth = width;
        int mipHeight = height;
        int mipLevel = 0;
        long prevImage = image;

        while (mipWidth != 0 || mipHeight != 0) {
            mipWidth = Math.max(mipWidth, 1);
            mipHeight = Math.max(mipHeight, 1);
            long resizedImage;

            TimeProfiler.start("ResizeMipLevel" + mipLevel);
            resizedImage = TexcLibraryJni.Resize(prevImage, mipWidth, mipHeight);
            if (resizedImage == 0) {
                throw new TextureGeneratorException("Failed to create mipmap " + mipLevel);
            }
            TimeProfiler.stop();

            images.add(resizedImage);

            // If we don't need all the mipmap images, just return the first.
            if (!generateMipChain) {
                return images;
            }

            prevImage = resizedImage;

            mipLevel++;
            mipWidth /= 2;
            mipHeight /= 2;
        }

        return images;
    }

    private static TextureImage.Image.Builder generateFromColorAndFormat(String name,
                                                                BufferedImage image,
                                                                ColorModel colorModel,
                                                                TextureFormat textureFormat,
                                                                String compressorName,
                                                                String compressorPresetName,
                                                                boolean generateMipMaps,
                                                                int maxTextureSize,
                                                                boolean premulAlpha,
                                                                EnumSet<Texc.FlipAxis> flipAxis) throws TextureGeneratorException {

        int width = image.getWidth();
        int height = image.getHeight();
        int componentCount = colorModel.getNumComponents();
        Integer pixelFormat;

        Logger logger = Logger.getLogger(TextureGenerator.class.getName());

        // Transform the texture format to a supported texture format
        textureFormat = textureFormatToSupportedTextureFormat(textureFormat);

        // pick a pixel format (for texc) based on the texture format
        pixelFormat = pixelFormatLUT.get(textureFormat);
        if (pixelFormat == null) {
            throw new TextureGeneratorException("Invalid texture format.");
        }

        ByteBuffer byteBuffer = getByteBuffer(image);
        byte[] bytes = byteBuffer.array();

        TimeProfiler.start("CreateTexture");
        long textureImage = TexcLibraryJni.CreateImage(name, width, height,
                                                        Texc.PixelFormat.PF_A8B8G8R8.getValue(),
                                                        Texc.ColorSpace.CS_SRGB.getValue(), bytes);

        TimeProfiler.stop();
        if (textureImage == 0) {
            throw new TextureGeneratorException("Failed to create texture");
        }

        try {

            int newWidth  = image.getWidth();
            int newHeight = image.getHeight();

            // For pvrtc textures
            newWidth = TextureUtil.closestPOT(newWidth);
            newHeight = TextureUtil.closestPOT(newHeight);

            // Shrink sides until width & height fit max texture size specified in tex profile
            if (maxTextureSize > 0) {
                while (newWidth > maxTextureSize || newHeight > maxTextureSize) {
                    newWidth = Math.max(newWidth / 2, 1);
                    newHeight = Math.max(newHeight / 2, 1);
                }

                assert(newWidth <= maxTextureSize && newHeight <= maxTextureSize);
            }

            // PVR textures need to be square on iOS
            if ((newHeight != newWidth) &&
                (textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 ||
                textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1 ||
                textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 ||
                textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1)) {

                logger.warning("PVR compressed texture is not square and will be resized.");

                newWidth = Math.max(newWidth, newHeight);
                newHeight = newWidth;
            }

            // Premultiply before scale so filtering cannot introduce colour artefacts.
            if (premulAlpha && !ColorModel.getRGBdefault().isAlphaPremultiplied()) {
                TimeProfiler.start("PreMultiplyAlpha");
                if (!TexcLibraryJni.PreMultiplyAlpha(textureImage)) {
                    throw new TextureGeneratorException("could not premultiply alpha");
                }
                TimeProfiler.stop();
            }

            // Resize to POT if necessary
            if (width != newWidth || height != newHeight) {
                TimeProfiler.start("Resize");
                long resizedTextureImage = TexcLibraryJni.Resize(textureImage, newWidth, newHeight);
                if (resizedTextureImage == 0) {
                    throw new TextureGeneratorException("could not resize texture to POT");
                }
                textureImage = resizedTextureImage;
                TimeProfiler.stop();
            }

            // Loop over all axis that should be flipped.
            for (Texc.FlipAxis flip : flipAxis) {
                TimeProfiler.start("FlipAxis");
                if (!TexcLibraryJni.Flip(textureImage, flip.getValue())) {
                    throw new TextureGeneratorException("could not flip on " + flip);
                }
                TimeProfiler.stop();
            }

            if (pixelFormat == Texc.PixelFormat.PF_R4G4B4A4.getValue() || pixelFormat == Texc.PixelFormat.PF_R5G6B5.getValue()) {

                TimeProfiler.start("Dither");
                if (!TexcLibraryJni.Dither(textureImage, pixelFormat)) {
                    throw new TextureGeneratorException("could not dither image");
                }
                TimeProfiler.stop();
            }

            ITextureCompressor textureCompressor = TextureCompression.getCompressor(compressorName);

            if (textureCompressor == null) {
                logger.warning(String.format("Texture compressor '%s' not found, using the default texture compressor.", compressorName));
                textureCompressor = getDefaultTextureCompressor();
                compressorPresetName = "DEFAULT";
            }

            TextureCompressorPreset textureCompressorPreset = TextureCompression.getPreset(compressorPresetName);
            if (textureCompressorPreset == null) {
                throw new TextureGeneratorException("Texture compressor preset not found.");
            }

            // Generate output images for builder
            TextureImage.Image.Builder builder = TextureImage.Image.newBuilder()
                    .setWidth(newWidth)
                    .setHeight(newHeight)
                    .setOriginalWidth(width)
                    .setOriginalHeight(height)
                    .setFormat(textureFormat);

            int mipMapLevel = 0;
            int offset = 0;

            List<Long> mipImages = GenerateImages(textureImage, newWidth, newHeight, generateMipMaps);
            List<byte[]> compressedMipImageDatas = new ArrayList<>();

            for (Long mipImage : mipImages) {

                byte[] uncompressed = TexcLibraryJni.GetData(mipImage);
                int mipWidth        = TexcLibraryJni.GetWidth(mipImage);
                int mipHeight       = TexcLibraryJni.GetHeight(mipImage);

                String paramsName = name;
                if (paramsName == null) {
                    paramsName = "MipMap_" + mipMapLevel;
                }

                TextureCompressorParams params = new TextureCompressorParams(paramsName, mipMapLevel, mipWidth, mipHeight, 0, componentCount, Texc.PixelFormat.PF_R8G8B8A8.getValue(), pixelFormat, Texc.ColorSpace.CS_SRGB.getValue());
                byte[] compressedData = textureCompressor.compress(textureCompressorPreset, params, uncompressed);

                if (compressedData.length == 0) {
                    throw new TextureGeneratorException("could not encode");
                }

                compressedMipImageDatas.add(compressedData);
                builder.addMipMapOffset(offset);
                builder.addMipMapSize(compressedData.length);
                builder.addMipMapSizeCompressed(compressedData.length);

                offset += compressedData.length;
                mipMapLevel++;
            }

            // Copy each slice into the final byte buffer
            // Offset == total size
            byte[] textureData = new byte[offset];
            int currentPos = 0;
            for(byte[] mipImageData : compressedMipImageDatas) {
                System.arraycopy(mipImageData, 0, textureData, currentPos, mipImageData.length);
                currentPos += mipImageData.length;
            }

            // Cleanup the texture images
            for (Long mipImage : mipImages) {
                TexcLibraryJni.DestroyImage(mipImage);
            }

            builder.setData(ByteString.copyFrom(textureData));
            builder.setFormat(textureFormat);

            return builder;

        } finally {
            TexcLibraryJni.DestroyImage(textureImage);
        }
    }

    // For convenience, some methods without the flipAxis and/or compress argument.
    // It will always try to flip on Y axis since this is the byte order that OpenGL expects for regular/most textures,
    // for those methods without this argument.
    public static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, null, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static TextureImage generate(InputStream inputStream, TextureProfile texProfile) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, texProfile, false, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static TextureImage generate(InputStream inputStream, TextureProfile texProfile, boolean compress) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        if (origImage == null) {
            throw new TextureGeneratorException("Unknown texture format.");
        }
        return generate(origImage, texProfile, compress, EnumSet.of(FlipAxis.FLIP_AXIS_Y));
    }

    public static TextureImage generate(InputStream inputStream, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException, IOException {
        TimeProfiler.start("Read Input Stream");
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        TimeProfiler.stop();
        return generate(origImage, texProfile, compress, flipAxis);
    }

    public static TextureImage generate(BufferedImage origImage, TextureProfile texProfile, boolean compress) throws TextureGeneratorException, IOException {
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
                 COMPRESSION_TYPE_WEBP -> TextureCompressorDefault.TextureCompressorName;
            case COMPRESSION_TYPE_BASIS_ETC1S,
                 COMPRESSION_TYPE_BASIS_UASTC,
                 COMPRESSION_TYPE_WEBP_LOSSY -> TextureCompressorBasisU.TextureCompressorName;
        };
    }

    private static String compressionLevelToTextureCompressorPreset(TextureImage.CompressionType type, TextureFormatAlternative.CompressionLevel level) {
        // Convert from basis to basis preset
        if (type == TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC || type == TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_ETC1S) {
            if (level == TextureFormatAlternative.CompressionLevel.FAST) {
                return "BASISU_FAST";
            } else if (level == TextureFormatAlternative.CompressionLevel.NORMAL) {
                return "BASISU_NORMAL";
            } else if (level == TextureFormatAlternative.CompressionLevel.HIGH) {
                return "BASISU_HIGH";
            } else if (level == TextureFormatAlternative.CompressionLevel.BEST) {
                return "BASISU_BEST";
            }
        }
        else if (type == TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT) {
            return "DEFAULT";
        }
        return null;
    }

    private static TextureImage.CompressionType textureCompressorToCompressionType(String compressor) {
        if (compressor.equals(TextureCompressorDefault.TextureCompressorName)) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
        } else if (compressor.equals(TextureCompressorBasisU.TextureCompressorName)) {
            return TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
        }
        // TODO: This shouldn't be needed eventually, but right now we need a compression type in the engine.
        return TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
    }

    // Main TextureGenerator.generate method that has all required arguments and the expected BufferedImage type for origImage.
    // Used by the editor
    public static TextureImage generate(BufferedImage origImage, TextureProfile texProfile, boolean compress, EnumSet<FlipAxis> flipAxis) throws TextureGeneratorException {
        // Convert image into readable format
        // Always convert to ABGR since the texc lib demands that for resizing etc
        TimeProfiler.start("generateTexture");
        BufferedImage image;
        if (origImage.getType() != BufferedImage.TYPE_4BYTE_ABGR) {
            image = convertImage(origImage, BufferedImage.TYPE_4BYTE_ABGR);
        } else {
            image = origImage;
        }

        // Setup texture format and settings
        ColorModel colorModel = origImage.getColorModel();
        int componentCount = colorModel.getNumComponents();
        TextureImage.Builder textureBuilder = TextureImage.newBuilder();

        if (texProfile != null) {

            // Generate an image for each format specified in the profile
            for (PlatformProfile platformProfile : texProfile.getPlatformsList()) {
                for (int i = 0; i < platformProfile.getFormatsList().size(); ++i) {
                    TextureFormat textureFormat = platformProfile.getFormats(i).getFormat();
                    String textureCompressor = platformProfile.getFormats(i).getCompressor();
                    String textureCompressorPreset = platformProfile.getFormats(i).getCompressorPreset();
                    TextureImage.CompressionType compressionType;

                    // We pick a "new" format based on the input image component count and a "target" format.
                    // For example we would rather have a texture format with 3 channels if the input
                    // image has 3 channels, even if the texture profile specified a format with 4 channels.
                    textureFormat = pickOptimalFormat(componentCount, textureFormat);

                    // Legacy options
                    boolean hasCompressionLevel = platformProfile.getFormats(i).hasCompressionLevel();
                    boolean hasCompressionType = platformProfile.getFormats(i).hasCompressionType();

                    if (hasCompressionType) {
                        compressionType = textureFormatToSupportedCompressionTypeOrDefault(textureFormat, platformProfile.getFormats(i).getCompressionType());
                    } else {
                        compressionType = textureCompressorToCompressionType(textureCompressor);
                    }

                    if (hasCompressionLevel) {
                        TextureFormatAlternative.CompressionLevel compressionLevel = platformProfile.getFormats(i).getCompressionLevel();
                        textureCompressor = compressionTypeToTextureCompressor(compressionType);
                        textureCompressorPreset = compressionLevelToTextureCompressorPreset(compressionType, compressionLevel);
                    }

                    try {
                        TextureImage.Image.Builder imageBuilder = generateFromColorAndFormat(null, image, colorModel, textureFormat, textureCompressor, textureCompressorPreset, platformProfile.getMipmaps(), platformProfile.getMaxTextureSize(), platformProfile.getPremultiplyAlpha(), flipAxis);
                        imageBuilder.setCompressionType(compressionType);
                        textureBuilder.addAlternatives(imageBuilder);
                    } catch (TextureGeneratorException e) {
                        throw e;
                    }

                }
            }

            textureBuilder.setCount(1);
            if (textureBuilder.getAlternativesCount() == 0) {
                texProfile = null;
            }
        }

        // If no texture profile was supplied, or no matching format was found, or no compression has been requested
        if (texProfile == null) {

            // Guess texture format based on number color components of input image
            TextureFormat textureFormat = pickOptimalFormat(componentCount, TextureFormat.TEXTURE_FORMAT_RGBA);
            TextureImage.Image.Builder imageBuilder = generateFromColorAndFormat(null, image, colorModel, textureFormat, "Default", "DEFAULT", true, 0, true, flipAxis);
            imageBuilder.setCompressionType(TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT);
            textureBuilder.addAlternatives(imageBuilder);
            textureBuilder.setCount(1);
        }

        textureBuilder.setType(Type.TYPE_2D);
        TextureImage textureImage = textureBuilder.build();
        TimeProfiler.stop();
        return textureImage;

    }

    public static void main(String[] args) throws IOException, TextureGeneratorException {
        System.setProperty("java.awt.headless", "true");

        // Install default texture compressors
        TextureCompression.registerCompressor(new TextureCompressorBasisU());

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
             BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {
            TextureImage texture = generate(is);
            texture.writeTo(os);
        }
    }
}
