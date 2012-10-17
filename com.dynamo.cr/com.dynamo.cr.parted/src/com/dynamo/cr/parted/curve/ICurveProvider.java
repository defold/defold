package com.dynamo.cr.parted.curve;

import org.eclipse.swt.graphics.Color;

public interface ICurveProvider {
    public HermiteSpline getSpline(int i);
    public void setSpline(HermiteSpline spline, int i, boolean intermediate);
    public boolean isEnabled(int i);
    public Color getColor(int i);
}
