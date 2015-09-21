package com.defold.editor.pipeline;

import java.util.Arrays;
import javax.vecmath.Vector2d;

/*
 * Intersection of two lines:
 *
 * PX0 + DX0 * ta = PX1 + DX1 * tb
 * PY0 + DY0 * ta = PY1 + DY1 * tb
 *
 * PX0 + DX0 * ta - PX1    PY0 + DY0 * ta - PY1
 * -------------------- =  --------------------
 *          DX1                        DY1
 *
 * (PX0 - PX1) * DY1 + DX0 * DY1 * ta =  (PY0 - PY1) * DX1 + DX1 * DY0 * ta
 *
 * (DX0 * DY1 - DX1 * DY0) * ta = (PY0 - PY1) * DX1 - (PX0 - PX1) * DY1
 *
 *        (PY0 - PY1) * DX1 - (PX0 - PX1) * DY1
 * ta =   -------------------------------------
 *               (DX0 * DY1 - DX1 * DY0)
 *
 */

/**
 * Simple convex shaper fitting using k-DOP style method
 * Origin for the coordinate system for the convex hull is located in the lower left.
 * Input images are in regular "image space" with origin in upper left.
 * Pixel are treated as points in the domain [0, width-1] and [0, height-1]. If pixel center
 * is preferred the resulting convex hull should be translated by a half unit.
 * @author chmu
 *
 */
public class ConvexHull2D {

    /**
     * Representation of a convex hull point
     * @author chmu
     *
     */
    public static class Point {
        int x;
        int y;

        public Point(int x, int y) {
            this.x = x;
            this.y = y;
        }

        public int getX() {
            return x;
        }

        public void setX(int x) {
            this.x = x;
        }

        public int getY() {
            return y;
        }

        public void setY(int y) {
            this.y = y;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof Point) {
                Point p = (Point) obj;
                return x == p.x && y == p.y;

            }
            return super.equals(obj);
        }

        @Override
        public int hashCode() {
            return x ^ y;
        }

        @Override
        public String toString() {
            return String.format("(%d, %d)", x, y);
        }
    }

    static double support(int width, int height, int[] mask, Vector2d dir) {

        double maxValue = -Double.MAX_VALUE;
        Vector2d p = new Vector2d();
        for (int y = height-1; y >= 0; --y) {
            for (int x = 0; x < width; ++x) {
                if (mask[x + (height - y - 1) * width] != 0) {
                    p.x = x - (width - 1.0) / 2.0;
                    p.y = y - (height - 1.0) / 2.0;
                    double len = p.dot(dir);
                    maxValue = Math.max(maxValue, len);
                }
            }
        }
        return maxValue;
    }

    static boolean validHull(Point[] points, int[] mask, int width, int height) {
        int n = points.length;
        for (int i = 0; i < n; ++i) {
            Point p0 = points[(i+1) % n];
            Point p1 = points[i];
            Vector2d normal = new Vector2d(-(p1.y - p0.y), p1.x - p0.x);
            normal.normalize();
            Vector2d p = new Vector2d();

            for (int y = height-1; y >= 0; --y) {
                for (int x = 0; x < width; ++x) {
                    if (mask[x + (height - y - 1) * width] != 0) {
                        p.x = p0.x - x;
                        p.y = p0.y - y;
                        double distance = p.dot(normal);
                        if (distance < -0.01) // TODO: Epsilon for floats...
                            return false;
                    }
                }
            }
        }

        return true;
    }

    static Point[] refine(Point[] points, int[] mask, int width, int height) {
        int n = points.length;

        boolean wasRefined;
        do {
            wasRefined = false;
            for (int i = 0; i < n; ++i) {
                Point[] refined = new Point[n-1];
                System.arraycopy(points, 0, refined, 0, i);
                System.arraycopy(points, i + 1, refined, i, n - i - 1);
                if (validHull(refined, mask, width, height)) {
                    // Successfully removed a point
                    points = refined;
                    --n;
                    --i;
                    wasRefined = true;
                }
                n = points.length;
            }
        } while (n > 3 && wasRefined);

        return points;
    }

    /**
     * Get convex shape for a single image
     * @param mask image mask. 0 is interpreted as background. != 0 is interpreted as foreground
     * @param width image width
     * @param height image height
     * @param nplanes number of planes to use when fitting
     * @return convex hull
     */
    public static Point[] imageConvexHull(int[] mask, int width, int height, int nplanes) {
        Vector2d[] points = new Vector2d[nplanes];
        Vector2d[] tangents = new Vector2d[nplanes];

        Vector2d dir = new Vector2d();
        for (int i = 0; i < nplanes; ++i) {
            double angle = i * 2.0 * Math.PI / ((double) nplanes - 0);
            dir.x = Math.cos(angle);
            dir.y = Math.sin(angle);

            dir.normalize();
            tangents[i] = new Vector2d(-dir.y, dir.x);

            double max = support(width, height, mask, dir);
            dir.scale(max);
            dir.x += (width - 1.0) / 2.0;
            dir.y += (height - 1.0) / 2.0;
            points[i] = new Vector2d(dir.x, dir.y);
        }

        Point[] result = new Point[nplanes];
        int npoints = 0;
        for (int i = 0; i < nplanes; ++i) {
            Vector2d p0 = points[i];
            Vector2d p1 = points[(i + 1) % nplanes];

            Vector2d d0 = tangents[i];
            Vector2d d1 = tangents[(i + 1) % nplanes];

            double t = ((p0.y - p1.y) * d1.x - (p0.x - p1.x) * d1.y) / (d0.x * d1.y - d1.x * d0.y);
            Vector2d inter = new Vector2d(d0);
            inter.scaleAdd(t, p0);

            Point p = new Point((int) Math.round(inter.x), (int) Math.round(inter.y));

            boolean found = false;
            for (int j = 0; j < npoints; ++j) {
                if (p.equals(result[j])) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                result[npoints++] = p;
            }
        }

        Point[] distinct = Arrays.copyOf(result, npoints);
        return refine(distinct, mask, width, height);
    }

}
