package com.dynamo.cr.sceneed.ui;

import com.dynamo.cr.sceneed.core.Manipulator;

public class AxisManipulator extends Manipulator {

    private Manipulator rootManipulator;
    private float[] color;

    public AxisManipulator(Manipulator rootManipulator, float[] color) {
        this.rootManipulator = rootManipulator;
        this.color = color;
    }

    @Override
    public boolean match(Object[] selection) {
        return false;
    }

    public float[] getColor() {
        return color;
    }

}
