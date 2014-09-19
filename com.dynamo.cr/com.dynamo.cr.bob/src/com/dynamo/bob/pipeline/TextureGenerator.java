package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

import com.dynamo.bob.pipeline.Texc.PixelFormat;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;


public class TextureGenerator {

    static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        return generate(origImage);
    }

    static TextureImage generate(BufferedImage origImage) throws TextureGeneratorException {
        BufferedImage image = origImage;

        int width = image.getWidth();
        int height = image.getHeight();

        PixelFormat pixelFormat = selectPixelFormat(origImage);

        int widthPOT = TextureUtil.closestPOT(origImage.getWidth());
        int heightPOT = TextureUtil.closestPOT(origImage.getHeight());
        if (width != widthPOT || height != heightPOT) {
            image = Texc.resize(image, widthPOT, heightPOT);
        }

        if (!ColorModel.getRGBdefault().isAlphaPremultiplied() && PixelFormat.R8G8B8A8 == pixelFormat) {
            image = Texc.premultiplyAlpha(image);
        }

        BufferedImage[] mipmaps = Texc.generateMipmaps(image);

        return Texc.compileTexture(origImage, mipmaps, pixelFormat);
    }

    private static PixelFormat selectPixelFormat(BufferedImage image) throws TextureGeneratorException {
        ColorModel colorModel = image.getColorModel();
        int componentCount = colorModel.getNumComponents();
        PixelFormat pixelFormat = PixelFormat.R8G8B8A8;
        switch (componentCount) {
        case 1:
            pixelFormat = PixelFormat.L8;
            break;
        case 3:
            pixelFormat = PixelFormat.R8G8B8;
            break;
        case 4:
            pixelFormat = PixelFormat.R8G8B8A8;
            break;
        default:
            throw new TextureGeneratorException(String.format("Cannot select pixel format for %d component texture", componentCount));
        }
        return pixelFormat;
    }
}
