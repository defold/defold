package com.dynamo.bob.pipeline;

import java.awt.geom.AffineTransform;
import java.awt.image.AffineTransformOp;
import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.util.ArrayList;

import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.google.protobuf.ByteString;

/**
 * Replacement for the Texc native code, removing dependency the PIL.
 *
 * @author peterhodges
 *
 */
public class Texc {

    public enum PixelFormat {
        L8,
        R8G8B8,
        R8G8B8A8
    }

    private static int handleLinuxPngTypeFailing(BufferedImage image) {
        // On linux, PNG type detection fails.
        // http://stackoverflow.com/questions/5836128/how-do-i-make-javas-imagebuffer-to-read-a-png-file-correctly
        // We work around this bug with a hack.
        int type = image.getType();
        if (0 == type) {
            type = 5;
        }
        return type;
    }

    public static BufferedImage resize(BufferedImage image, int width, int height) {
        BufferedImage result = new BufferedImage(width, height, handleLinuxPngTypeFailing(image));

        AffineTransform transform = new AffineTransform();
        transform.scale((double)width / (double)image.getWidth(), (double)height / (double)image.getHeight());
        AffineTransformOp operation = new AffineTransformOp(transform, AffineTransformOp.TYPE_BILINEAR);

        return operation.filter(image, result);
    }

    public static BufferedImage premultiplyAlpha(BufferedImage image) {
        int numPixels = image.getWidth() * image.getHeight();
        int[] rgb = new int[numPixels];
        rgb = image.getRGB(0, 0, image.getWidth(), image.getHeight(), rgb, 0, image.getWidth());
        for (int i=0; i<numPixels; ++i) {
            int red = rgb[i] & 0xff;
            int green = (rgb[i] >> 8) & 0xff;
            int blue = (rgb[i] >> 16) & 0xff;
            int alpha = (rgb[i] >> 24) & 0xff;

            red = (red * alpha) / 255;
            green = (green * alpha) / 255;
            blue = (blue * alpha) / 255;

            rgb[i] = red | (green << 8) | (blue << 16) | (alpha << 24);
        }

        BufferedImage result = new BufferedImage(image.getWidth(), image.getHeight(), handleLinuxPngTypeFailing(image));
        result.setRGB(0, 0, image.getWidth(), image.getHeight(), rgb, 0, image.getWidth());
        return result;
    }

    public static BufferedImage[] generateMipmaps(BufferedImage top) throws TextureGeneratorException {
        if (top.getWidth() != TextureUtil.closestPOT(top.getWidth()) ||
                top.getHeight() != TextureUtil.closestPOT(top.getHeight())) {
            throw new TextureGeneratorException(String.format("Cannot generate mipmaps: illegal dimensions %dx%d", top.getWidth(), top.getHeight()));
        }

        ArrayList<BufferedImage> mips = new ArrayList<BufferedImage>();
        mips.add(top);

        int w = top.getWidth();
        int h = top.getHeight();
        while (true) {
            w >>= 1;
            h >>= 1;

            if (0 == w && 0 == h) {
                break;
            }

            BufferedImage current = mips.get(mips.size()-1);
            mips.add(resize(current, Math.max(w, 1), Math.max(h,  1)));
        }

        BufferedImage[] result = new BufferedImage[mips.size()];
        return mips.toArray(result);
    }

    public static TextureImage compileTexture(BufferedImage origImage, BufferedImage[] mips, PixelFormat pixelFormat) throws TextureGeneratorException {
        TextureFormat textureFormat = getTextureFormat(pixelFormat);

        if (PixelFormat.L8 == pixelFormat) {
            for (int i=0; i<mips.length; ++i) {
                mips[i] = convertToLuminance(mips[i]);
            }
        }

        BufferedImage first = mips[0];
        TextureImage.Builder textureBuilder = TextureImage.newBuilder();

        TextureImage.Image.Builder raw = TextureImage.Image.newBuilder()
                .setWidth(first.getWidth()).setHeight(first.getHeight())
                .setOriginalWidth(origImage.getWidth()).setOriginalHeight(origImage.getHeight())
                .setFormat(textureFormat);

        int bufferSize = first.getWidth() * first.getHeight() * getComponentCount(pixelFormat) * 2;
        ByteBuffer buffer = ByteBuffer.allocateDirect(bufferSize);

        int dataSize = 0;
        int lastWidth = first.getWidth();
        int lastHeight = first.getHeight();
        for (int i=0; i < mips.length; ++i) {
            BufferedImage current = mips[i];

            if (current.getWidth() != TextureUtil.closestPOT(current.getWidth()) ||
                    current.getHeight() != TextureUtil.closestPOT(current.getHeight())) {
                throw new TextureGeneratorException(String.format("Cannot compile mipmaps: illegal dimensions %dx%d", current.getWidth(), current.getHeight()));
            }
            if (current.getWidth() > lastWidth || current.getHeight() > lastHeight) {
                throw new TextureGeneratorException("Cannot compile mipmaps: mipmap levels are not ordered in descending size");
            }

            int offset = dataSize;
            int size = writeToBuffer(buffer, offset, current, pixelFormat);
            raw.addMipMapOffset(offset);
            raw.addMipMapSize(size);
            dataSize += size;

            lastWidth = current.getWidth();
            lastHeight = current.getHeight();
        }

        buffer.limit(dataSize);
        buffer.flip();

        raw.setData(ByteString.copyFrom(buffer));
        raw.setFormat(textureFormat);
        textureBuilder.addAlternatives(raw);
        textureBuilder.setType(Type.TYPE_2D);
        textureBuilder.setCount(1);

        return textureBuilder.build();
    }

