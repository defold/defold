package com.dynamo.cr.tileeditor.pipeline.test;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.assertThat;

import java.awt.image.BufferedImage;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;

import javax.imageio.ImageIO;

import org.junit.Test;

import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D;
import com.dynamo.cr.tileeditor.pipeline.ConvexHull2D.Point;

public class ConvexShapeFitterTest {

    static HashSet<Point> calc(String fileName) throws IOException {
        BufferedImage image = ImageIO.read(new FileInputStream(fileName));
        int width = image.getWidth();
        int height = image.getHeight();
        int[] mask = image.getAlphaRaster().getPixels(0, 0, width, height, new int[width * height]);

        Point[] points = ConvexHull2D.imageConvexHull(mask, width, height, 8);
        return new HashSet<Point>(Arrays.asList(points));
    }

    @Test
    public void test45() throws Exception {
        HashSet<Point> points = calc("test/test_45.png");
        assertThat(points.size(), is(3));
        assertThat(points, hasItem(new Point(0, 0)));
        assertThat(points, hasItem(new Point(7, 7)));
        assertThat(points, hasItem(new Point(7, 0)));
    }

    @Test
    public void testCenterBox() throws Exception {
        HashSet<Point> points = calc("test/test_center_box.png");
        assertThat(points.size(), is(4));
        assertThat(points, hasItem(new Point(1, 1)));
        assertThat(points, hasItem(new Point(1, 6)));
        assertThat(points, hasItem(new Point(6, 1)));
        assertThat(points, hasItem(new Point(6, 6)));
    }

    @Test
    public void testGround() throws Exception {
        HashSet<Point> points = calc("test/test_ground.png");
        assertThat(points.size(), is(4));
        assertThat(points, hasItem(new Point(0, 0)));
        assertThat(points, hasItem(new Point(0, 3)));
        assertThat(points, hasItem(new Point(7, 0)));
        assertThat(points, hasItem(new Point(7, 3)));
    }

    @Test
    public void testGroundPrim() throws Exception {
        HashSet<Point> points = calc("test/test_ground_prim.png");
        System.out.println(points);
        assertThat(points.size(), is(6));
        assertThat(points, hasItem(new Point(0, 0)));
        assertThat(points, hasItem(new Point(0, 3)));
        assertThat(points, hasItem(new Point(7, 0)));
        assertThat(points, hasItem(new Point(7, 3)));

        // Additional points (compared to test_ground.png)
        assertThat(points, hasItem(new Point(1, 4)));
        assertThat(points, hasItem(new Point(6, 4)));
    }

    @Test
    public void testPoints() throws Exception {
        HashSet<Point> points = calc("test/test_points.png");
        assertThat(points.size(), is(6));

        assertThat(points, hasItem(new Point(1, 1)));
        assertThat(points, hasItem(new Point(2, 0)));
        assertThat(points, hasItem(new Point(1, 4)));
        assertThat(points, hasItem(new Point(3, 6)));
        assertThat(points, hasItem(new Point(7, 2)));
        assertThat(points, hasItem(new Point(7, 0)));
    }

}
