package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.Node;

public class ManipulatorRendererUtil {
    public static final double BASE_LENGTH = 110;
    public static final double BASE_THICKNESS = 1.5;
    public static final double BASE_HEAD_RADIUS = 6.5;
    public static final double BASE_DIM = 25;

    public static double getScaleFactor(Node node,
            IRenderView renderView) {

        Matrix4d world = new Matrix4d();
        node.getWorldTransform(world);
        Vector4d position = new Vector4d();
        world.getColumn(3, position);

        Matrix4d viewTransform = new Matrix4d();
        viewTransform = renderView.getViewTransform();
        viewTransform.invert();
        Vector4d xAxis = new Vector4d();
        viewTransform.getColumn(0, xAxis);

        Vector4d p2 = new Vector4d(position);
        p2.add(xAxis);
        double[] cp1 = renderView.worldToView(new Point3d(position.x, position.y, position.z));
        double[] cp2 = renderView.worldToView(new Point3d(p2.x, p2.y, p2.z));
        double factor = Math.abs(cp1[0] - cp2[0]);
        return factor;
    }

}
