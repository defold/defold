package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;

@SuppressWarnings("serial")
public abstract class TransformManipulator extends RootManipulator {

    private boolean transformChanged = false;
    private List<Matrix4d> originalLocalTransforms = new ArrayList<Matrix4d>();
    private List<Matrix4d> newLocalTransforms = new ArrayList<Matrix4d>();

    protected abstract String getOperation();
    protected abstract void applyTransform(List<Node> selection, List<Matrix4d> originalLocalTransforms);

    @Override
    protected final void transformChanged() {
        this.transformChanged = true;
        applyTransform(getSelection(), this.originalLocalTransforms);
    }

    @Override
    protected void selectionChanged() {
        List<Node> sel = getSelection();
        this.originalLocalTransforms.clear();
        for (Node node : sel) {
            Matrix4d transform = new Matrix4d();
            node.getLocalTransform(transform);
            this.originalLocalTransforms.add(transform);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    public void refresh() {
        List<Node> sel = getSelection();
        if (!sel.isEmpty()) {
            Node n = sel.get(0);
            Matrix4d world = new Matrix4d();
            n.getWorldTransform(world);
            Vector3d translation = new Vector3d();
            world.get(translation);
            this.translation.set(translation);
        }
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
    public void mouseUp(MouseEvent e) {
        if (transformChanged) {
            for (Node node : getSelection()) {
                Matrix4d transform = new Matrix4d();
                node.getLocalTransform(transform);
                newLocalTransforms.add(transform);
            }

            TransformNodeOperation operation = new TransformNodeOperation(getOperation(), getSelection(), originalLocalTransforms, newLocalTransforms);
            getController().executeOperation(operation);
            transformChanged = false;
        }
    }
}
