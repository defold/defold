package com.dynamo.cr.contenteditor.manipulator;

import javax.media.opengl.GL;
import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;

import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.GLUtil;

public class RotateManipulator extends TransformManipulator {
    private int startX;
    private double angle = 0;
    private double snapValue = Math.PI * 0.25;
    private final double radius = 120;

    private final static double SNAP_LIMIT = 0.33;

    @Override
    public void draw(ManipulatorDrawContext context) {
        super.draw(context);

        GL gl = context.gl;
        double factor = ManipulatorUtil.getHandleScaleFactor(getHandlePosition(context.nodes), context.editor);
        double r = this.radius / factor;

        gl.glLineWidth(3);

        for (int i = 0; i < 3; ++i) {
            gl.glColor3fv(Constants.AXIS_COLOR[i], 0);
            gl.glPushMatrix();
            GLUtil.multMatrix(gl, this.manipulatorTransformWS);
            GLUtil.multMatrix(gl, this.handleTransforms[i]);

            context.beginDrawHandle();
            GLUtil.drawCircle(gl, r);
            context.endDrawHandle();

            gl.glPopMatrix();
        }

        if (context.selectMode() || context.manipulatorHandle == -1) {
            return;
        }

        double da = 5 * Math.PI / 180.0;

        gl.glEnable (GL.GL_BLEND);
        gl.glBlendFunc (GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
        gl.glColor4d(0.6, 0.4, 0.0, 0.5);

        gl.glPushMatrix();
        GLUtil.multMatrix(gl, this.originalManipulatorTransformWS);
        GLUtil.multMatrix(gl, this.handleTransforms[context.manipulatorHandle]);

        gl.glBegin(GL.GL_TRIANGLES);

        double y0 = r;
        double z0 = 0;

        for (double a = 0; a <= Math.abs(this.angle) + da; a += da) {
            double aa = Math.min(a, Math.abs(this.angle)) * Math.signum(this.angle);

            double y1 = r * Math.cos(aa);
            double z1 = r * Math.sin(aa);

            gl.glVertex3d(0, 0, 0);
            gl.glVertex3d(0, y0, z0);
            gl.glVertex3d(0, y1, z1);

            y0 = y1;
            z0 = z1;
        }
        gl.glEnd();
        gl.glDisable(GL.GL_BLEND);

        gl.glPopMatrix();
    }

    @Override
    public void mouseDown(ManipulatorContext context) {
        super.mouseDown(context);

        this.startX = context.mouseX;
        this.angle = 0;
    }

    @Override
    public void mouseMove(ManipulatorContext context) {
        int dx = context.mouseX - this.startX;
        double mouseAngle = dx * 0.02;
        this.angle = mouseAngle;
        if (context.snapActive && this.snapValue > 0.0) {
            double t = mouseAngle / this.snapValue;
            double t0 = Math.round(t);
            if (Math.abs(t - t0) < SNAP_LIMIT) {
                this.angle = this.snapValue * t0;
            }
        }

        this.manipulatorTransformWS.set(this.originalManipulatorTransformWS);
        Matrix4d m = new Matrix4d();
        AxisAngle4d aa = new AxisAngle4d(this.handleAxisMS.x, this.handleAxisMS.y, this.handleAxisMS.z, angle);
        m.set(aa);
        this.manipulatorTransformWS.mul(m);

        super.mouseMove(context);
    }

    @Override
    public void mouseUp(ManipulatorContext context) {
        super.mouseUp(context);

        this.angle = 0;
    }
}
