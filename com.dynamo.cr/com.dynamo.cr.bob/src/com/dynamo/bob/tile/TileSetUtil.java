package com.dynamo.bob.tile;

import java.awt.image.BufferedImage;
import java.awt.image.Raster;


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
        if (image == null && collisionImage == null) {
            return null;
        }
        Metrics metrics = new Metrics();
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

    public static class ConvexHulls {
        public ConvexHulls() {
        }
        public ConvexHulls(ConvexHull[] convexHulls, float[] convexHullPoints) {
            this.hulls = convexHulls;
            this.points = convexHullPoints;
        }
        public ConvexHulls(ConvexHulls convexHulls) {
            this.hulls = new ConvexHull[convexHulls.hulls.length];
            this.points = new float[convexHulls.points.length];
            System.arraycopy(convexHulls.hulls, 0, this.hulls, 0, convexHulls.hulls.length);
            System.arraycopy(convexHulls.points, 0, this.points, 0, convexHulls.points.length);
        }
        public ConvexHull[] hulls;
        public float[]   points;
    }

    private static ConvexHull2D.Point[] simplifyHull(ConvexHull2D.Point[] points, int width, int height, int hullTargetVertexCount) {
        if (hullTargetVertexCount != 0) {
            points = ConvexHull2D.simplifyHull(points, hullTargetVertexCount, width, height);

            if (points.length > hullTargetVertexCount) {
                // Fallback to the tight rect
                points = ConvexHull2D.calcRect(points);
            }
            else if (points.length < hullTargetVertexCount) {
                // Add degenerate triangles at the end
                ConvexHull2D.Point arr[] = new ConvexHull2D.Point[hullTargetVertexCount];
                for (int i = 0; i < points.length; ++i) {
                    arr[i] = points[i];
                }
                for (int i = points.length; i < hullTargetVertexCount; ++i) {
                    arr[i] = points[points.length-1]; // copy the last vertex, to make degenerate triangles
                }
                points = arr;
            }
        }
        return points;
    }

    public static ConvexHull2D.Point[] calculateConvexHulls(Raster alphaRaster, int planeCount, int hullTargetVertexCount) {
        int width = alphaRaster.getWidth();
        int height = alphaRaster.getHeight();
        int[] mask = new int[width * height];
        mask = alphaRaster.getPixels(0, 0, width, height, mask);

        ConvexHull2D.Point[] points = ConvexHull2D.imageConvexHull(mask, width, height, planeCount);
        return simplifyHull(points, width, height, hullTargetVertexCount);
    }

    public static ConvexHulls calculateConvexHulls(
            Raster alphaRaster, int planeCount,
            int width, int height, int tileWidth, int tileHeight,
            int tileMargin, int tileSpacing, int hullTargetVertexCount) {

        int tilesPerRow = TileSetUtil.calculateTileCount(tileWidth, width, tileMargin, tileSpacing);
        int tilesPerColumn = TileSetUtil.calculateTileCount(tileHeight, height, tileMargin, tileSpacing);
        ConvexHull2D.Point[][] points = new ConvexHull2D.Point[tilesPerRow * tilesPerColumn][];
        int pointCount = 0;
        int[] mask = new int[tileWidth * tileHeight];

        ConvexHull[] convexHulls = new ConvexHull[tilesPerColumn * tilesPerRow];

        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int x = tileMargin + col * (2 * tileMargin + tileSpacing + tileWidth);
                int y = tileMargin + row * (2 * tileMargin + tileSpacing + tileHeight);
                mask = alphaRaster.getPixels(x, y, tileWidth, tileHeight, mask);
                int index = col + row * tilesPerRow;
                points[index] = ConvexHull2D.imageConvexHull(mask, tileWidth, tileHeight, planeCount);

                if (hullTargetVertexCount != 0) {
                    points[index] = simplifyHull(points[index], tileWidth, tileHeight, hullTargetVertexCount);
                }

                ConvexHull convexHull = new ConvexHull(null, pointCount, points[index].length);
                convexHulls[index] = convexHull;
                pointCount += points[index].length;
            }
        }
        float[] convexHullPoints = new float[pointCount * 2];
        int totalIndex = 0;
        for (int row = 0; row < tilesPerColumn; ++row) {
            for (int col = 0; col < tilesPerRow; ++col) {
                int index = col + row * tilesPerRow;
                for (int i = 0; i < points[index].length; ++i) {
                    convexHullPoints[totalIndex++] = points[index][i].getX();
                    convexHullPoints[totalIndex++] = points[index][i].getY();
                }
            }
        }

        return new ConvexHulls(convexHulls, convexHullPoints);
    }

}
