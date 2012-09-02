package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.IRenderView;
import com.dynamo.cr.sceneed.core.Node;

public class ManipulatorUtil {

    private static final double CLAMP_LEVEL = 0.2;

    public static Vector4d closestPoint(Node node, IRenderView renderView, MouseEvent e) {
        Vector4d pos = new Vector4d(node.getTranslation());
        pos.setW(1.0);
        Vector4d axis = transform(node, 1, 0, 0);

        Vector4d clickPos = new Vector4d();
        Vector4d clickDir = new Vector4d();
        renderView.viewToWorld(e.x, e.y, clickPos, clickDir);

        return ManipulatorUtil.projectLineToLine(pos, axis, clickPos, clickDir);
    }

    public static Vector4d projectLineToLine(Vector4d p0, Vector4d d0, Vector4d p1, Vector4d d1)
    {
        Vector4d u = d0;
        Vector4d v = d1;
        Vector4d w = new Vector4d();
        w.sub(p0, p1);
        double a = u.dot(u); // always >= 0
        double b = u.dot(v);
        double c = v.dot(v); // always >= 0
        double d = u.dot(w);
        double e = v.dot(w);
        double D = a * c - b * b; // always >= 0
        double sc;

        // compute the line parameters of the two closest points
        if (D < 0.00001)
        { // the lines are almost parallel
            sc = 0.0;
        }
        else
        {
            sc = (b * e - c * d) / D;
        }

        Vector4d closest0 = new Vector4d(d0);
        closest0.scaleAdd(sc, p0);

        return closest0;
    }

    public static Vector4d transform(Node node, double x, double y, double z) {
        Matrix4d transform = new Matrix4d();
        node.getWorldTransform(transform);
        Vector4d axis = new Vector4d(x, y, z, 0);
        transform.transform(axis);
        return axis;
    }

    public static boolean isSnappingActive(MouseEvent event) {
        return (event.stateMask & SWT.SHIFT) != 0;
    }

    public static Vector4d snapToGrid(Vector4d point, IRenderView renderView) {
        double gridSize = renderView.getGrid().getPrimarySize();
        double[] p = new double[4];
        point.get(p);
        for (int i = 0; i < 3; ++i) {
            p[i] /= gridSize;
            double v = Math.abs(p[i] - Math.floor(p[i]));
            if (v < CLAMP_LEVEL || v > 1.0f - CLAMP_LEVEL) {
                p[i] = Math.round(p[i]);
            }
            p[i] *= gridSize;
        }
        return new Vector4d(p);
    }
}
