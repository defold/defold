package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class ScaleManipulator extends TransformManipulator {

    private ScaleAxisManipulator scaleManipulator;

    public ScaleManipulator() {
        scaleManipulator = new ScaleAxisManipulator(this, new float[] {1, 0, 0, 1});
        addChild(scaleManipulator);
    }

    @Override
    protected String getOperation() {
        return "Scale";
    }

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (!(object instanceof Node) || !((Node) object).isFlagSet(Node.Flags.SCALABLE)) {
                return false;
            }
        }
        return true;
    }

    private double getScale(Matrix4d transform, Matrix3d tmpRotationScale) {
        transform.getRotationScale(tmpRotationScale);
        return tmpRotationScale.getScale();
    }

    @Override
    protected void applyTransform(List<Node> selection, List<Matrix4d> originalLocalTransforms) {
        Matrix3d tmp = new Matrix3d();
        double distance = this.scaleManipulator.getDistance();
        double factor = 0.05;
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node instance = selection.get(i);
            double scale = getScale(originalLocalTransforms.get(i), tmp);
            scale += factor * distance;
            scale = Math.abs(scale);
            instance.setScale(scale);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        transformChanged();
    }

}
