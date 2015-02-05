package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Quat4f;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation.ITransformer;

@SuppressWarnings("serial")
public class ScaleManipulator extends TransformManipulator<Vector3d> {

    private class ScaleTransformer implements ITransformer<Vector3d> {
        @Override
        public Vector3d get(Node n) {
            return n.getScale();
        }

        @Override
        public void set(Node n, Vector3d value) {
            n.setScale(value);
        }
    }

    private ScaleAxisManipulator xManipulator;
    private ScaleAxisManipulator yManipulator;

    private ScaleTransformer transformer = new ScaleTransformer();

    public ScaleManipulator() {
        xManipulator = new ScaleAxisManipulator(this,  new float[] {1, 0, 0, 1});
        yManipulator = new ScaleAxisManipulator(this,  new float[] {0, 1, 0, 1});
        addChild(xManipulator);
        addChild(yManipulator);
    }

    @Override
    protected void selectionChanged() {
        super.selectionChanged();

        // align with selected objects
        List<Node> sel = getSelection();
        Quat4d rotation = new Quat4d();
        if (sel.size() > 0)
        {
            rotation = sel.get(0).getRotation();
            if (sel.size() > 1)
            {
                // blend in the rest
                double interp = 1.0f / (sel.size());
                for (int i=1;i<sel.size();i++)
                    rotation.interpolate(sel.get(i).getRotation(), interp);
            }
            xManipulator.setRotation(rotation);
            Quat4d yrot = new Quat4d(0, 0, 0.707, 0.707);
            yrot.mul(rotation);
            yManipulator.setRotation(yrot);
        }
    }

    @Override
    protected String getOperation() {
        return "Scale";
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
        this.transformChanged();
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
    protected void applyTransform(List<Node> selection, List<Vector3d> originalLocalTransforms) {
        double distX = this.xManipulator.getDistance();
        double distY = this.yManipulator.getDistance();
        double factor = 0.025;
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node instance = selection.get(i);
            Vector3d scale = originalLocalTransforms.get(i);
            Vector3d newScale = new Vector3d(
                    Math.abs(scale.x + factor * distX),
                    Math.abs(scale.y + factor * distY),
                    scale.z
            );
            instance.setScale(newScale);
       }
    }

    @Override
    protected ITransformer<Vector3d> getTransformer() {
        return this.transformer;
    }

}
