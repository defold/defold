package com.dynamo.cr.parted.curve;

import java.util.ArrayList;
import java.util.List;

/**
 * Segmented Hermite spline representation
 * Given N control points N-1 Hermite spline segments are projected to
 * the range [xa, xb]
 * where xa and xb is in the value domain [0,1]
 * In order to keep derivative consistent(*) between segments the tangent is scaled by
 * dx where dx is defined as [xb - xa]
 *
 *
 * (*)
 * The mapping can be expressed parametrically as (x(t), y(t))
 *
 * where x(t) is the mapping function
 * and y(t) the Hermite spline function
 *
 * dy   y'(t)
 * -- = ----
 * dx   x'(t)
 *
 * if x(t) is a linear transformation: x(t) = at + c
 *
 * =>
 *
 * dy   y'(t)    y'(t)
 * -- = ----  =  ----
 * dx   x'(t)      a
 *
 * Hence, scaling every tangent with a yields consistent derivatives and C1 continuity
 * between segments
 *
 * @author chmu
 *
 */
public class HermiteSpline {

    static final double MAX_TANGENT_ANGLE = Math.PI / 2 - 0.0001;

    private List<SplinePoint> points = new ArrayList<SplinePoint>(32);

    public HermiteSpline() {
        points.add(new SplinePoint(0, 0, 0.5, 0.5));
        points.add(new SplinePoint(1, 1, 0.5, 0.5));
    }

    private static double hermite(double x0, double x1, double t0, double t1, double t) {
        return (2 * t * t * t - 3 * t * t + 1) * x0 +
               (t * t * t - 2 * t * t + t) * t0 +
               (- 2 * t * t * t + 3 * t * t) * x1 +
               (t * t * t - t * t) * t1;
    }

    private static double hermiteD(double x0, double x1, double t0, double t1, double t) {
        return (6 * t * t - 6 * t) * x0 +
               (3 * t * t - 4 * t + 1) * t0 +
               (- 6 * t * t + 6 * t ) * x1 +
               (3 * t * t - 2 * t) * t1;
    }

    public int getCount() {
        return points.size();
    }

    public SplinePoint getPoint(int i) {
        return new SplinePoint(points.get(i));
    }

    public void getValue(int segment, double t, double[] value) {
        SplinePoint p0 = getPoint(segment);
        SplinePoint p1 = getPoint(segment + 1);
        double segT = t;
        double dx = p1.x - p0.x;

        double py0 = p0.y;
        double py1 = p1.y;
        double pt0 = dx * p0.ty / p0.tx;
        double pt1 = dx * p1.ty / p1.tx;

        double x = p0.x * (1 - segT) + p1.x * segT;
        double y = hermite(py0, py1, pt0, pt1, segT);

        value[0] = x;
        value[1] = y;
    }

    public double getY(double x) {
        int segmentCount = getSegmentCount();
        int segment = 0;
        double t = 0;
        for (int s = 0; s < segmentCount; ++s) {
            SplinePoint p0 = getPoint(s);
            SplinePoint p1 = getPoint(s + 1);
            if (x >= p0.x && x < p1.x) {
                t = (x - p0.x) / (p1.x - p0.x);
                segment = s;
                break;
            }
        }

        double[] value = new double[2];
        getValue(segment, t, value);
        return value[1];
    }

    public void getTangent(int segment, double t, double[] value) {
        SplinePoint p0 = getPoint(segment);
        SplinePoint p1 = getPoint(segment + 1);
        double segT = t;
        double dx = p1.x - p0.x;

        double py0 = p0.y;
        double py1 = p1.y;
        double pt0 = dx * p0.ty / p0.tx;
        double pt1 = dx * p1.ty / p1.tx;

        double d = hermiteD(py0, py1, pt0, pt1, segT) / dx;
        double x = 1;
        double y = d;

        double l = Math.sqrt(x * x + y * y);
        value[0] = x / l;
        value[1] = y / l;
    }

    public void setPosition(int i, double newX, double newY) {
        SplinePoint p = this.points.get(i);

        if (i == 0) {
            newX = 0;
        } else if (i == getCount() - 1) {
            newX = 1;
        }

        p.x = newX;
        p.y = newY;

        double minMargin = 0.01;
        if (i > 0) {
            p.x = Math.max(getPoint(i - 1).x + minMargin, p.x);
        }
        if (i < getCount() - 1) {
            p.x = Math.min(getPoint(i + 1).x - minMargin, p.x);
        }

    }

    public void setTangent(int i, double tx, double ty) {
        SplinePoint p = this.points.get(i);
        tx = Math.max(tx, 0);
        double l = Math.sqrt((tx * tx + ty * ty));
        p.tx = tx / l;
        p.ty = ty / l;
    }

    public int getSegmentCount() {
        return getCount() - 1;
    }

    public int insertPoint(double x, double y) {
        int segmentCount = getSegmentCount();
        for (int s = 0; s < segmentCount; ++s) {
            SplinePoint p0 = getPoint(s);
            SplinePoint p1 = getPoint(s + 1);
            if (x >= p0.x && x < p1.x) {
                double[] pos = new double[2];
                double[] tan = new double[2];
                double t = (x - p0.x) / (p1.x - p0.x);
                getValue(s, t, pos);
                getTangent(s, t, tan);
                this.points.add(s + 1, new SplinePoint(pos[0], pos[1], tan[0], tan[1]));
                return s + 1;
            }
        }
        return -1;
    }

    public void removePoint(int index) {
        if (index > 0 && index < getCount() - 1) {
            this.points.remove(index);
        }
    }

    public void clear() {
        this.points.clear();
    }
}
