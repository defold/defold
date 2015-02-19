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
    private ScaleAxisManipulator zManipulator;

    private ScaleTransformer transformer = new ScaleTransformer();

    public ScaleManipulator() {
        xManipulator = new ScaleAxisManipulator(this,  new float[] {1, 0, 0, 1});
        yManipulator = new ScaleAxisManipulator(this,  new float[] {0, 1, 0, 1});
        zManipulator = new ScaleAxisManipulator(this,  new float[] {0, 0, 1, 1});
        addChild(xManipulator);
        addChild(yManipulator);
        addChild(zManipulator);
    }

    private Quat4d quatFromAxises(Vector3d src, Vector3d target)
    {
        double lenprod = src.length() * target.length();
        if (lenprod < 0.000000001)
            return new Quat4d();

        Vector3d axis = new Vector3d();
        axis.cross(src, target);
        double cos = src.dot(target) / lenprod;
        if (cos > 9.99)
            return new Quat4d();
        else
        {
            double angle = Math.acos(cos);
            axis.scale(Math.sin(angle / 2) / axis.length());
            return new Quat4d(axis.x, axis.y, axis.z, Math.cos(angle / 2));
        }

    }

    @Override
    protected void selectionChanged() {
        super.selectionChanged();
        // compute rotation for selected object to give the axises
        // the correct direction.
        List<Node> sel = getSelection();
        Quat4d rotation = new Quat4d();

        // rotation quat from matrix
        Vector4d xAxis = new Vector4d();
        Vector4d yAxis = new Vector4d();
        Vector4d zAxis = new Vector4d();
        Matrix4d transform = new Matrix4d();
        sel.get(0).getWorldTransform(transform);
        transform.getColumn(0, xAxis);
        transform.getColumn(1, yAxis);
        transform.getColumn(2, zAxis);

        xManipulator.setRotation(quatFromAxises(new Vector3d(1,0,0), new Vector3d(xAxis.x, xAxis.y, xAxis.z)));
        yManipulator.setRotation(quatFromAxises(new Vector3d(1,0,0), new Vector3d(yAxis.x, yAxis.y, yAxis.z)));
        zManipulator.setRotation(quatFromAxises(new Vector3d(1,0,0), new Vector3d(zAxis.x, zAxis.y, zAxis.z)));
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
        double distZ = this.zManipulator.getDistance();
        double factor = 0.025;
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node instance = selection.get(i);
            Vector3d scale = originalLocalTransforms.get(i);
            Vector3d newScale = new Vector3d(
                    Math.abs(scale.x + factor * distX),
                    Math.abs(scale.y + factor * distY),
                    Math.abs(scale.z + factor * distZ)
            );
            instance.setScale(newScale);
       }
    }

    @Override
    protected ITransformer<Vector3d> getTransformer() {
        return this.transformer;
    }

}
