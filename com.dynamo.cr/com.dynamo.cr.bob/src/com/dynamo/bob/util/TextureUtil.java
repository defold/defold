package com.dynamo.bob.util;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

public class TextureUtil {
    public static int closestPOT(int i) {
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

    public static int getImageType(BufferedImage image) {
        int type = image.getType();
        if (type == 0) {
            switch (image.getColorModel().getNumComponents()) {
            case 4:
                type = BufferedImage.TYPE_4BYTE_ABGR;
                break;
            case 3:
                type = BufferedImage.TYPE_3BYTE_BGR;
                break;
            case 1:
                type = BufferedImage.TYPE_BYTE_GRAY;
                break;
            default:
                type = BufferedImage.TYPE_4BYTE_ABGR;
                break;
            }
        }
        return type;
    }

    public static List<BufferedImage> extrudeBorders(List<BufferedImage> srcImages, int extrudeBorders) {
        List<BufferedImage> result = srcImages;
        if (extrudeBorders > 0) {
            result = new ArrayList<BufferedImage>(srcImages.size());
            for (BufferedImage srcImage : srcImages) {
                result.add(extrudeBorders(srcImage, extrudeBorders));
            }
        }
        return result;
    }

    public static BufferedImage extrudeBorders(BufferedImage src, int extrudeBorders) {
        if (extrudeBorders > 0) {
            int origWidth = src.getWidth();
            int origHeight = src.getHeight();
            int newWidth = origWidth + extrudeBorders * 2;
            int newHeight = origHeight + extrudeBorders * 2;
            int type = getImageType(src);
            BufferedImage tgt = new BufferedImage(newWidth, newHeight, type);
            int numComponents = src.getColorModel().getNumComponents();
            int[] srcPixels = new int[origWidth * origHeight * numComponents];
            src.getRaster().getPixels(0, 0, origWidth, origHeight, srcPixels);
            int[] tgtPixels = new int[newWidth * newHeight * numComponents];
            for (int y = 0; y < newHeight; ++y) {
                for (int x = 0; x < newWidth; ++x) {
                    int index = (x + y * newWidth) * numComponents;
                    int sx = Math.min(Math.max(x - extrudeBorders, 0), origWidth-1);
                    int sy = Math.min(Math.max(y - extrudeBorders, 0), origHeight-1);
                    int sindex = (sx + sy * origWidth) * numComponents;
                    for (int i = 0; i < numComponents; ++i) {
                        tgtPixels[index + i] = srcPixels[sindex + i];
                    }
                }
            }
            tgt.getRaster().setPixels(0, 0, newWidth, newHeight, tgtPixels);
            return tgt;
        } else {
            return src;
        }
    }
}
