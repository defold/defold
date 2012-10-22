package com.dynamo.cr.parted.curve.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import com.dynamo.cr.parted.curve.HermiteSpline;

public class HermiteSplineTest {

    static double EPSILON = 0.001;

    @Test
    public void defaultSpline() {
        HermiteSpline spline = new HermiteSpline();
        double[] value = new double[2];

        spline.getValue(0, 0, value);
        assertEquals(0, value[0], EPSILON);
        assertEquals(0, value[1], EPSILON);
        spline.getTangent(0, 0, value);
        assertEquals(1 / Math.sqrt(2), value[0], EPSILON);
        assertEquals(1 / Math.sqrt(2), value[1], EPSILON);

        spline.getValue(0, 1, value);
        assertEquals(1, value[0], EPSILON);
        assertEquals(1, value[1], EPSILON);
        spline.getTangent(0, 1, value);
        assertEquals(1 / Math.sqrt(2), value[0], EPSILON);
        assertEquals(1 / Math.sqrt(2), value[1], EPSILON);
    }

    @Test
    public void insert() {
        HermiteSpline spline = new HermiteSpline();
        double[] pre = new double[2];
        double[] post = new double[2];
        spline.getTangent(0, 0.5, pre);
        spline = spline.insertPoint(0.5, 0.5);
        assertEquals(3, spline.getCount());
        spline.getTangent(0, 0.5, post);
        assertEquals(pre[0], post[0], EPSILON);
        assertEquals(pre[1], post[1], EPSILON);
    }

    @Test
    public void testContinuity() {
        HermiteSpline spline = new HermiteSpline();
        spline = spline.setPosition(0, 0, 0);
        spline = spline.setTangent(0, 1, 1);
        spline = spline.setPosition(1, 0, 0);
        spline = spline.setTangent(0, 1, -1);

        spline = spline.insertPoint(0.25, 0.5);
        spline = spline.setTangent(1, 1, -3);

        double[] v1 = new double[2];
        double[] v2 = new double[2];
        spline.getTangent(0, 1 - EPSILON, v1);
        spline.getTangent(1, 0 + EPSILON, v2);
        assertEquals(v1[0], v2[0], EPSILON);
        assertEquals(v1[1], v2[1], EPSILON);
    }

    @Test
    public void testExtremValues1() {
        HermiteSpline spline = new HermiteSpline();
        spline = spline.setPosition(0, 0, 0);
        spline = spline.setTangent(0, 1, 1);
        spline = spline.setPosition(1, 0, 0);
        spline = spline.setTangent(1, 1, 0);

        double[] v = new double[2];
        spline.getExtremValues(v);
        assertEquals(0, v[0], EPSILON);
        assertEquals(0.148148, v[1], EPSILON);
    }

    @Test
    public void testExtremValues2() {
        HermiteSpline spline = new HermiteSpline();
        spline = spline.setPosition(0, 0, 0);
        spline = spline.setTangent(0, 1, 1);
        spline = spline.setPosition(1, 0, 1);
        spline = spline.setTangent(1, 1, 1);

        double[] v = new double[2];
        spline.getExtremValues(v);
        assertEquals(0, v[0], EPSILON);
        assertEquals(1, v[1], EPSILON);
    }

    @Test
    public void testExtremValues3() {
        HermiteSpline spline = new HermiteSpline();
        spline = spline.setPosition(0, 0, 0);
        spline = spline.setTangent(0, 1, 0);
        spline = spline.setPosition(1, 0, 0);
        spline = spline.setTangent(1, 1, 0);

        double[] v = new double[2];
        spline.getExtremValues(v);
        assertEquals(0, v[0], EPSILON);
        assertEquals(0, v[1], EPSILON);
    }

    @Test
    public void testExtremValues4() {
        HermiteSpline spline = new HermiteSpline();
        spline = spline.insertPoint(0.5, 1);

        spline = spline.setPosition(0, 0, 0);
        spline = spline.setTangent(0, 0.9999974, 0.002293415);

        spline = spline.setPosition(1, 0.790099, 0.87162215);
        spline = spline.setTangent(1, 0.25862634, 0.96597743);

        spline = spline.setPosition(2, 1, 0);
        spline = spline.setTangent(2, 0.09647936, -0.995335);

        double[] v = new double[2];
        spline.getExtremValues(v);
        assertEquals(0.0, v[0], 0.01);
        assertTrue(v[1] < 1);
    }

}
