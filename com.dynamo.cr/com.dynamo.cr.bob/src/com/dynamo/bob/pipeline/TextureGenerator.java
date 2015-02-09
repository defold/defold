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

import javax.imageio.ImageIO;

import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TexcLibrary;
import com.dynamo.bob.TexcLibrary.ColorSpace;
import com.dynamo.bob.TexcLibrary.PixelFormat;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.google.protobuf.ByteString;
import com.sun.jna.Pointer;


public class TextureGenerator {


    // Two generate() methods to generate TextureImages without any texture profile.
    static TextureImage generate(BufferedImage origImage) throws TextureGeneratorException, IOException {
        return generate(origImage, null);
     }

    static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        return generate(inputStream, null);
     }

    static TextureImage generate(InputStream inputStream, TextureProfile texProfile) throws TextureGeneratorException, IOException {
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

    private static TextureImage.Image generateFromColorAndFormat(BufferedImage image, ColorModel colorModel, TextureFormat textureFormat, boolean generateMipMaps, int maxTextureSize) throws TextureGeneratorException, IOException {

        int width = image.getWidth();
        int height = image.getHeight();
        int componentCount = colorModel.getNumComponents();
        int pixelFormat = PixelFormat.R8G8B8A8;

        int dataSize = width * height * 4;
        ByteBuffer buffer = ByteBuffer.allocateDirect(dataSize);
        int[] rasterData = new int[dataSize];
        image.getRaster().getPixels(0, 0, width, height, rasterData);
        for (int i = 0; i < rasterData.length; ++i) {
            buffer.put((byte) (rasterData[i] & 0xff));
        }
        buffer.flip();
        Pointer texture = TexcLibrary.TEXC_Create(width, height, PixelFormat.R8G8B8A8, ColorSpace.SRGB, buffer);

        // pick a pixel format (for texc) based on the texture format
        switch (textureFormat)
        {
        case TEXTURE_FORMAT_LUMINANCE:
            pixelFormat = PixelFormat.L8;
            break;
        case TEXTURE_FORMAT_RGB:
            pixelFormat = PixelFormat.R8G8B8;
            break;
        case TEXTURE_FORMAT_RGBA:
            pixelFormat = PixelFormat.R8G8B8A8;
            break;

        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            pixelFormat = PixelFormat.RGB_PVRTC_2BPPV1;
            break;
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            pixelFormat = PixelFormat.RGB_PVRTC_4BPPV1;
            break;
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            pixelFormat = PixelFormat.RGBA_PVRTC_2BPPV1;
            break;
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            pixelFormat = PixelFormat.RGBA_PVRTC_4BPPV1;
            break;

        case TEXTURE_FORMAT_RGB_ETC1:
            pixelFormat = PixelFormat.RGB_ETC1;
            break;

        case TEXTURE_FORMAT_RGB_DXT1:
            pixelFormat = PixelFormat.RGB_DXT1;
            break;
        case TEXTURE_FORMAT_RGBA_DXT1:
            pixelFormat = PixelFormat.RGBA_DXT1;
            break;
        case TEXTURE_FORMAT_RGBA_DXT3:
            pixelFormat = PixelFormat.RGBA_DXT3;
            break;
        case TEXTURE_FORMAT_RGBA_DXT5:
            pixelFormat = PixelFormat.RGBA_DXT5;
            break;

        default:
            throw new TextureGeneratorException( "Invalid texture format." );
        }

        try {

            int newWidth  = image.getWidth();
            int newHeight = image.getHeight();

            newWidth = TextureUtil.closestPOT(newWidth);
            newHeight = TextureUtil.closestPOT(newHeight);

            if (maxTextureSize > 0) {
                while (newWidth > maxTextureSize) {
                    newWidth = newWidth / 2;
                }

                while (newHeight > maxTextureSize) {
                    newHeight = newHeight / 2;
                }

                assert( newWidth <= maxTextureSize && newHeight <= maxTextureSize );
            }

            if (width != newWidth || height != newHeight) {
                if (!TexcLibrary.TEXC_Resize(texture, newWidth, newHeight)) {
                    throw new TextureGeneratorException("could not resize texture to POT");
                }
            }
            if (!ColorModel.getRGBdefault().isAlphaPremultiplied()) {
                if (!TexcLibrary.TEXC_PreMultiplyAlpha(texture)) {
                    throw new TextureGeneratorException("could not premultiply alpha");
                }
            }
            if (generateMipMaps)
            {
                if (!TexcLibrary.TEXC_GenMipMaps(texture)) {
                    throw new TextureGeneratorException("could not generate mip-maps");
                }
            }
            if (!TexcLibrary.TEXC_Transcode(texture, pixelFormat, ColorSpace.SRGB)) {
                throw new TextureGeneratorException("could not transcode");
            }

            int bufferSize = newWidth * newHeight * componentCount;

            if (generateMipMaps)
                bufferSize *= 2; // twice the size for mip maps

            buffer = ByteBuffer.allocateDirect(bufferSize);
            dataSize = TexcLibrary.TEXC_GetData(texture, buffer, bufferSize);
            buffer.limit(dataSize);

            TextureImage.Image.Builder raw = TextureImage.Image.newBuilder().setWidth(newWidth).setHeight(newHeight)
                    .setOriginalWidth(width).setOriginalHeight(height).setFormat(textureFormat);

            int w = newWidth;
            int h = newHeight;
            int offset = 0;
            while (w != 0 || h != 0) {
                w = Math.max(w, 1);
                h = Math.max(h, 1);
                int size = w * h * componentCount;
                raw.addMipMapOffset(offset);
                raw.addMipMapSize(size);
                offset += size;
                w >>= 1;
                h >>= 1;

                if (!generateMipMaps) // Run only once for non-mipmaps
                    break;
            }

            raw.setData(ByteString.copyFrom(buffer));
            raw.setFormat(textureFormat);

            return raw.build();

        } finally {
            TexcLibrary.TEXC_Destroy(texture);
        }

    }

    static TextureImage generate(BufferedImage origImage, TextureProfile texProfile) throws TextureGeneratorException, IOException {
        // Convert image into readable format
        // Always convert to ABGR since the texc lib demands that for resizing etc
        BufferedImage image;
        if (origImage.getType() != BufferedImage.TYPE_4BYTE_ABGR) {
            image = convertImage(origImage, BufferedImage.TYPE_4BYTE_ABGR);
        } else {
            image = origImage;
        }

        // Setup texture format and settings
        TextureFormat textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
        ColorModel colorModel = origImage.getColorModel();
        TextureImage.Builder textureBuilder = TextureImage.newBuilder();

        if (texProfile != null)
        {

            // Generate an image for each format specified in the profile
            int altCount = 0;
            for ( PlatformProfile platformProfile : texProfile.getPlatformsList())
            {
                for ( int i = 0; i < platformProfile.getFormatsList().size(); ++i)
                {
                    textureFormat = platformProfile.getFormats(i).getFormat();

                    try
                    {
                        TextureImage.Image raw = generateFromColorAndFormat(image, colorModel, textureFormat, platformProfile.getMipmaps(), platformProfile.getMaxTextureSize() );
                        textureBuilder.addAlternatives(raw);
                        altCount += 1;
                    } catch (TextureGeneratorException e) {
                        throw e;
                    }

                }
            }

            if (altCount > 0)
            {
                textureBuilder.setCount(altCount);
            } else {
                // if no platform alternatives or texture formats was found,
                // we need to fall back to default texture formats below.
                texProfile = null;
            }
        }

        // If no texture profile was supplied, or no matching format was found
        if (texProfile == null)
        {

            // Guess texture format based on number color components of input image
            int componentCount = colorModel.getNumComponents();
            switch (componentCount) {
            case 1:
                textureFormat = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
                break;
            case 3:
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
                break;
            case 4:
                textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
                break;
            }

            TextureImage.Image raw = generateFromColorAndFormat(image, colorModel, textureFormat, true, 0);
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
