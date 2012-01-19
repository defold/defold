package com.dynamo.cr.contenteditor.commands;
import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.commands.AbstractHandler;

import com.dynamo.cr.contenteditor.editors.Camera;
import com.dynamo.cr.scene.math.MathUtil;

public abstract class BaseAddHandler extends AbstractHandler {
    protected static Vector4d getPlacementPosition(Camera camera) {
        Vector4d pos;
        if (camera.getType() == Camera.Type.ORTHOGRAPHIC) {
            Matrix4d m = new Matrix4d();
            camera.getViewMatrix(m);
            m.invert();
            Vector4d z = new Vector4d();
            m.getColumn(2, z);
            pos = camera.getPosition();
            double d = MathUtil.dot(z, pos);
            z.scale(-d);
            pos.add(z);
        } else {
            Matrix4d m = new Matrix4d();
            camera.getViewMatrix(m);
            m.invert();
            // NOTE: -20 should perhaps be configurable?
            pos = new Vector4d(0, 0, -20, 1);
            m.transform(pos);
        }
        pos.x = Math.round(pos.x);
        pos.y = Math.round(pos.y);
        pos.z = Math.round(pos.z);
        return pos;
    }

}
