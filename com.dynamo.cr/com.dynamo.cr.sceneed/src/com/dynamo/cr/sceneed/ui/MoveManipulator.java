package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;

@SuppressWarnings("serial")
public class MoveManipulator extends RootManipulator {

    private AxisManipulator xAxisManipulator;
    private AxisManipulator yAxisManipulator;
    private AxisManipulator zAxisManipulator;
    private List<Matrix4d> originalLocalTransforms = new ArrayList<Matrix4d>();
    private List<Matrix4d> newLocalTransforms = new ArrayList<Matrix4d>();
    private boolean transformChanged = false;
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
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (!(object instanceof Node) || !((Node) object).isFlagSet(Node.Flags.TRANSFORMABLE)) {
                return false;
            }
        }
        return true;
    }

    @Override
    protected void transformChanged() {
        transformChanged = true;
        List<Node> selection = getSelection();

        Vector3d delta = new Vector3d(getTranslation());
        delta.sub(this.originalTranslation);
        Vector3d translation = new Vector3d();
        Point3d pTranslation = new Point3d();
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            this.originalLocalTransforms.get(i).get(translation);
            translation.add(delta);
            pTranslation.set(translation);
            selection.get(i).setTranslation(pTranslation);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    protected void selectionChanged() {
        List<Node> sel = getSelection();
        Point3d center = new Point3d();
        this.originalLocalTransforms.clear();
        for (Node node : sel) {
            Matrix4d transform = new Matrix4d();
            node.getLocalTransform(transform);
            this.originalLocalTransforms.add(transform);

            transform = new Matrix4d();
            node.getWorldTransform(transform);
            Vector3d translation = new Vector3d();
            transform.get(translation);
            center.add(translation);
        }

        center.scale(1.0 / sel.size());
        this.translation.set(center);
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.originalTranslation = getTranslation();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (transformChanged) {
            for (Node node : getSelection()) {
                Matrix4d transform = new Matrix4d();
                node.getLocalTransform(transform);
                newLocalTransforms.add(transform);
            }

            TransformNodeOperation operation = new TransformNodeOperation("Move", getSelection(), originalLocalTransforms, newLocalTransforms);
            getController().executeOperation(operation);
        }
    }

}
