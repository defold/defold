package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Quat4d;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;


public class MoveManipulator extends Manipulator {

    private AxisManipulator xAxisManipulator;
    private AxisManipulator yAxisManipulator;
    private AxisManipulator zAxisManipulator;

    public MoveManipulator() {
        xAxisManipulator = new AxisManipulator(this, new float[] {1, 0, 0, 1});
        yAxisManipulator = new AxisManipulator(this, new float[] {0, 1, 0, 1});
        zAxisManipulator = new AxisManipulator(this, new float[] {0, 0, 1, 1});

        yAxisManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));
        zAxisManipulator.setRotation(new Quat4d(-0.5, -0.5, -0.5, 0.5));

        addChild(xAxisManipulator);
        addChild(yAxisManipulator);
        addChild(zAxisManipulator);
    }

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (object instanceof Node) {
                return true;
            }
        }
        return false;
    }

}
