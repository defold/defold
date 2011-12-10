package com.dynamo.cr.sceneed.ui;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;

public class CircleManipulator extends Manipulator {

    private RootManipulator rootManipulator;
    private float[] color;
    private boolean rotating = false;
    private int startX;
    private double angle;

    public CircleManipulator(RootManipulator rootManipulator, float[] color) {
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

    @Override
    public void mouseDown(MouseEvent e) {
        if (getController().isManipulatorSelected(this)) {
            startX = e.x;
            rotating = true;
        }
    }

    @Override
    public void mouseUp(MouseEvent e) {
        rotating = false;
    }

    @Override
    public void mouseMove(MouseEvent e) {
        if (rotating) {
            int dx = e.x - startX;
            double mouseAngle = dx * 0.02;
            this.angle = mouseAngle;

            Matrix4d transform = new Matrix4d();
            getWorldTransform(transform);
            Vector4d axis = new Vector4d(1, 0, 0, 0);
            transform.transform(axis);

            AxisAngle4d aa = new AxisAngle4d(axis.getX(), axis.getY(), axis.getZ(), this.angle);
            Quat4d quat = new Quat4d();
            quat.set(aa);
            rootManipulator.setRotation(quat);
            rootManipulator.transformChanged();
        }
    }

}
