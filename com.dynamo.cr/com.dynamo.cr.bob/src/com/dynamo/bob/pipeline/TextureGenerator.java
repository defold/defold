package com.dynamo.bob.pipeline;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.ImageIO;

import com.dynamo.bob.TexcLibrary;
import com.dynamo.bob.TexcLibrary.ColorSpace;
import com.dynamo.bob.TexcLibrary.DitherType;
import com.dynamo.bob.TexcLibrary.PixelFormat;
import com.dynamo.bob.TexcLibrary.CompressionLevel;
import com.dynamo.bob.TexcLibrary.CompressionType;
import com.dynamo.bob.TexcLibrary.FlipAxis;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.Platform;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.google.protobuf.ByteString;
import com.sun.jna.Pointer;


public class TextureGenerator {

    private static HashMap<TextureFormatAlternative.CompressionLevel, Integer> compressionLevelLUT = new HashMap<TextureFormatAlternative.CompressionLevel, Integer>();
    static {
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.FAST, CompressionLevel.CL_FAST);
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.NORMAL, CompressionLevel.CL_NORMAL);
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.HIGH, CompressionLevel.CL_HIGH);
        compressionLevelLUT.put(TextureFormatAlternative.CompressionLevel.BEST, CompressionLevel.CL_BEST);
    }

    private static HashMap<TextureImage.CompressionType, Integer> compressionTypeLUT = new HashMap<TextureImage.CompressionType, Integer>();
    static {
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT, CompressionType.CT_DEFAULT);
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_WEBP, CompressionType.CT_WEBP);
        compressionTypeLUT.put(TextureImage.CompressionType.COMPRESSION_TYPE_WEBP_LOSSY, CompressionType.CT_WEBP_LOSSY);
    }

    private static HashMap<TextureFormat, Integer> pixelFormatLUT = new HashMap<TextureFormat, Integer>();
    static {
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_LUMINANCE, PixelFormat.L8);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB, PixelFormat.R8G8B8);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA, PixelFormat.R8G8B8A8);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1, PixelFormat.RGB_PVRTC_2BPPV1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1, PixelFormat.RGB_PVRTC_4BPPV1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1, PixelFormat.RGBA_PVRTC_2BPPV1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1, PixelFormat.RGBA_PVRTC_4BPPV1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_ETC1, PixelFormat.RGB_ETC1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_16BPP, PixelFormat.R5G6B5);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_16BPP, PixelFormat.R4G4B4A4);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA, PixelFormat.L8A8);

        /*
        JIRA issue: DEF-994
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGB_DXT1, PixelFormat.RGB_DXT1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_DXT1, PixelFormat.RGBA_DXT1);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_DXT3, PixelFormat.RGBA_DXT3);
        pixelFormatLUT.put(TextureFormat.TEXTURE_FORMAT_RGBA_DXT5, PixelFormat.RGBA_DXT5);
        */
    }

    // Two generate() methods to generate TextureImages without any texture profile.
    public static TextureImage generate(BufferedImage origImage) throws TextureGeneratorException, IOException {
        return generate(origImage, null);
     }

    public static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        return generate(inputStream, null);
     }

    public static TextureImage generate(InputStream inputStream, TextureProfile texProfile) throws TextureGeneratorException, IOException {
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        return generate(origImage, texProfile);
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
                if (componentCount == 2)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE_ALPHA;
                return TextureFormat.TEXTURE_FORMAT_RGB;
            }

            case TEXTURE_FORMAT_RGBA: {
                if (componentCount == 1)
                    return TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                if (componentCount == 2)
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

            // DXT
            /*
            JIRA issue: DEF-994

            case TEXTURE_FORMAT_RGBA_DXT1: {
                if (componentCount < 4)
                    return TextureFormat.TEXTURE_FORMAT_RGB_DXT1;

                return targetFormat;
            }
            */
        }

        return targetFormat;
    }

    private static TextureImage.Image generateFromColorAndFormat(BufferedImage image, ColorModel colorModel, TextureFormat textureFormat, TextureFormatAlternative.CompressionLevel compressionLevel, TextureImage.CompressionType compressionType, boolean generateMipMaps, int maxTextureSize, boolean compress) throws TextureGeneratorException, IOException {

        int width = image.getWidth();
        int height = image.getHeight();
        int componentCount = colorModel.getNumComponents();
        Integer pixelFormat = PixelFormat.R8G8B8A8;
        int texcCompressionLevel;
        int texcCompressionType;

        int dataSize = width * height * 4;
        ByteBuffer buffer = ByteBuffer.allocateDirect(dataSize);

        // On Linux we run out of memory while trying to load a 4K texture.
        // We split the pixel read out into blocks with 512 scan lines per block.
        for (int y = 0; y < height; y+=512) {

            int count = Math.min(height - y, 512);

            int[] rasterData = new int[count*width*4];
            image.getRaster().getPixels(0, y, width, count, rasterData);
            for (int i = 0; i < rasterData.length; ++i) {
                buffer.put((byte) (rasterData[i] & 0xff));
            }
        }

        buffer.flip();
        Pointer texture = TexcLibrary.TEXC_Create(width, height, PixelFormat.R8G8B8A8, ColorSpace.SRGB, buffer);

        // convert from protobuf specified compressionlevel to texc int
        texcCompressionLevel = compressionLevelLUT.get(compressionLevel);

        // convert from protobuf specified compressionType to texc int
        texcCompressionType = compressionTypeLUT.get(compressionType);

        if (!compress) {
            texcCompressionLevel = CompressionLevel.CL_FAST;
            texcCompressionType = CompressionType.CT_DEFAULT;

            // If pvrtc or etc1, set these as rgba instead. Since these formats will take some time to compress even
            // with "fast" setting and we don't want to increase the build time more than we have to.
            if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGB_ETC1) {
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
            } else if (textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 || textureFormat == TextureFormat.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1) {
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
            }
        }

        // pick a pixel format (for texc) based on the texture format
        pixelFormat = pixelFormatLUT.get(textureFormat);
        if (pixelFormat == null) {
            throw new TextureGeneratorException("Invalid texture format.");
        }

        try {

            int newWidth  = image.getWidth();
            int newHeight = image.getHeight();

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
                logger.log(Level.WARNING, "PVR compressed texture is not square and will be resized.");

                newWidth = Math.max(newWidth, newHeight);
                newHeight = newWidth;
            }

            // Premultiply before scale so filtering cannot introduce colour artefacts.
            if (!ColorModel.getRGBdefault().isAlphaPremultiplied()) {
                if (!TexcLibrary.TEXC_PreMultiplyAlpha(texture)) {
                    throw new TextureGeneratorException("could not premultiply alpha");
                }
            }

            if (width != newWidth || height != newHeight) {
                if (!TexcLibrary.TEXC_Resize(texture, newWidth, newHeight)) {
                    throw new TextureGeneratorException("could not resize texture to POT");
                }
            }

            if (generateMipMaps) {
                if (!TexcLibrary.TEXC_GenMipMaps(texture)) {
                    throw new TextureGeneratorException("could not generate mip-maps");
                }
            }

            if (!TexcLibrary.TEXC_Flip(texture, FlipAxis.FLIP_AXIS_Y)) {
                throw new TextureGeneratorException("could not flip");
            }

            if (!TexcLibrary.TEXC_Transcode(texture, pixelFormat, ColorSpace.SRGB, texcCompressionLevel, texcCompressionType, DitherType.DT_DEFAULT)) {
                throw new TextureGeneratorException("could not transcode");
            }

            int bufferSize = TexcLibrary.TEXC_GetTotalDataSize(texture);
            buffer = ByteBuffer.allocateDirect(bufferSize);
            dataSize = TexcLibrary.TEXC_GetData(texture, buffer, bufferSize);
            buffer.limit(dataSize);

            TextureImage.Image.Builder raw = TextureImage.Image.newBuilder().setWidth(newWidth).setHeight(newHeight)
                    .setOriginalWidth(width).setOriginalHeight(height).setFormat(textureFormat);

            int w = newWidth;
            int h = newHeight;
            int offset = 0;
            int mipMap = 0;
            while (w != 0 || h != 0) {
                w = Math.max(w, 1);
                h = Math.max(h, 1);
                raw.addMipMapOffset(offset);
                int size = TexcLibrary.TEXC_GetDataSizeUncompressed(texture, mipMap);
                raw.addMipMapSize(size);
                int size_compressed = TexcLibrary.TEXC_GetDataSizeCompressed(texture, mipMap);
                if(size_compressed != 0) {
                    size = size_compressed;
                }
                raw.addMipMapSizeCompressed(size_compressed);
                offset += size;
                w >>= 1;
                h >>= 1;
                mipMap += 1;

                if (!generateMipMaps) // Run section only once for non-mipmaps
                    break;
            }

            raw.setData(ByteString.copyFrom(buffer));
            raw.setFormat(textureFormat);
            raw.setCompressionType(compressionType);
            raw.setCompressionFlags(TexcLibrary.TEXC_GetCompressionFlags(texture));

            return raw.build();

        } finally {
            TexcLibrary.TEXC_Destroy(texture);
        }

    }

    public static TextureImage generate(BufferedImage origImage, TextureProfile texProfile) throws TextureGeneratorException, IOException {
        // Convert image into readable format
        // Always convert to ABGR since the texc lib demands that for resizing etc
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
                        TextureImage.Image raw = generateFromColorAndFormat(image, colorModel, textureFormat, compressionLevel, compressionType, platformProfile.getMipmaps(), platformProfile.getMaxTextureSize(), texProfile.getCompress() );
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

            TextureImage.Image raw = generateFromColorAndFormat(image, colorModel, textureFormat, TextureFormatAlternative.CompressionLevel.NORMAL, TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT, true, 0, false);
            textureBuilder.addAlternatives(raw);
            textureBuilder.setCount(1);

        }

        textureBuilder.setType(Type.TYPE_2D);
        return textureBuilder.build();

    }

    public static void main(String[] args) throws IOException, TextureGeneratorException {
        System.setProperty("java.awt.headless", "true");
        try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
             BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {
            TextureImage texture = generate(is);
            texture.writeTo(os);
        }
    }
}
