package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.SetPropertiesOperation;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation.ITransformer;

@SuppressWarnings("serial")
public class RotateManipulator extends TransformManipulator<Quat4d> {

    private class RotateTransformer implements ITransformer<Quat4d> {
        @Override
        public Quat4d get(Node n) {
            return n.getRotation();
        }

        @Override
        public void set(Node n, Quat4d value) {
            if(n.isOverridable())
            {
                IPropertyAccessor<Object, ISceneModel> accessor = getNodePropertyAccessor(n);
                boolean overridden = accessor.isOverridden(n, "rotation", n.getModel());
                if(overridden) {
                    n.setRotation(value);
                } else {
                    SetPropertiesOperation<Object, ISceneModel> op = new SetPropertiesOperation<Object, ISceneModel>(n, "rotation", accessor, RotateManipulator.this.originalRotation, value, overridden, n.getModel());
                    n.getModel().executeOperation(op);
                }
            } else {
                n.setRotation(value);
            }
        }

    }

    private CircleManipulator xCircleManipulator;
    private CircleManipulator yCircleManipulator;
    private CircleManipulator zCircleManipulator;
    private Quat4d originalRotation = new Quat4d();
    private RotateTransformer transformer = new RotateTransformer();

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
    protected String getOperation() {
        return "Rotate";
    }

    private void getRotation(Matrix4d transform, Matrix3d tmpRotationScale, Quat4d rotation) {
        transform.getRotationScale(tmpRotationScale);
        tmpRotationScale.normalize();
        rotation.set(tmpRotationScale);
    }

    @Override
    protected void applyTransform(List<Node> selection, List<Quat4d> originalLocalTransforms) {
        Quat4d delta = getRotation();
        delta.mulInverse(this.originalRotation);
        Matrix4d world = new Matrix4d();
        Matrix3d tmp = new Matrix3d();
        Quat4d rotation = new Quat4d();
        Quat4d localDelta = new Quat4d();
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node node = selection.get(i);
            Node parent = node.getParent();
            localDelta.set(delta);
            if (parent != null) {
                parent.getWorldTransform(world);
                getRotation(world, tmp, rotation);
                localDelta.mul(rotation);
                rotation.conjugate();
                localDelta.mul(rotation, localDelta);
            }
            rotation.set(originalLocalTransforms.get(i));
            rotation.mul(localDelta, rotation);
            node.applyRotation(rotation);
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.originalRotation = getRotation();
    }

    @Override
    protected ITransformer<Quat4d> getTransformer() {
        return this.transformer;
    }

}