    private static int getComponentCount(PixelFormat pixelFormat) throws TextureGeneratorException {
        int componentCount = 0;
        switch (pixelFormat) {
        case L8:
            componentCount = 1;
            break;
        case R8G8B8:
            componentCount = 3;
            break;
        case R8G8B8A8:
            componentCount = 4;
            break;
        default:
            throw new TextureGeneratorException(String.format("Unsupported pixel format: %s", pixelFormat.name()));
        }
        return componentCount;
    }

    private static TextureFormat getTextureFormat(PixelFormat pixelFormat) throws TextureGeneratorException {
        TextureFormat textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
        switch (pixelFormat) {
        case L8:
            textureFormat = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
            break;
        case R8G8B8:
            textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
            break;
        case R8G8B8A8:
            textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
            break;
        default:
           throw new TextureGeneratorException(String.format("Unsupported pixel format: %d", pixelFormat));
        }
        return textureFormat;
    }

    private static int writeToBuffer(ByteBuffer buffer, int offset, BufferedImage image, PixelFormat pixelFormat) throws TextureGeneratorException {
        int numPixels = image.getWidth() * image.getHeight();
        int[] rgb = new int[numPixels];
        rgb = image.getRGB(0, 0, image.getWidth(), image.getHeight(), rgb, 0, image.getWidth());

        switch (pixelFormat) {
        case L8:
            for (int i=0; i<numPixels; ++i) {
                int red = rgb[i] & 0xff;
                int green = (rgb[i] >> 8) & 0xff;
                int blue = (rgb[i] >> 16) & 0xff;
                byte y = (byte)Math.max(Math.max(red, green), blue);
                buffer.put(y);
            }
            break;
        case R8G8B8:
            for (int i=0; i<numPixels; ++i) {
                byte red = (byte)(rgb[i] & 0xff);
                byte green = (byte)((rgb[i] >> 8) & 0xff);
                byte blue = (byte)((rgb[i] >> 16) & 0xff);
                buffer.put(blue);
                buffer.put(green);
                buffer.put(red);
            }
            break;
        case R8G8B8A8:
            for (int i=0; i<numPixels; ++i) {
                byte red = (byte)(rgb[i] & 0xff);
                byte green = (byte)((rgb[i] >> 8) & 0xff);
                byte blue = (byte)((rgb[i] >> 16) & 0xff);
                byte alpha = (byte)((rgb[i] >> 24) & 0xff);
                buffer.put(blue);
                buffer.put(green);
                buffer.put(red);
                buffer.put(alpha);
            }
            break;
        }

        return numPixels * getComponentCount(pixelFormat);
    }

    // relative luminance: https://en.wikipedia.org/wiki/Relative_luminance
    public static BufferedImage convertToLuminance(BufferedImage image) {
        BufferedImage result = null;

        if (1 == image.getColorModel().getNumComponents()) {
            result = image;
        } else {
            result = new BufferedImage(image.getWidth(), image.getHeight(), BufferedImage.TYPE_BYTE_GRAY);

            int scanSize = image.getWidth() * image.getHeight();
            int[] rgb = new int[scanSize];
            rgb = image.getRGB(0, 0, image.getWidth(), image.getHeight(), rgb, 0, image.getWidth());
            for (int i=0; i<scanSize; ++i) {
                int pixel = rgb[i];

                float red = (pixel&0xff) * 0.2126f;
                float green = ((pixel>>8)&0xff) * 0.7152f;
                float blue = ((pixel>>16)&0xff) * 0.0722f;
                float luminance = Math.min(255.0f, red + green + blue);

                rgb[i] = Math.round(luminance) | (Math.round(luminance) << 8) | (Math.round(luminance) << 16);
            }

            result.setRGB(0, 0, image.getWidth(), image.getHeight(), rgb, 0, 0);
        }

        return result;
    }

}
