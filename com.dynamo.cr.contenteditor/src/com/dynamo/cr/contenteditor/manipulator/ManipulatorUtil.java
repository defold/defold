package com.dynamo.cr.contenteditor.manipulator;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.Camera;
import com.dynamo.cr.contenteditor.editors.IEditor;

public class ManipulatorUtil {

    public static double getHandleScaleFactor(Vector4d handlePosition,
            IEditor editor) {

        Camera camera = editor.getCamera();
        Matrix4d view_matrix = new Matrix4d();
        camera.getViewMatrix(view_matrix);
        view_matrix.invert();
        Vector4d x_axis = new Vector4d();
        view_matrix.getColumn(0, x_axis);

        Vector4d p2 = new Vector4d(handlePosition);
        p2.add(x_axis);
        double[] cp1 = editor.worldToView(new Point3d(handlePosition.x, handlePosition.y, handlePosition.z));
        double[] cp2 = editor.worldToView(new Point3d(p2.x, p2.y, p2.z));
        double factor = Math.abs(cp1[0] - cp2[0]);
        return factor;
    }

}
