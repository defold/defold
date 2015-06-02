package com.dynamo.cr.sceneed.ui;

import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.SetPropertiesOperation;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation.ITransformer;

@SuppressWarnings("serial")
public class MoveManipulator extends TransformManipulator<Point3d> {

    private class MoveTransformer implements ITransformer<Point3d> {

        @Override
        public Point3d get(Node n) {
            return n.getTranslation();
        }

        @Override
        public void set(Node n, Point3d value) {
            if(n.isOverridable())
            {
                IPropertyAccessor<Object, ISceneModel> accessor = getNodePropertyAccessor(n);
                boolean overridden = accessor.isOverridden(n, "translation", n.getModel());
                if(overridden) {
                    n.setTranslation(value);
                } else {
                    SetPropertiesOperation<Object, ISceneModel> op = new SetPropertiesOperation<Object, ISceneModel>(n, "translation", accessor, MoveManipulator.this.originalTranslation, value, overridden, n.getModel());
                    n.getModel().executeOperation(op);
                }
            } else {
                n.setTranslation(value);
            }
        }
    }

    private AxisManipulator xAxisManipulator;
    private AxisManipulator yAxisManipulator;
    private AxisManipulator zAxisManipulator;
    private ScreenPlaneManipulator screenPlaneManipulator;
    private Point3d originalTranslation = new Point3d();
    private MoveTransformer transformer = new MoveTransformer();

    public MoveManipulator() {
        xAxisManipulator = new AxisManipulator(this, new float[] {1, 0, 0, 1});
        yAxisManipulator = new AxisManipulator(this, new float[] {0, 1, 0, 1});
        yAxisManipulator.setRotation(new Quat4d(0.5, 0.5, 0.5, 0.5));
        zAxisManipulator = new AxisManipulator(this, new float[] {0, 0, 1, 1});
        zAxisManipulator.setRotation(new Quat4d(-0.5, -0.5, -0.5, 0.5));

        screenPlaneManipulator = new ScreenPlaneManipulator(this, new float[] {0, 1, 1, 1});

        addChild(xAxisManipulator);
        addChild(yAxisManipulator);
        addChild(zAxisManipulator);
        addChild(screenPlaneManipulator);
    }

    @Override
    protected String getOperation() {
        return "Move";
    }

    @Override
    protected void applyTransform(List<Node> selection, List<Point3d> originalLocalTransforms) {
        Vector3d delta = new Vector3d(getTranslation());
        delta.sub(this.originalTranslation);
        Vector3d translation = new Vector3d();
        Point3d pTranslation = new Point3d();
        Matrix4d invWorld = new Matrix4d();
        Vector3d localDelta = new Vector3d();
        int n = selection.size();
        for (int i = 0; i < n; ++i) {
            Node node = selection.get(i);
            Node parent = node.getParent();
            localDelta.set(delta);
            if (parent != null) {
                parent.getWorldTransform(invWorld);
                invWorld.invert();
                invWorld.transform(localDelta);
            }
            originalLocalTransforms.get(i).get(translation);
            translation.add(localDelta);
            pTranslation.set(translation);
            selection.get(i).applyTranslation(pTranslation);
        }
    }

    @Override
    public void mouseDown(MouseEvent e) {
        this.originalTranslation = getTranslation();
    }

    @Override
    protected ITransformer<Point3d> getTransformer() {
        return this.transformer;
    }

}
