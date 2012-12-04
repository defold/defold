package com.dynamo.cr.go.core.manipulators;

import java.util.List;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;

import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.ScaleAxisManipulator;
import com.dynamo.cr.sceneed.ui.TransformManipulator;

@SuppressWarnings("serial")
public class UniformScaleManipulator extends TransformManipulator {

    private ScaleAxisManipulator scaleManipulator;

    public UniformScaleManipulator() {
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
            if (!(object instanceof InstanceNode)) {
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
        double factor = 0.002; // * ManipulatorRendererUtil.getScaleFactor(this.scaleManipulator, getController().getRenderView());
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            InstanceNode instance = (InstanceNode)selection.get(i);
            double scale = getScale(originalLocalTransforms.get(i), tmp);
            scale += factor * distance;
            instance.setScale(scale);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        transformChanged();
    }

}
