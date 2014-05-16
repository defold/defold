package com.dynamo.cr.sceneed.ui;

import java.util.List;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation.ITransformer;

@SuppressWarnings("serial")
public class ScaleManipulator extends TransformManipulator<Double> {

    private class ScaleTransformer implements ITransformer<Double> {
        @Override
        public Double get(Node n) {
            return n.getScale();
        }

        @Override
        public void set(Node n, Double value) {
            n.setScale(value);
        }
    }

    private ScaleAxisManipulator scaleManipulator;
    private ScaleTransformer transformer = new ScaleTransformer();

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

    @Override
    protected void applyTransform(List<Node> selection, List<Double> originalLocalTransforms) {
        double distance = this.scaleManipulator.getDistance();
        double factor = 0.05;
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node instance = selection.get(i);
            double scale = originalLocalTransforms.get(i);
            scale += factor * distance;
            scale = Math.abs(scale);
            instance.setScale(scale);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        transformChanged();
    }

    @Override
    protected ITransformer<Double> getTransformer() {
        return this.transformer;
    }

}
