package com.dynamo.cr.parted.curve;

public interface ICurveProvider {
    HermiteSpline getSpline(int i);
    void setSpline(HermiteSpline spline, int i, boolean intermediate);
    boolean isEnabled(int i);
}
