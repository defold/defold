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

package com.dynamo.bob.tile.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertThat;
import static org.junit.matchers.JUnitMatchers.hasItem;

import java.awt.image.BufferedImage;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.ByteOrder;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;

import javax.imageio.ImageIO;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.tile.ConvexHull2D;
import com.dynamo.bob.tile.ConvexHull2D.Point;

public class ConvexHull2DTest {

    @Before
    public void setUp() {
        System.setProperty("java.awt.headless", "true");
    }

    static HashSet<Point> calc(String fileName, int planeCount) throws IOException {
        BufferedImage image = ImageIO.read(new FileInputStream(fileName));
        int width = image.getWidth();
        int height = image.getHeight();
        int[] mask = image.getAlphaRaster().getPixels(0, 0, width, height, new int[width * height]);

        Point[] points = ConvexHull2D.imageConvexHull(mask, width, height, planeCount);
        return new HashSet<Point>(Arrays.asList(points));
    }

    static HashSet<Point> calcTrim(String fileName, int targetCount) throws IOException {
        BufferedImage image = ImageIO.read(new FileInputStream(fileName));
        int width = image.getWidth();
        int height = image.getHeight();
        int[] mask = image.getAlphaRaster().getPixels(0, 0, width, height, new int[width * height]);

        ConvexHull2D.PointF[] pointsf = ConvexHull2D.imageConvexHullCorners(mask, width, height, targetCount);
        Point[] points = new Point[pointsf.length];
        for (int i = 0; i < pointsf.length; ++i) {
            double x = (pointsf[i].x + 0.5) * (double)width;
            double y = (pointsf[i].y + 0.5) * (double)height;
            points[i] = new Point((int)x, (int)y);
        }
        return new HashSet<Point>(Arrays.asList(points));
    }

    @Test
    public void test45() throws Exception {
        for (int planeCount : new int[] { 8, 16 }) {
            HashSet<Point> points = calc("test/test_45.png", planeCount);
            assertEquals(3, points.size());
            assertThat(points, hasItem(new Point(0, 0)));
            assertThat(points, hasItem(new Point(7, 7)));
            assertThat(points, hasItem(new Point(7, 0)));
        }
    }

    @Test
    public void testCenterBox() throws Exception {
        for (int planeCount : new int[] { 8, 16 }) {
            HashSet<Point> points = calc("test/test_center_box.png", planeCount);
            assertEquals(4, points.size());
            assertThat(points, hasItem(new Point(1, 1)));
            assertThat(points, hasItem(new Point(1, 6)));
            assertThat(points, hasItem(new Point(6, 1)));
            assertThat(points, hasItem(new Point(6, 6)));
        }
    }

    @Test
    public void testGround() throws Exception {
        for (int planeCount : new int[] { 8, 16 }) {
            HashSet<Point> points = calc("test/test_ground.png", planeCount);
            assertEquals(4, points.size());
            assertThat(points, hasItem(new Point(0, 0)));
            assertThat(points, hasItem(new Point(0, 3)));
            assertThat(points, hasItem(new Point(7, 0)));
            assertThat(points, hasItem(new Point(7, 3)));
        }
    }

    @Test
    public void testGroundPrim() throws Exception {
        /*
         * NOTE: This is perhaps a bit fragile to test
         * but interesting that a more optimal solution is
         * found when starting from 16 planes
         */

        {
            HashSet<Point> points = calc("test/test_ground_prim.png", 8);

            assertEquals(6, points.size());
            assertThat(points, hasItem(new Point(0, 0)));
            assertThat(points, hasItem(new Point(0, 3)));
            assertThat(points, hasItem(new Point(7, 0)));
            assertThat(points, hasItem(new Point(7, 3)));

            // Additional points (compared to test_ground.png)
            assertThat(points, hasItem(new Point(1, 4)));
            assertThat(points, hasItem(new Point(6, 4)));
        }

        {
            HashSet<Point> points = calc("test/test_ground_prim.png", 16);

            assertEquals(5, points.size());
            assertThat(points, hasItem(new Point(0, 0)));
            assertThat(points, hasItem(new Point(0, 3)));
            assertThat(points, hasItem(new Point(7, 0)));
            assertThat(points, hasItem(new Point(7, 3)));

            // Additional points (compared to test_ground.png)
            // NOTE: This is more optimal than the above
            // that solves for 8 planes
            assertThat(points, hasItem(new Point(5, 4)));
        }
    }

    @Test
    public void testPoints() throws Exception {
        for (int planeCount : new int[] { 8, 16 }) {
            HashSet<Point> points = calc("test/test_points.png", planeCount);
            assertEquals(3, points.size());

            assertThat(points, hasItem(new Point(1, 1)));
            assertThat(points, hasItem(new Point(3, 6)));
            assertThat(points, hasItem(new Point(7, 0)));
        }
    }

    private static byte[] createImageBytes(int width, int height) {
        final int bytes_per_pixel = 4;
        return new byte[width * height * bytes_per_pixel];
    }

