package com.dynamo.cr.contenteditor.manipulator;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.scene.graph.Node;
import com.dynamo.cr.scene.graph.NodeUtil;
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
        Vector4d position = getHandlePosition(context.nodes, context.pivot);
        double factor = ManipulatorUtil.getHandleScaleFactor(position, context.editor);
        // Draw arrows
        for (int i = 0; i < 3; ++i) {
            if (context.manipulatorHandle == i) {
                gl.glColor3fv(Constants.SELECTED_HANDLE_COLOR, 0);
            } else {
                gl.glColor3fv(Constants.AXIS_COLOR[i], 0);
            }
            gl.glPushMatrix();
            GLUtil.multMatrix(gl, this.manipulatorTransformWS);
            GLUtil.multMatrix(gl, this.handleTransforms[i]);

            context.beginDrawHandle(i);
            GLUtil.drawArrow(gl, 120 / factor, 20 / factor, 1 / factor, 5 / factor);
            context.endDrawHandle();

            gl.glPopMatrix();
        }
        // Draw view-parallel square
        if (context.manipulatorHandle == VIEW_HANDLE_NAME) {
            gl.glColor3fv(Constants.SELECTED_HANDLE_COLOR, 0);
        } else {
            gl.glColor3fv(Constants.DEFAULT_HANDLE_COLOR, 0);
        }
        Matrix4d invView = new Matrix4d();
        context.editor.getCamera().getViewMatrix(invView);
        invView.invert();
        invView.setColumn(3, position);
        gl.glPushMatrix();
        GLUtil.multMatrix(gl, invView);
        Matrix4d scale = new Matrix4d();
        scale.setIdentity();
        scale.mul(15/factor);
        scale.setElement(3, 3, 1.0);
        GLUtil.multMatrix(gl, scale);
        context.beginDrawHandle(VIEW_HANDLE_NAME);
        gl.glBegin(GL.GL_LINE_LOOP);
        gl.glVertex3i(1, 1, 0);
        gl.glVertex3i(1, -1, 0);
        gl.glVertex3i(-1, -1, 0);
        gl.glVertex3i(-1, 1, 0);
        gl.glEnd();
        context.endDrawHandle();
        gl.glPopMatrix();
    }

    @Override
    public void mouseDown(ManipulatorContext context) {
        super.mouseDown(context);
        Vector4d closest;
        Vector4d pos = getHandlePosition(context.nodes, context.pivot);
        switch (context.manipulatorHandle) {
        case VIEW_HANDLE_NAME:
            Matrix4d invView = new Matrix4d();
            context.editor.getCamera().getViewMatrix(invView);
            invView.invert();
            Vector4d normal = new Vector4d();
            invView.getColumn(2, normal);
            Vector4d clickPos = new Vector4d();
            Vector4d clickDir = new Vector4d();
            context.editor.viewToWorld(context.mouseX, context.mouseY, clickPos, clickDir);
            closest = MathUtil.projectLineToPlane(clickPos, clickDir, this.startPoint, normal);
            break;
        default:
            Vector4d handleAxisWS = new Vector4d(this.handleAxisMS);
            this.originalManipulatorTransformWS.transform(handleAxisWS);
            closest = closestPoint(context.editor, context.mouseX, context.mouseY, pos, handleAxisWS);
        }
        this.startPoint.set(closest);
    }

    @Override
    public void mouseMove(ManipulatorContext context)
    {
        Vector4d delta;
        Vector4d position = new Vector4d();
        this.originalManipulatorTransformWS.getColumn(3, position);
        switch (context.manipulatorHandle) {
        case VIEW_HANDLE_NAME:
            Matrix4d invView = new Matrix4d();
            context.editor.getCamera().getViewMatrix(invView);
            invView.invert();
            Vector4d normal = new Vector4d();
            invView.getColumn(2, normal);
            Vector4d clickPos = new Vector4d();
            Vector4d clickDir = new Vector4d();
            context.editor.viewToWorld(context.mouseX, context.mouseY, clickPos, clickDir);
            delta = MathUtil.projectLineToPlane(clickPos, clickDir, this.startPoint, normal);
            delta.sub(this.startPoint);
            break;
        default:
            Vector4d handleAxisWS = new Vector4d(this.handleAxisMS);
            this.originalManipulatorTransformWS.transform(handleAxisWS);
            delta = closestPoint(context.editor, context.mouseX, context.mouseY, this.startPoint, handleAxisWS);
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
            break;
        }
        position.add(delta);
        this.manipulatorTransformWS.setColumn(3, position);

        Matrix4d nodeTransformWS = new Matrix4d();
        int i = 0;
        for (Node n : context.nodes) {
            Vector4d manipulatorTranslation = new Vector4d();
            this.manipulatorTransformWS.getColumn(3, manipulatorTranslation);
            nodeTransformWS.set(this.manipulatorTransformWS);
            nodeTransformWS.mul(this.nodeTransformsMS[i]);
            NodeUtil.setWorldTransform(n, nodeTransformWS);
            this.manipulatorTransformWS.setColumn(3, manipulatorTranslation);
            ++i;
        }
    }
}
