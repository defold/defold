package com.defold.editor.pipeline;

public class BezierUtil {
    public static double curve(double t, double v0, double v1, double v2, double v3) {
        double u = 1.0 - t;
        return u * u * u * v0 + 3 * u * u * t * v1 + 3 * u * t * t * v2 + t * t * t * v3;
    }

    public static double findT(double targetV, double v0, double v1, double v2, double v3) {
        if (v3 <= v0 || v1 < v0 || v1 > v3 || v2 < v0 || v2 > v3) {
            throw new IllegalArgumentException("The curve must be monotonically increasing.");
        }
        if (targetV < v0) {
            return v0;
        }
        if (targetV > v3) {
            return v3;
        }
        double min = 0.0;
        double max = 1.0;
        double t = (targetV - v0) / (v3 - v0);
        double v = curve(t, v0, v1, v2, v3);
        double diff = v - targetV;
        while (diff * diff > 0.000001) {
            if (diff < 0.0) {
                min = t;
                t += (max - t) * 0.5;
            } else {
                max = t;
                t -= 0.5 * (t - min);
            }
            v = curve(t, v0, v1, v2, v3);
            diff = v - targetV;
        }
        return t;
    }

}
