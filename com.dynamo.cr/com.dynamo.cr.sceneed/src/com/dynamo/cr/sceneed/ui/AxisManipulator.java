package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;

@SuppressWarnings("serial")
public class AxisManipulator extends Manipulator {

    private RootManipulator rootManipulator;
    private Vector4d startPoint;
    private Vector4d originalTranslation = new Vector4d();
    private boolean moving = false;

    public AxisManipulator(RootManipulator rootManipulator, float[] color) {
        super(color);
        this.rootManipulator = rootManipulator;
    }

    @Override
    public boolean match(Object[] selection) {
        return false;
    }

    @Override
    public void mouseDown(MouseEvent e) {
        if (isEnabled() && getController().isManipulatorSelected(this)) {
            Matrix4d transform = new Matrix4d();
            getWorldTransform(transform);
            transform.getColumn(3, originalTranslation);
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

            Vector4d translation = new Vector4d(originalTranslation);
            translation.add(delta);
            if (ManipulatorUtil.isSnappingActive(e)) {
                translation = ManipulatorUtil.snapToGrid(translation, this.getController().getRenderView());
            }
            rootManipulator.setTranslation(new Point3d(translation.getX(), translation.getY(), translation.getZ()));
            rootManipulator.transformChanged();
        }
    }

}
