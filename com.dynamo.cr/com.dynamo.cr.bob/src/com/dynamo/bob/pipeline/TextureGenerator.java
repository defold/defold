package com.dynamo.bob.pipeline;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

import javax.imageio.ImageIO;

import com.dynamo.bob.TexcLibrary;
import com.dynamo.bob.TexcLibrary.ColorSpace;
import com.dynamo.bob.TexcLibrary.PixelFormat;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.google.protobuf.ByteString;
import com.sun.jna.Pointer;

public class TextureGenerator {

    static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        return generate(origImage);
     }

    private static BufferedImage convertImage(BufferedImage origImage, int type) {
        BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), type);
        Graphics2D g2d = image.createGraphics();
        g2d.drawImage(origImage, 0, 0, null);
        g2d.dispose();
        return image;
    }

    static TextureImage generate(BufferedImage origImage) throws TextureGeneratorException, IOException {
        // Convert image into readable format
        // Always convert to ABGR since the texc lib demands that for resizing etc
        BufferedImage image;
        if (origImage.getType() != BufferedImage.TYPE_4BYTE_ABGR) {
            image = convertImage(origImage, BufferedImage.TYPE_4BYTE_ABGR);
        } else {
            image = origImage;
        }

        int width = image.getWidth();
        int height = image.getHeight();
        ColorModel colorModel = origImage.getColorModel();
        int componentCount = colorModel.getNumComponents();
        int pixelFormat = PixelFormat.R8G8B8A8;
        TextureFormat textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
        switch (componentCount) {
        case 1:
            pixelFormat = PixelFormat.L8;
            textureFormat = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
            break;
        case 3:
            pixelFormat = PixelFormat.R8G8B8;
            textureFormat = TextureFormat.TEXTURE_FORMAT_RGB;
            break;
        case 4:
            pixelFormat = PixelFormat.R8G8B8A8;
            textureFormat = TextureFormat.TEXTURE_FORMAT_RGBA;
            break;
        }
        int dataSize = width * height * 4;
        ByteBuffer buffer = ByteBuffer.allocateDirect(dataSize);
        int[] rasterData = new int[dataSize];
        image.getRaster().getPixels(0, 0, width, height, rasterData);
        for (int i = 0; i < rasterData.length; ++i) {
            buffer.put((byte) (rasterData[i] & 0xff));
        }
        buffer.flip();
        Pointer texture = TexcLibrary.TEXC_Create(width, height, PixelFormat.R8G8B8A8, ColorSpace.SRGB, buffer);
        try {
            int widthPOT = TextureUtil.closestPOT(origImage.getWidth());
            int heightPOT = TextureUtil.closestPOT(origImage.getHeight());
            if (width != widthPOT || height != heightPOT) {
                if (!TexcLibrary.TEXC_Resize(texture, widthPOT, heightPOT)) {
                    throw new TextureGeneratorException("could not resize texture to POT");
                }
            }
            if (!ColorModel.getRGBdefault().isAlphaPremultiplied()) {
                if (!TexcLibrary.TEXC_PreMultiplyAlpha(texture)) {
                    throw new TextureGeneratorException("could not premultiply alpha");
                }
            }
            if (!TexcLibrary.TEXC_GenMipMaps(texture)) {
                throw new TextureGeneratorException("could not generate mip-maps");
            }
            if (!TexcLibrary.TEXC_Transcode(texture, pixelFormat, ColorSpace.SRGB)) {
                throw new TextureGeneratorException("could not transcode");
            }
            // twice the size for mip maps
            int bufferSize = widthPOT * heightPOT * componentCount * 2;
            buffer = ByteBuffer.allocateDirect(bufferSize);
            dataSize = TexcLibrary.TEXC_GetData(texture, buffer, bufferSize);
            buffer.limit(dataSize);

            TextureImage.Builder textureBuilder = TextureImage.newBuilder();

            TextureImage.Image.Builder raw = TextureImage.Image.newBuilder().setWidth(widthPOT).setHeight(heightPOT)
                    .setOriginalWidth(width).setOriginalHeight(height).setFormat(textureFormat);

            int w = widthPOT;
            int h = heightPOT;
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
            }

            raw.setData(ByteString.copyFrom(buffer));
            raw.setFormat(textureFormat);
            textureBuilder.addAlternatives(raw);
            return textureBuilder.build();

        } finally {
            TexcLibrary.TEXC_Destroy(texture);
        }

    }


}
