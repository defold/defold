package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class MoveManipulator extends TransformManipulator {

    private AxisManipulator xAxisManipulator;
    private AxisManipulator yAxisManipulator;
    private AxisManipulator zAxisManipulator;
    private Point3d originalTranslation = new Point3d();

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
    protected String getOperation() {
        return "Move";
    }

    @Override
    protected void applyTransform(List<Node> selection, List<Matrix4d> originalLocalTransforms) {
        Vector3d delta = new Vector3d(getTranslation());
        delta.sub(this.originalTranslation);
        Vector3d translation = new Vector3d();
        Point3d pTranslation = new Point3d();
        Matrix4d invWorld = new Matrix4d();
        Vector3d localDelta = new Vector3d();
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node node = selection.get(i);
            Node parent = node.getParent();
            localDelta.set(delta);
            if (parent != null) {
                parent.getWorldTransform(invWorld);
                invWorld.invert();
                invWorld.transform(localDelta);
            }
            originalLocalTransforms.get(i).get(translation);
            translation.add(localDelta);
            pTranslation.set(translation);
            selection.get(i).setTranslation(pTranslation);
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.originalTranslation = getTranslation();
        this.transformChanged = false;
    }


}
