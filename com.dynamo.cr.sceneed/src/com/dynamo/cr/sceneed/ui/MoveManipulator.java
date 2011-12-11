package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;

public class MoveManipulator extends RootManipulator {

    private AxisManipulator xAxisManipulator;
    private AxisManipulator yAxisManipulator;
    private AxisManipulator zAxisManipulator;
    private List<Matrix4d> originalLocalTransforms = new ArrayList<Matrix4d>();
    private List<Matrix4d> newLocalTransforms = new ArrayList<Matrix4d>();
    private boolean transformChanged = false;

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
            if (object instanceof Node && ((Node) object).isFlagSet(Node.Flags.TRANSFORMABLE)) {
                return true;
            }
        }
        return false;
    }

    @Override
    protected void transformChanged() {
        transformChanged = true;
        List<Node> selection = getSelection();

        Matrix4d transform = new Matrix4d();
        getWorldTransform(transform);
        for (Node node : selection) {
            node.setWorldTransform(transform);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    protected void selectionChanged() {
        List<Node> sel = getSelection();
        Vector4d center = new Vector4d();
        for (Node node : sel) {
            Matrix4d transform = new Matrix4d();
            node.getLocalTransform(transform);
            originalLocalTransforms.add(transform);

            transform = new Matrix4d();
            node.getWorldTransform(transform);
            transform.getColumn(3, translation);
            center.add(translation);
        }

        center.scale(1.0 / sel.size());
        center.setW(0);
        setTranslation(center);
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
