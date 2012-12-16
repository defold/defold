package com.dynamo.cr.tileeditor.pipeline.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.junit.matchers.JUnitMatchers.hasItem;

import java.awt.image.BufferedImage;
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

}
