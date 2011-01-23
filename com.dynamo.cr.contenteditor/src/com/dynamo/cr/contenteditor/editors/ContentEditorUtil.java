package com.dynamo.cr.contenteditor.editors;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.contenteditor.math.MathUtil;

public class ContentEditorUtil
{
    public static IContainer findProjectRoot(IFile file)
    {
        IContainer c = file.getParent();
        while (c != null)
        {
            if (c.exists(new Path("game.project")))
            {
                return c;
            }
            c = c.getParent();
        }

        return null;
    }

    public static void viewToWorld(Camera camera, int x, int y, Vector4d world_point, Vector4d world_vector)
    {
        Vector4d click_pos = camera.unProject(x, y, 0);
        Vector4d click_dir = new Vector4d();

        if (camera.getType() == Camera.Type.ORTHOGRAPHIC)
        {
            /*
             * NOTES:
             * We cancel out the z-component in world_point below.
             * The convention is that the unproject z-value for orthographic projection should
             * be 0.0.
             * Pity that the orthographic case is an exception. Solution?
             */
            Matrix4d view = new Matrix4d();
            camera.getViewMatrix(view);

            Vector4d view_axis = new Vector4d();
            view.getColumn(2, view_axis);
            click_dir.set(view_axis);

            double projected_length = MathUtil.dot(click_pos, view_axis);
            view_axis.scale(projected_length);
            click_pos.sub(view_axis);
        }
        else
        {
            click_dir.sub(click_pos, camera.getPosition());
            click_dir.normalize();
        }

        world_point.set(click_pos);
        world_vector.set(click_dir);
    }
}
