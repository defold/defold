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

// $ java -cp ~/work/defold/tmp/dynamo_home/share/java/bob-light.jar com.dynamo.bob.tile.ConvexHull2D in.png out.png 6

package com.dynamo.bob.tile;

// The code below must remain identical to the implementation in the editor!
// ./editor/src/java/com/defold/editor/pipeline/ConvexHull2D.java

import java.util.Arrays;
import java.lang.Math;
import javax.vecmath.Vector2d;

// for easier debugging standalone
import java.io.IOException;
import java.io.File;
import javax.imageio.ImageIO;

import java.awt.*;
import java.awt.image.*;

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

    public static class PointF {
        public double x;
        public double y;

        public PointF(double x, double y) {
            this.x = x;
            this.y = y;
        }

        public double getX() {
            return x;
        }

        public void setX(double x) {
            this.x = x;
        }

        public double getY() {
            return y;
        }

        public void setY(double y) {
            this.y = y;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof PointF) {
                PointF p = (PointF) obj;
                return x == p.x && y == p.y;

            }
            return super.equals(obj);
        }

        @Override
        public int hashCode() {
            final int h1 = Double.hashCode(x);
            final int h2 = Double.hashCode(y);
            return h1 ^ h2;
        }

        @Override
        public String toString() {
            return String.format("(%f, %f)", x, y);
        }
    }

    public static class Line {
        PointF p;
        PointF vector;

        public Line(PointF p0, PointF p1) {
            this.p = p0;
            this.vector = new PointF(p1.x - p0.x, p1.y - p0.y);
        }

        private static double perp(double ux, double uy, double vx, double vy) {
            return ux * vy - uy * vx;
        }

        public static boolean intersect(Line l0, Line l1, PointF out) {
            double d = perp(l0.vector.x, l0.vector.y, l1.vector.x, l1.vector.y);
            if (Math.abs(d) < 0.000001f) {
                return false; // parallel lines
            }

            double t = perp(l1.vector.x, l1.vector.y, l0.p.x-l1.p.x, l0.p.y-l1.p.y) / d;

            if (t < 0.5) { // wrong side
                return false;
            }

            out.x = l0.p.x + t * l0.vector.x;
            out.y = l0.p.y + t * l0.vector.y;
            return true;
        }
    }

    // Used when inserting each center point of a texel
    static double supportCenter(int width, int height, int[] mask, Vector2d dir) {
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

    static double supportCorners(int width, int height, int[] mask, Vector2d dir) {
        double maxValue = -Double.MAX_VALUE;
        double centerX = width / 2.0;
        double centerY = height / 2.0;
        Vector2d p = new Vector2d();
        for (int y = height-1; y >= 0; --y) {
            for (int x = 0; x < width; ++x) {
                if (mask[x + (height - y - 1) * width] != 0) {
                    p.x = x + 0 - centerX;
                    p.y = y + 0 - centerY;
                    maxValue = Math.max(maxValue, p.dot(dir));

                    p.x = x + 1 - centerX;
                    p.y = y + 0 - centerY;
                    maxValue = Math.max(maxValue, p.dot(dir));

                    p.x = x + 1 - centerX;
                    p.y = y + 1 - centerY;
                    maxValue = Math.max(maxValue, p.dot(dir));

                    p.x = x + 0 - centerX;
                    p.y = y + 1 - centerY;
                    maxValue = Math.max(maxValue, p.dot(dir));
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


    static private boolean validHullF(PointF[] points, int[] mask, int width, int height) {
        int n = points.length;
        for (int i = 0; i < n; ++i) {
            PointF p0 = points[(i+1) % n];
            PointF p1 = points[i];
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

    /**
     * Get convex shape for a single image
     * @note the planes are always evenly distributed around the center at 360/nplanes increments
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

            double max = supportCenter(width, height, mask, dir);

            // Create a point from the direction and distance
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

    private static double areaX2(PointF p0, PointF p1, PointF p2) {
        // normally you'd divide by two, but we just need the area for sorting
        double v0x = p1.x - p0.x;
        double v0y = p1.y - p0.y;
        double v1x = p2.x - p0.x;
        double v1y = p2.y - p0.y;
        return (v0x*v1y) - (v0y*v1x); // a 3D cross product
    }

    public static double area(PointF[] points) {
        double a = 0;
        for (int i = 1; i < points.length-1; ++i) {
            a += ConvexHull2D.areaX2(points[0], points[i], points[i+1]);
        }
        return a * 0.5;
    }

    // Tries to remove the least significant edges of a hull
    // https://softwareengineering.stackexchange.com/a/395439
    public static PointF[] simplifyHull(PointF[] points, int targetCount) {
        while (points.length > targetCount) {

            int indexOfLeastSignificance = -1;
            double leastArea = 2;
            PointF intersectionPoint = null;
            boolean duplicateFound = false;

            for (int i = 0; i < points.length; ++i) {
                PointF p0 = points[(i-1+points.length)%points.length];
                PointF p1 = points[i];
                PointF p2 = points[(i+1)%points.length];
                PointF p3 = points[(i+2)%points.length];

                double diffX = p2.x - p1.x;
                double diffY = p2.y - p1.y;
                double distSquared = diffX*diffX + diffY*diffY;
                boolean pointsEqual = distSquared < 0.00001f;

                if (pointsEqual) {
                    duplicateFound = true;
                    indexOfLeastSignificance = i;
                    break;
                }

                Line l0 = new Line(p0, p1);
                Line l1 = new Line(p2, p3);

                // Project the line (p1-p0) onto the line (p2-p3)
                PointF pf = new PointF(-1, -1);
                if (!Line.intersect(l0, l1, pf)) {
                    continue;
                }

                // check that the intersection is inside the box
                if (Math.abs(pf.x) > 0.5 || Math.abs(pf.y) > 0.5) {
                    continue;
                }

                // calculate the area of triangle [p1, p2, I]
                double area = ConvexHull2D.areaX2(p1, p2, pf);
                if (area < leastArea) {
                    indexOfLeastSignificance = i;
                    leastArea = area;
                    intersectionPoint = pf;
                }
            }

            if (indexOfLeastSignificance == -1) {
                break;
            }

            // We have chosen an edge to remove
            if (!duplicateFound) {
                points[(indexOfLeastSignificance+1)%points.length] = intersectionPoint;
            }

            PointF[] refined = new PointF[points.length-1];
            System.arraycopy(points, 0, refined, 0, indexOfLeastSignificance);
            System.arraycopy(points, indexOfLeastSignificance + 1, refined, indexOfLeastSignificance, points.length - indexOfLeastSignificance - 1);

            points = refined;
        }
        return points;
    }

    // we don't want to generate invalid hulls by exceeding the bounding box by a small amount
    // so if the value is outside the box, by a very small amount, we round it to the box edge
    private static double roundEdgeValue(double v) {
        if (Math.abs(v) <= 0.5) {
            return v;
        }
        if (v < -0.5 && (v + 0.5) > -0.0001) {
            return -0.5;
        }
        if (v > 0.5 && (v - 0.5) < 0.0001) {
            return 0.5;
        }
        return v;
    }

    /**
     * @note returns CW winding
     * @return a PointF array where each point is in the space [-0.5, 0.5]
     */
    public static PointF[] imageConvexHullCorners(int[] mask, int width, int height, int targetCount) {
        final int nplanes = 16;
        Vector2d[] points = new Vector2d[nplanes];
        Vector2d[] tangents = new Vector2d[nplanes];

        double max_dim = (double)Math.max(width, height);

        Vector2d dir = new Vector2d();
        for (int i = 0; i < nplanes; ++i) {
            double angle = i * 2.0 * Math.PI / ((double) nplanes - 0);
            dir.x = Math.cos(angle);
            dir.y = Math.sin(angle);

            dir.normalize();
            tangents[i] = new Vector2d(-dir.y, dir.x);

            double max = supportCorners(width, height, mask, dir);

            // Create a point from the direction and distance
            dir.scale(max);
            dir.x /= max_dim;
            dir.y /= max_dim;

            points[i] = new Vector2d(dir.x, dir.y);
        }

        PointF[] result = new PointF[nplanes];
        int npoints = 0;
        for (int i = 0; i < nplanes; ++i) {
            // Calculate the intersection point between two planes
            Vector2d p0 = points[i];
            Vector2d p1 = points[(i + 1) % nplanes];

            Vector2d d0 = tangents[i];
            Vector2d d1 = tangents[(i + 1) % nplanes];

            double t = ((p0.y - p1.y) * d1.x - (p0.x - p1.x) * d1.y) / (d0.x * d1.y - d1.x * d0.y);
            Vector2d inter = new Vector2d(d0);
            inter.scaleAdd(t, p0);

            // Currently the the point is expressed in the unit space of (1 / max_dim)
            // We need it to be in the space(s) of the (1/width, 1/height)
            double x = inter.x * max_dim;
            double y = inter.y * max_dim;
            x = x / (double)width;
            y = y / (double)height;

            x = roundEdgeValue(x);
            y = roundEdgeValue(y);
            result[npoints++] = new PointF(x, y);
        }

        // Reverse the list to make it CW
        PointF[] tmp = new PointF[npoints];
        for (int i = 0; i < npoints; ++i) {
            tmp[i] = result[npoints-i-1];
        }
        result = tmp;

        return simplifyHull(result, targetCount);
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

    // Use helper script test_convexhull2d.sh to run this main function
    public static void main(String[] args) throws IOException {
        if (args.length < 1) {
            System.out.println("No image specified!");
            return;
        }

        if (args.length >= 2 && args[0].equals(args[1])) {
            System.out.println("Source and target images cannot be the same!");
            return;
        }

        int numTargetVertices = 4;
        if (args.length >= 3) {
            numTargetVertices = Integer.valueOf(args[2]);
        }

        int dilateCount = 0;
        if (args.length >= 4) {
            dilateCount = Integer.valueOf(args[3]);
        }

        for (String arg : args) {
            System.out.println("ARG: " + arg);
        }

        BufferedImage img = null;
        try {
            img = ImageIO.read(new File(args[0]));
        } catch (IOException e) {
            System.out.println("Couldn't read image: " + args[0]);
            return;
        }

        // // Create mask
        int width = img.getWidth();
        int height = img.getHeight();
        System.out.println(String.format("w/h: %d x %d", width, height));


        int maxX = -1;
        int maxY = -1;
        int minX = width+1;
        int minY = height+1;

        int any_zero_alpha = 0;
        int mask[] = new int[width*height];
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int color = img.getRGB(x, y); // bgra
                int alpha = (color >> 24) & 0xff;
                mask[y * width + x] = alpha == 0 ? 0 : 1;
                any_zero_alpha |= alpha == 0 ? 1 : 0;

                if (alpha > 0) {
                    if (x > maxX)
                        maxX = x;
                    if (x < minX)
                        minX = x;
                    if (y > maxY)
                        maxY = y;
                    if (y < minY)
                        minY = y;
                }
            }
        }

        System.out.println(String.format("dilateCount: %d", dilateCount));
        System.out.println(String.format("any_zero_alpha!: %d", any_zero_alpha));
        System.out.println(String.format("numTargetVertices: %d", numTargetVertices));

        if (dilateCount > 0) {
            mask = dilate(mask, width, height, dilateCount*2 + 1);
        }

        PointF[] points = imageConvexHullCorners(mask, width, height, numTargetVertices);

        Graphics2D g2d = img.createGraphics();
        BasicStroke bs = new BasicStroke(1);
        g2d.setStroke(bs);

        g2d.setColor(Color.BLUE);
        g2d.drawLine(minX, minY, minX, maxY);
        g2d.drawLine(maxX, minY, maxX, maxY);
        g2d.drawLine(minX, minY, maxX, minY);
        g2d.drawLine(minX, maxY, maxX, maxY);

        g2d.setColor(Color.RED);

        System.out.println(String.format("Points: %d", points.length));
        for (int i = 0; i < points.length; ++i) {
            PointF point = points[i];
            PointF pointNext = points[(i+1)%points.length];

            Point ipoint = new Point((int)((point.getX() + 0.5) * width), (int)((point.getY() + 0.5) * height));
            Point ipointNext = new Point((int)((pointNext.getX() + 0.5) * width), (int)((pointNext.getY() + 0.5) * height));

            System.out.println(String.format("  %2d: %f x %f  %d x %d", i, point.getX(), point.getY(), ipoint.getX(), ipoint.getY()));

            g2d.drawLine(ipoint.getX(), height-ipoint.getY(), ipointNext.getX(), height-ipointNext.getY());
        }

        if (args.length >= 2) {
            // Write output
            try {
                File outputfile = new File(args[1]);
                String ext = args[1].substring(args[1].lastIndexOf(".")+1);
                ImageIO.write(img, ext, outputfile);
                System.out.println("Wrote " + args[1]);
            } catch (IOException e) {
                System.out.println("Couldn't write image: " + args[1]);
                return;
            }
        }
    }
}