    private static void plotTexelARGB(byte[] data, int width, int height, int x, int y, int r, int g, int b, int a) {
        final int bytes_per_pixel = 4;
        int index = y * height * bytes_per_pixel + x * bytes_per_pixel;
        data[index+0] = (byte)a;
        data[index+1] = (byte)r;
        data[index+2] = (byte)g;
        data[index+3] = (byte)b;
    }

    private static BufferedImage getTestImage(byte[] data, int width, int height) {
        BufferedImage image = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
        IntBuffer intBuf = ByteBuffer.wrap(data)
                                     .order(ByteOrder.LITTLE_ENDIAN)
                                     .asIntBuffer();
        int[] array = new int[intBuf.remaining()];
        intBuf.get(array);
        image.setRGB(0, 0, width, height, array, 0, width);
        return image;
    }

    private static int[] getImageAlpha(BufferedImage image) {
        int width = image.getWidth();
        int height = image.getHeight();
        int[] alpha = new int[width * height];
        alpha = image.getAlphaRaster().getPixels(0, 0, width, height, alpha);
        return alpha;
    }

    private boolean hasArrayItem(ConvexHull2D.PointF[] points, ConvexHull2D.PointF point) {
        for (ConvexHull2D.PointF p : points) {
            double dx = point.getX() - p.getX();
            double dy = point.getY() - p.getY();
            if ( (dx*dx+dy*dy) < 0.001 ) {
                return true;
            }
        }
        return false;
    }


    private ConvexHull2D.PointF sub(ConvexHull2D.PointF a, ConvexHull2D.PointF b) {
        return new ConvexHull2D.PointF(b.getX()-a.getX(), b.getY()-a.getY());
    }

    private double simpleCross(ConvexHull2D.PointF a, ConvexHull2D.PointF b) {
        // Since we know the z=0, the X/Y of the cross product is 0, and only the Z component is valid
        // x = Ay * Bz - By * Az
        // y = Az * Bx - Bz * Ax
        double z = a.getX() * b.getY() - b.getX() * a.getY();
        // <0 CW
        // >0 CCW
        return z;
    }

    @Test
    public void testImageHullWinding() {
        int width;
        int height;
        byte[] data;
        BufferedImage image;
        int offset;
        ConvexHull2D.PointF[] points;

        // *****************************************************************************************
        width = 64;
        height = 64;
        offset = 0;
        data = createImageBytes(width, height);

        plotTexelARGB(data, width, height,       offset  ,        offset , 1,2,3,4);
        plotTexelARGB(data, width, height, width-offset-1, height-offset-1, 1,2,3,4);
        plotTexelARGB(data, width, height,       offset  , height-offset-1, 1,2,3,4);
        plotTexelARGB(data, width, height, width-offset-1,        offset , 1,2,3,4);
        image = getTestImage(data, width, height);

        points = ConvexHull2D.imageConvexHullCorners(getImageAlpha(image), width, height, 4);

        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF(-0.5, -0.5)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF(-0.5,  0.5)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF( 0.5, -0.5)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF( 0.5,  0.5)));

        // *****************************************************************************************
        width = 64;
        height = 64;
        offset = 16;
        data = createImageBytes(width, height);

        plotTexelARGB(data, width, height,       offset  ,        offset , 1,2,3,4);
        plotTexelARGB(data, width, height, width-offset-1, height-offset-1, 1,2,3,4);
        plotTexelARGB(data, width, height,       offset  , height-offset-1, 1,2,3,4);
        plotTexelARGB(data, width, height, width-offset-1,        offset , 1,2,3,4);
        image = getTestImage(data, width, height);

        points = ConvexHull2D.imageConvexHullCorners(getImageAlpha(image), width, height, 4);

        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF(-0.25, -0.25)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF(-0.25,  0.25)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF( 0.25, -0.25)));
        assertTrue(hasArrayItem(points, new ConvexHull2D.PointF( 0.25,  0.25)));

        // Assert CCW
        assertTrue(simpleCross(sub(points[1], points[0]), sub(points[2], points[0])) < 0);
        assertTrue(simpleCross(sub(points[2], points[0]), sub(points[3], points[0])) < 0);
    }

    @Test
    public void testImageHullTargetCount() throws Exception {
        HashSet<Point> points = calcTrim("test/test_image_7389.png", 8);

        // 0: 0.364769 x -0.014665  243 x 201
        // 1: 0.144800 x -0.374244  181 x 52
        // 2: -0.010580 x -0.479453  137 x 8
        // 3: -0.336299 x -0.388100  45 x 46
        // 4: -0.336299 x -0.091389  46 x 169
        // 5: -0.005227 x 0.449809  139 x 394
        // 6: 0.279224 x 0.370030  218 x 361
        // 7: 0.364769 x 0.230191  243 x 303
        assertEquals(8, points.size());
        assertThat(points, hasItem(new Point(243, 201)));
        assertThat(points, hasItem(new Point(181, 52)));
        assertThat(points, hasItem(new Point(137, 8)));
        assertThat(points, hasItem(new Point(45, 46)));
        assertThat(points, hasItem(new Point(46, 169)));
        assertThat(points, hasItem(new Point(139, 394)));
        assertThat(points, hasItem(new Point(218, 361)));
        assertThat(points, hasItem(new Point(243, 303)));
    }

}
