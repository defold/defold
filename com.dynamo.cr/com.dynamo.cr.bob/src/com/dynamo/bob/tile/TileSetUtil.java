// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob.tile;

import com.dynamo.bob.textureset.TextureSetLayout.Rect;

// The code below must remain identical to the implementation in the editor!
// ./editor/src/java/com/defold/editor/pipeline/TileSetUtil.java

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

    public static Metrics calculateMetrics(Rect image, int tileWidth, int tileHeight, int tileMargin, int tileSpacing, Rect collisionImage, float scale, float border) {
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

    private static int Max(int a, int b) { return a > b ? a : b; }
    private static int Min(int a, int b) { return a < b ? a : b; }

    // Returns a CW rect
    private static ConvexHull2D.Point[] calcRect(int[] alpha, int width, int height, int inflate) {
        int maxX = -1;
        int maxY = -1;
        int minX = width + 1;
        int minY = width + 1;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int a = alpha[y * width + x];
                if (a != 0) {
                    maxX = Max(maxX, x);
                    maxY = Max(maxY, y);
                    minX = Min(minX, x);
                    minY = Min(minY, y);
                }
            }
        }

        minX = Max(0, minX - inflate);
        minY = Max(0, minY - inflate);
        maxX = Min(width, maxX + inflate);
        maxY = Min(height, maxY + inflate);

        ConvexHull2D.Point[] points = new ConvexHull2D.Point[4];
        points[0] = new ConvexHull2D.Point(minX, minY);
        points[1] = new ConvexHull2D.Point(minX, maxY);
        points[2] = new ConvexHull2D.Point(maxX, maxY);
        points[3] = new ConvexHull2D.Point(maxX, minY);
        return points;
    }

    public static boolean isHullValid(ConvexHull2D.Point[] points, int width, int height) {
        for (ConvexHull2D.Point point : points) {
            if (point.x < 0 || point.x > width ||
                point.y < 0 || point.y > height) {
                return false;
            }
        }
        return true;
    }

    public static boolean isHullValid(ConvexHull2D.PointF[] points) {
        for (ConvexHull2D.PointF point : points) {
            if (point.x < -0.5 || point.x > 0.5 ||
                point.y < -0.5 || point.y > 0.5) {
                return false;
            }
        }
        return true;
    }
    private static int nonzeroValue(int[] mask, int width, int height, int x, int y, int kernelSize)
    {
        int kernelHalfSize = kernelSize / 2;
        for (int ky = -kernelHalfSize; ky <= kernelHalfSize; ++ky) {
            int yy = y + ky;
            if (yy < 0 || yy >= height)
                continue;
            for (int kx = -kernelHalfSize; kx <= kernelHalfSize; ++kx) {
                int xx = x + kx;
                if (xx < 0 || xx >= width)
                    continue;
                if (mask[yy * width + xx] != 0)
                    return 1;
            }
        }
        return 0;
    }

    private static int[] dilate(int[] mask, int width, int height, int kernelSize) {
        int[] tmp = new int[width*height];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                tmp[y * width + x] = nonzeroValue(mask, width, height, x, y, kernelSize);
            }
        }
        return tmp;
    }

    private static boolean isEmpty(int[] mask, int width, int height) {
        for (int i = 0; i < width*height; ++i) {
            if (mask[i] != 0)
                return false;
        }
        return true;
    }

    // returns a tight rect with CW winding
    public static ConvexHull2D.PointF[] calculateRect(Raster alphaRaster, int inflate) {
        int width = alphaRaster.getWidth();
        int height = alphaRaster.getHeight();
        int[] alpha = new int[width * height];
        alpha = alphaRaster.getPixels(0, 0, width, height, alpha);

        ConvexHull2D.Point ipoints[] = calcRect(alpha, width, height, inflate);
        ConvexHull2D.PointF points[] = new ConvexHull2D.PointF[4];

        // make sure we don't change winding
        points[1] = new ConvexHull2D.PointF(ipoints[0].x / (double)width - 0.5, -(ipoints[0].y / (double)height - 0.5));
        points[0] = new ConvexHull2D.PointF(ipoints[1].x / (double)width - 0.5, -(ipoints[1].y / (double)height - 0.5));
        points[3] = new ConvexHull2D.PointF(ipoints[2].x / (double)width - 0.5, -(ipoints[2].y / (double)height - 0.5));
        points[2] = new ConvexHull2D.PointF(ipoints[3].x / (double)width - 0.5, -(ipoints[3].y / (double)height - 0.5));
        return points;
    }

    public static ConvexHull2D.PointF[] calculateConvexHull(Raster alphaRaster, int hullTargetVertexCount, int dilateCount) {
        int width = alphaRaster.getWidth();
        int height = alphaRaster.getHeight();
        int[] alpha = new int[width * height];
        alpha = alphaRaster.getPixels(0, 0, width, height, alpha);

        if (isEmpty(alpha, width, height))
            return null;

        if (dilateCount > 0) {
            alpha = dilate(alpha, width, height, dilateCount * 2 + 1);
        }

        ConvexHull2D.PointF[] points = ConvexHull2D.imageConvexHullCorners(alpha, width, height, hullTargetVertexCount);

        // Check the vertices, and if they're outside of the rectangle, fallback to the tight rect
        if (!isHullValid(points)) {
            return null;
        }
        return points;
    }

    // for the physics collision hulls
    public static ConvexHulls calculateConvexHulls(
            Raster alphaRaster, int hullTargetVertexCount,
            int width, int height, int tileWidth, int tileHeight,
            int tileMargin, int tileSpacing) {

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
                points[index] = ConvexHull2D.imageConvexHull(mask, tileWidth, tileHeight, hullTargetVertexCount);

                // Check the vertices, and if they're outside of the rectangle, fallback to the tight rect
                if (!isHullValid(points[index], tileWidth, tileHeight)) {
                    points[index] = calcRect(mask, tileWidth, tileHeight, 0);
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
