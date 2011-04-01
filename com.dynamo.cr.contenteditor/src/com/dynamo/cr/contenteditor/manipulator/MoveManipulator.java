package com.dynamo.cr.contenteditor.manipulator;

import javax.media.opengl.GL;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.math.MathUtil;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.GLUtil;

// http://caad.arch.ethz.ch/info/maya/manual/UserGuide/Overview/TransformingObjects.fm.html
public class MoveManipulator extends TransformManipulator {
    private Vector4d startPoint = new Vector4d();
    private double snapValue = 0.5f;

    private final static double SNAP_LIMIT = 0.25;

    protected static Vector4d closestPoint(IEditor editor, int x, int y, Vector4d pos, Vector4d axis) {
        Vector4d click_pos = new Vector4d();
        Vector4d click_dir = new Vector4d();
        editor.viewToWorld(x, y, click_pos, click_dir);

        return MathUtil.projectLineToLine(pos, axis, click_pos, click_dir);
    }

    @Override
    public void draw(ManipulatorDrawContext context)
    {
        super.draw(context);

        GL gl = context.gl;
        double factor = ManipulatorUtil.getHandleScaleFactor(getHandlePosition(context.nodes), context.editor);
        for (int i = 0; i < 3; ++i) {
            gl.glColor3fv(Constants.AXIS_COLOR[i], 0);
            gl.glPushMatrix();
            GLUtil.multMatrix(gl, this.manipulatorTransformWS);
            GLUtil.multMatrix(gl, this.handleTransforms[i]);

            context.beginDrawHandle();
            GLUtil.drawArrow(gl, 120 / factor, 20 / factor, 1 / factor, 5 / factor);
            context.endDrawHandle();

            gl.glPopMatrix();
        }
    }

    @Override
    public void mouseDown(ManipulatorContext context) {
        super.mouseDown(context);

        Vector4d pos = getHandlePosition(context.nodes);
        Vector4d handleAxisWS = new Vector4d(this.handleAxisMS);
        this.originalManipulatorTransformWS.transform(handleAxisWS);
        Vector4d closest = closestPoint(context.editor, context.mouseX, context.mouseY, pos, handleAxisWS);
        this.startPoint.set(closest);
    }

    @Override
    public void mouseMove(ManipulatorContext context)
    {
        Vector4d handleAxisWS = new Vector4d(this.handleAxisMS);
        this.originalManipulatorTransformWS.transform(handleAxisWS);
        Vector4d delta = closestPoint(context.editor, context.mouseX, context.mouseY, this.startPoint, handleAxisWS);
        delta.sub(this.startPoint);
        if (context.snapActive && this.snapValue > 0.0f) {
            double length = delta.dot(handleAxisWS);
            double t = length / this.snapValue;
            double t0 = Math.round(t);
            if (Math.abs(t - t0) < SNAP_LIMIT) {
                t = t0;
            }
            delta.scale(t, handleAxisWS);
        }
        Vector4d position = new Vector4d();
        this.originalManipulatorTransformWS.getColumn(3, position);
        position.add(delta);
        this.manipulatorTransformWS.setColumn(3, position);

        super.mouseMove(context);
    }
}
