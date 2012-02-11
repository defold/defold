package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;

public class RotateManipulator extends RootManipulator {

    private CircleManipulator xCircleManipulator;
    private CircleManipulator yCircleManipulator;
    private CircleManipulator zCircleManipulator;
    private List<Matrix4d> originalLocalTransforms = new ArrayList<Matrix4d>();
    private List<Matrix4d> newLocalTransforms = new ArrayList<Matrix4d>();
    private boolean transformChanged = false;
    private Quat4d originalRotation = new Quat4d();

    public RotateManipulator() {
        xCircleManipulator = new CircleManipulator(this, new float[] {1, 0, 0, 1});
        yCircleManipulator = new CircleManipulator(this, new float[] {0, 1, 0, 1});
        zCircleManipulator = new CircleManipulator(this, new float[] {0, 0, 1, 1});

        yCircleManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));
        zCircleManipulator.setRotation(new Quat4d(-0.5, -0.5, -0.5, 0.5));

        addChild(xCircleManipulator);
        addChild(yCircleManipulator);
        addChild(zCircleManipulator);
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

        Matrix4d transform = new Matrix4d();
        getWorldTransform(transform);
        Quat4d delta = getRotation();
        delta.mulInverse(this.originalRotation);
        Quat4d rotation = new Quat4d();
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            this.originalLocalTransforms.get(i).get(rotation);
            rotation.mul(delta);
            selection.get(i).setRotation(rotation);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    protected void selectionChanged() {
        List<Node> sel = getSelection();
        Vector4d center = new Vector4d();
        this.originalLocalTransforms.clear();
        for (Node node : sel) {
            Matrix4d transform = new Matrix4d();
            node.getLocalTransform(transform);
            this.originalLocalTransforms.add(transform);

            transform = new Matrix4d();
            node.getWorldTransform(transform);
            Vector4d translation = new Vector4d();
            transform.getColumn(3, translation);
            center.add(translation);
            this.translation.set(translation.getX(), translation.getY(), translation.getZ());
        }

        Matrix4d transform = new Matrix4d();
        sel.get(0).getLocalTransform(transform);
        Vector4d translation = new Vector4d();
        Quat4d rotation = new Quat4d();
        transform.getColumn(3, translation);
        rotation.set(transform);

        setTranslation(new Point3d(translation.getX(), translation.getY(), translation.getZ()));
        setRotation(rotation);
    }

    @Override
    public void refresh() {
        selectionChanged();
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.originalRotation = getRotation();
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
