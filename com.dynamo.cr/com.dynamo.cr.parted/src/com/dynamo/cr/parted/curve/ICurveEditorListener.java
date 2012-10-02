package com.dynamo.cr.parted.curve;

public interface ICurveEditorListener {
    public void splineChanged(String label, CurveEditor editor, HermiteSpline oldSpline, HermiteSpline newSpline);
}
