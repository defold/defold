package com.dynamo.bob.pipeline;

import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.Raster;
import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.TextureFormat;
import com.google.protobuf.ByteString;

public class TextureGenerator {

    private static BufferedImage convertImage(BufferedImage origImage, int type) {
        BufferedImage image = new BufferedImage(origImage.getWidth(), origImage.getHeight(), type);
        Graphics2D g2d = image.createGraphics();
        g2d.drawImage(origImage, 0, 0, null);
        g2d.dispose();
        return image;
    }

    private static int totalMipMapSize(int w, int h, int components) {
        int total = 0;
        while (!(w == 0 && h == 0)) {
            w = Math.max(1, w);
            h = Math.max(1, h);

            total += w * h * components;

            w >>= 1;
            h >>= 1;
        }
        return total;
    }

    private static BufferedImage scaleImage(BufferedImage src, int targetWidth, int targetHeight, int type) {
        BufferedImage scaledImage = new BufferedImage(targetWidth, targetHeight, type);
        Graphics2D g = scaledImage.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC);

        g.drawImage(src, 0, 0, targetWidth, targetHeight, null);
        g.dispose();
        return scaledImage;
    }

    protected static BufferedImage scaleImageIncrementally(BufferedImage src,
            int targetWidth, int targetHeight, int type) {
        boolean hasReassignedSrc = false;
        int currentWidth = src.getWidth();
        int currentHeight = src.getHeight();
        int fraction = 4;

        // NOTE: Code from https://github.com/thebuzzmedia/imgscalr/blob/master/src/main/java/org/imgscalr/Scalr.java
        // The method just scales down the image in steps according to fraction above

        do {
            int prevCurrentWidth = currentWidth;
            int prevCurrentHeight = currentHeight;

            /*
             * If the current width is bigger than our target, cut it in half
             * and sample again.
             */
            if (currentWidth > targetWidth) {
                currentWidth -= (currentWidth / fraction);

                /*
                 * If we cut the width too far it means we are on our last
                 * iteration. Just set it to the target width and finish up.
                 */
                if (currentWidth < targetWidth)
                    currentWidth = targetWidth;
            }

            /*
             * If the current height is bigger than our target, cut it in half
             * and sample again.
             */

            if (currentHeight > targetHeight) {
                currentHeight -= (currentHeight / fraction);

                /*
                 * If we cut the height too far it means we are on our last
                 * iteration. Just set it to the target height and finish up.
                 */

                if (currentHeight < targetHeight)
                    currentHeight = targetHeight;
            }
            if (prevCurrentWidth == currentWidth
                    && prevCurrentHeight == currentHeight)
                break;


            // Render the incremental scaled image.
            BufferedImage incrementalImage = scaleImage(src, currentWidth, currentHeight, type);

            if (hasReassignedSrc)
                src.flush();

            /*
             * Now treat our incremental partially scaled image as the src image
             * and cycle through our loop again to do another incremental
             * scaling of it (if necessary).
             */
            src = incrementalImage;

            /*
             * Keep track of us re-assigning the original caller-supplied source
             * image with one of our interim BufferedImages so we know when to
             * explicitly flush the interim "src" on the next cycle through.
             */
            hasReassignedSrc = true;

            // Track how many times we go through this cycle to scale the image.
        } while (currentWidth != targetWidth || currentHeight != targetHeight);

        /*
         * Once the loop has exited, the src image argument is now our scaled
         * result image that we want to return.
         */
        return src;
    }

    static TextureImage generate(InputStream inputStream) throws TextureGeneratorException, IOException {
        BufferedImage origImage = ImageIO.read(inputStream);
        inputStream.close();
        return generate(origImage);
     }

    static TextureImage generate(BufferedImage origImage) throws TextureGeneratorException, IOException {
        BufferedImage image;
        if (origImage.getType() == BufferedImage.TYPE_BYTE_INDEXED) {
            /*
             * If the image is of indexed type convert to:
             * TYPE_BYTE_GRAY if #components == 1
             * TYPE_4BYTE_ABGR is #components != 1 && has-alpha
             * TYPE_4BYTE_ABGR is #components != 1 && !has-alpha
             */

            int type;
            if (origImage.getColorModel().getNumComponents() == 1) {
                type = BufferedImage.TYPE_BYTE_GRAY;
            } else if (origImage.getColorModel().hasAlpha()) {
                type = BufferedImage.TYPE_4BYTE_ABGR;
            } else {
                type = BufferedImage.TYPE_3BYTE_BGR;
            }
            image = convertImage(origImage, type);

        } else {
            if (origImage.getType() == BufferedImage.TYPE_CUSTOM) {
                if (origImage.getColorModel().hasAlpha()) {
                    image = convertImage(origImage, BufferedImage.TYPE_4BYTE_ABGR);
                } else {
                    image = convertImage(origImage, BufferedImage.TYPE_3BYTE_BGR);
                }
            } else {
                // Keep image as is
                image = origImage;
            }
        }

        // Scale image to closest power of two if necessary
        int widthPowerTwo = closestPowerTwo(image.getWidth());
        int heightPowerTwo = closestPowerTwo(image.getHeight());
        if (image.getWidth() != widthPowerTwo || image.getHeight() != heightPowerTwo) {
            image = scaleImage(image, widthPowerTwo, heightPowerTwo, image.getType());
        }

        TextureImage.Builder textureBuilder = TextureImage.newBuilder();

        ColorModel colorModel = image.getColorModel();
        int numComponents = colorModel.getNumComponents();
        boolean hasAlpha = colorModel.hasAlpha();

        TextureFormat format = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
        if (numComponents == 1 && hasAlpha == false) {
            format = TextureFormat.TEXTURE_FORMAT_LUMINANCE;
        } else if (numComponents == 3 && hasAlpha == false) {
            format = TextureFormat.TEXTURE_FORMAT_RGB;
        } else if (numComponents == 4 && hasAlpha == true) {
            format = TextureFormat.TEXTURE_FORMAT_RGBA;
        } else {
            throw new TextureGeneratorException(String.format("unsupported color model: '%s'", colorModel.toString()));
        }

        int width = image.getWidth();
        int height = image.getHeight();

        TextureImage.Image.Builder uncompressed = TextureImage.Image
                .newBuilder()
                .setWidth(width)
                .setHeight(height)
                .setOriginalWidth(origImage.getWidth())
                .setOriginalHeight(origImage.getHeight());

        int w = width;
        int h = height;

        byte[] buffer = new byte[totalMipMapSize(width, height, numComponents)];
        int offset = 0;
        BufferedImage currentImage = origImage;

        while (!(w == 0 && h == 0)) {
            w = Math.max(1, w);
            h = Math.max(1, h);

            BufferedImage tempImage;
            if (width == w && height == h) {
                tempImage = scaleImage(currentImage, w, h, image.getType());
            } else {
                tempImage = scaleImageIncrementally(currentImage, w, h, image.getType());
            }

            Raster raster = tempImage.getData();
            int size = w * h * numComponents;
            int[] rasterData = new int[size];
            raster.getPixels(0, 0, w, h, rasterData);

            for (int i = 0; i < rasterData.length; ++i) {
                buffer[offset + i] = (byte) (rasterData[i] & 0xff);
            }

            uncompressed.addMipMapOffset(offset);
            uncompressed.addMipMapSize(size);
            offset += size;

            w >>= 1;
            h >>= 1;

            currentImage = tempImage;
        }

        uncompressed.setData(ByteString.copyFrom(buffer));
        uncompressed.setFormat(format);
        textureBuilder.addAlternatives(uncompressed);
        TextureImage texture = textureBuilder.build();
        return texture;
    }

    public static int closestPowerTwo(int i) {
        int nextPow2 = 1;
        while (nextPow2 < i) {
            nextPow2 *= 2;
        }
        int prevPower2 = Math.max(1, nextPow2 / 2);

        int nextDistance = nextPow2 - i;
        int prevDistance = i - prevPower2;

        if (nextDistance <= prevDistance)
            return nextPow2;
        else
            return prevPower2;
    }

}
