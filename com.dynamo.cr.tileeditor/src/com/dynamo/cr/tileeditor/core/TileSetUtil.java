package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;

public class TileSetUtil {
    public static int calculateTileCount(int tileSize, int imageSize, int tileMargin, int tileSpacing) {
        int actualTileSize = (2 * tileMargin + tileSpacing + tileSize);
        if (actualTileSize > 0) {
            return (imageSize + tileSpacing)/actualTileSize;
        } else {
            return 0;
        }
    }

    public static class Metrics {
        public int tilesPerRow;
        public int tilesPerColumn;
        public int tileSetWidth;
        public int tileSetHeight;
        public float visualWidth;
        public float visualHeight;
    }

    public static Metrics calculateMetrics(BufferedImage image, int tileWidth, int tileHeight, int tileMargin, int tileSpacing, BufferedImage collisionImage, float scale, float border) {
        Metrics metrics = new Metrics();
        if (image == null && collisionImage == null) {
            return metrics;
        }
        if (image != null) {
            metrics.tileSetWidth = image.getWidth();
            metrics.tileSetHeight = image.getHeight();
        } else {
            metrics.tileSetWidth = collisionImage.getWidth();
            metrics.tileSetHeight = collisionImage.getHeight();
        }

        metrics.tilesPerRow = calculateTileCount(tileWidth, metrics.tileSetWidth, tileMargin, tileSpacing);
        metrics.tilesPerColumn = calculateTileCount(tileHeight, metrics.tileSetHeight, tileMargin, tileSpacing);

        if (metrics.tilesPerRow <= 0 || metrics.tilesPerColumn <= 0) {
            return null;
        }

        float pixelBorderSize = border / scale;
        metrics.visualWidth = (tileWidth + pixelBorderSize) * metrics.tilesPerRow + pixelBorderSize;
        metrics.visualHeight = (tileHeight + pixelBorderSize) * metrics.tilesPerColumn + pixelBorderSize;
        return metrics;
    }

}
