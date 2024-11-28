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
import com.dynamo.bob.pipeline.TexcLibraryJni;
import com.dynamo.bob.pipeline.Texc.ColorSpace;
import com.dynamo.bob.pipeline.Texc.PixelFormat;
import com.dynamo.bob.pipeline.Texc.CompressionLevel;
import com.dynamo.bob.pipeline.Texc.CompressionType;
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
import com.sun.jna.Pointer;


public class TextureGenerator {

    // specify what is maximum of threads TextureGenerator may use
    public static int maxThreads = Project.getDefaultMaxCpuThreads();

    private static HashMap<TextureFormatAlternative.CompressionLevel, Integer> compressionLevelLUT = new HashMap<TextureFormatAlternative.CompressionLevel, Integer>();
    static {
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.FAST, Texc.CompressionLevel.CL_FAST.getValue());
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.NORMAL, Texc.CompressionLevel.CL_NORMAL.getValue());
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.HIGH, Texc.CompressionLevel.CL_HIGH.getValue());
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.BEST, Texc.CompressionLevel.CL_BEST.getValue());
    }

    private static HashMap<TextureImage.CompressionType, Integer> compressionTypeLUT = new HashMap<TextureImage.CompressionType, Integer>();
    static {
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT, Texc.CompressionType.CT_DEFAULT.getValue());
        // For backwards compatibility, we automatically convert the WEBP to either DEFAULT, or UASTC
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_WEBP, Texc.CompressionType.CT_DEFAULT.getValue());
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_WEBP_LOSSY, Texc.CompressionType.CT_BASIS_UASTC.getValue());

        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC, Texc.CompressionType.CT_BASIS_UASTC.getValue());
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_ETC1S, Texc.CompressionType.CT_BASIS_ETC1S.getValue());
    }

    private static HashMap<TextureFormat, Integer> pixelFormatLUT = new HashMap<TextureFormat, Integer>();
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

    private static TextureImage.Image generateFromColorAndFormat(String name,
                                                                BufferedImage image,
                                                                ColorModel colorModel,
                                                                TextureFormat textureFormat,
                                                                TextureFormatAlternative.CompressionLevel compressionLevel,
                                                                TextureImage.CompressionType compressionType,
                                                                boolean generateMipMaps,
                                                                int maxTextureSize,
                                                                boolean compress,
                                                                boolean premulAlpha,
                                                                EnumSet<Texc.FlipAxis> flipAxis) throws TextureGeneratorException {

        int width = image.getWidth();
        int height = image.getHeight();
        int componentCount = colorModel.getNumComponents();
        Integer pixelFormat;
        int texcCompressionLevel;
        int texcCompressionType;

        int dataSize = width * height * 4;

        // convert from protobuf specified compressionlevel to texc int
        texcCompressionLevel = compressionLevelLUT.get(compressionLevel);

        // convert compression type from WebP to something else
        if (compressionType == TextureImage.CompressionType.COMPRESSION_TYPE_WEBP)
            compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;
        else
        if (compressionType == TextureImage.CompressionType.COMPRESSION_TYPE_WEBP_LOSSY)
            compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;

        // convert from protobuf specified compressionType to texc int
        texcCompressionType = compressionTypeLUT.get(compressionType);

        if (!compress) {
            texcCompressionLevel = Texc.CompressionLevel.CL_FAST.getValue();
            texcCompressionType = Texc.CompressionType.CT_DEFAULT.getValue();
            compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT;

            // If pvrtc or etc1, set these as rgba instead. Since these formats will take some time to compress even
            // with "fast" setting and we don't want to increase the build time more than we have to.
            if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_ETC1) {
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
            } else if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1) {
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
            }
        }
        else {
            // Issue 5753: Since we currently don't support pre compressed hardware textures, so we use UASTC instead
            if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_ETC1) {
                compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
            } else if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1) {
                compressionType = TextureImage.CompressionType.COMPRESSION_TYPE_BASIS_UASTC;
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
            }
        }

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

                Logger logger = Logger.getLogger(TextureGenerator.class.getName());
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

            // TODO: We should get the real settings from the texture profile (I think?)
            ITextureCompressor textureCompressor;
            TextureCompressorPreset textureCompressorPreset = null;

            // If we're writing a .basis file, we don't actually store each mip map separately
            // In this case, we pretend that there's only one mip level
            if (texcCompressionType == CompressionType.CT_BASIS_UASTC.getValue() ||
                    texcCompressionType == CompressionType.CT_BASIS_ETC1S.getValue() )
            {
                textureCompressor = TextureCompression.getCompressor("BasisU");

                if (texcCompressionLevel == Texc.CompressionLevel.CL_FAST.getValue()) {
                    textureCompressorPreset = TextureCompression.getPreset("BASISU_FAST");
                } else if (texcCompressionLevel == CompressionLevel.CL_NORMAL.getValue()) {
                    textureCompressorPreset = TextureCompression.getPreset("BASISU_NORMAL");
                } else if (texcCompressionLevel == CompressionLevel.CL_HIGH.getValue()) {
                    textureCompressorPreset = TextureCompression.getPreset("BASISU_HIGH");
                } else if (texcCompressionLevel == CompressionLevel.CL_BEST.getValue()) {
                    textureCompressorPreset = TextureCompression.getPreset("BASISU_BEST");
                }

            } else {
                textureCompressor = TextureCompression.getCompressor("Default");
                textureCompressorPreset = TextureCompression.getPreset("DEFAULT");
            }

            // Generate output images into builder
            TextureImage.Image.Builder raw = TextureImage.Image.newBuilder()
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

                TextureCompressorParams params = new TextureCompressorParams(paramsName, mipMapLevel, mipWidth, mipHeight, 0, componentCount, pixelFormat, pixelFormat, Texc.ColorSpace.CS_SRGB.getValue());
                byte[] compressedData = textureCompressor.compress(textureCompressorPreset, params, uncompressed);

                if (compressedData.length == 0) {
                    throw new TextureGeneratorException("could not encode");
                }

                compressedMipImageDatas.add(compressedData);
                raw.addMipMapOffset(offset);
                raw.addMipMapSize(compressedData.length);
                raw.addMipMapSizeCompressed(compressedData.length);

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

            raw.setData(ByteString.copyFrom(textureData));
            raw.setFormat(textureFormat);
            raw.setCompressionType(compressionType);

            // What are the flags here?
            // raw.setCompressionFlags(TexcLibraryJni.GetCompressionFlags(texture));

            return raw.build();

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
                    TextureImage.CompressionType compressionType = platformProfile.getFormats(i).getCompressionType();
                    TextureFormatAlternative.CompressionLevel compressionLevel = platformProfile.getFormats(i).getCompressionLevel();
                    TextureFormat textureFormat = platformProfile.getFormats(i).getFormat();

                    // We pick a "new" format based on the input image component count and a "target" format.
                    // For example we would rather have a texture format with 3 channels if the input
                    // image has 3 channels, even if the texture profile specified a format with 4 channels.
                    textureFormat = pickOptimalFormat(componentCount, textureFormat);

                    try {
                        TextureImage.Image raw = generateFromColorAndFormat(null, image, colorModel, textureFormat, compressionLevel, compressionType, platformProfile.getMipmaps(), platformProfile.getMaxTextureSize(), compress, platformProfile.getPremultiplyAlpha(), flipAxis);
                        textureBuilder.addAlternatives(raw);
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

        // If no texture profile was supplied, or no matching format was found
        if (texProfile == null) {

            // Guess texture format based on number color components of input image
            TextureFormat textureFormat = pickOptimalFormat(componentCount, TextureFormat.TEXTURE_FORMAT_RGBA);
            TextureImage.Image raw = generateFromColorAndFormat(null, image, colorModel, textureFormat, TextureFormatAlternative.CompressionLevel.NORMAL, TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT, true, 0, false, true, flipAxis);
            textureBuilder.addAlternatives(raw);
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
        TextureCompression.registerCompressor(new TextureCompressorDefault());
        TextureCompression.registerCompressor(new TextureCompressorBasisU());

        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
             BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {
            TextureImage texture = generate(is);
            texture.writeTo(os);
        }
    }
}
