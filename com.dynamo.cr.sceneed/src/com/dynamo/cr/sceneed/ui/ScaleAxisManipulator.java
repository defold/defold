package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;

public class ScaleAxisManipulator extends Manipulator {

    private RootManipulator rootManipulator;
    private float[] color;
    private Vector4d startPoint;
    private boolean moving = false;
    private double distance = 1;

    public ScaleAxisManipulator(RootManipulator rootManipulator, float[] color) {
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

    public double getDistance() {
        return distance;
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (getController().isManipulatorSelected(this)) {
            this.startPoint = ManipulatorUtil.closestPoint(this, getController().getRenderView(), e);
            moving = true;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        moving = false;
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (moving) {
            Vector4d closestPoint = ManipulatorUtil.closestPoint(this, getController().getRenderView(), e);
            Vector4d delta = new Vector4d();
            delta.sub(closestPoint, startPoint);

            Vector4d axis = ManipulatorUtil.transform(this, 1, 0, 0);
            double dir = Math.signum(axis.dot(delta));
            distance = dir * delta.length();

            rootManipulator.manipulatorChanged(this);
        }
    }

}
