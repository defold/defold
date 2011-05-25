package com.dynamo.cr.scene.util;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.media.opengl.glu.GLUquadric;
import javax.vecmath.Matrix4d;

import com.dynamo.cr.scene.util.Constants;

public class GLUtil
{
    public static void multMatrix(GL gl, Matrix4d m)
    {
        m.transpose();

        double[] a = new double[16];
        int i = 0;
        a[i++] = m.m00;
        a[i++] = m.m01;
        a[i++] = m.m02;
        a[i++] = m.m03;

        a[i++] = m.m10;
        a[i++] = m.m11;
        a[i++] = m.m12;
        a[i++] = m.m13;

        a[i++] = m.m20;
        a[i++] = m.m21;
        a[i++] = m.m22;
        a[i++] = m.m23;

        a[i++] = m.m30;
        a[i++] = m.m31;
        a[i++] = m.m32;
        a[i++] = m.m33;

        m.transpose();

        gl.glMultMatrixd(a, 0);
    }

    static int[] indices = new int[] { 4, 5, 6, 7, 2, 3, 7, 6, 0, 4, 7, 3, 0, 1, 5, 4, 1, 5, 6, 2, 0, 3, 2, 1 };

    public static void drawCube(GL gl, double size)
    {
        double x0 = -size / 2.0f;
        double x1 = size / 2.0f;

        double v[][] = new double[8][3];
        v[0][0] = v[3][0] = v[4][0] = v[7][0] = x0;
        v[1][0] = v[2][0] = v[5][0] = v[6][0] = x1;
        v[0][1] = v[1][1] = v[4][1] = v[5][1] = x0;
        v[2][1] = v[3][1] = v[6][1] = v[7][1] = x1;
        v[0][2] = v[1][2] = v[2][2] = v[3][2] = x0;
        v[4][2] = v[5][2] = v[6][2] = v[7][2] = x1;

        gl.glBegin(GL.GL_QUADS);
        for (int i = 0; i < indices.length; i+=4)
        {
            gl.glVertex3dv(v[indices[i+0]], 0);
            gl.glVertex3dv(v[indices[i+1]], 0);
            gl.glVertex3dv(v[indices[i+2]], 0);
            gl.glVertex3dv(v[indices[i+3]], 0);
        }
        gl.glEnd();
    }

    public static void drawCube(GL gl, double x0, double y0, double z0, double x1, double y1, double z1)
    {
        double v[][] = new double[8][3];
        v[0][0] = v[3][0] = v[4][0] = v[7][0] = x0;
        v[1][0] = v[2][0] = v[5][0] = v[6][0] = x1;
        v[0][1] = v[1][1] = v[4][1] = v[5][1] = y0;
        v[2][1] = v[3][1] = v[6][1] = v[7][1] = y1;
        v[0][2] = v[1][2] = v[2][2] = v[3][2] = z0;
        v[4][2] = v[5][2] = v[6][2] = v[7][2] = z1;

        gl.glBegin(GL.GL_QUADS);
        for (int i = 0; i < indices.length; i+=4)
        {
            gl.glVertex3dv(v[indices[i+0]], 0);
            gl.glVertex3dv(v[indices[i+1]], 0);
            gl.glVertex3dv(v[indices[i+2]], 0);
            gl.glVertex3dv(v[indices[i+3]], 0);
        }
        gl.glEnd();
    }

    public static void drawArrow(GL gl, double length, double cone_length, double radius, double cone_radius)
    {
        GLU glu = new GLU();
        GLUquadric q =  glu.gluNewQuadric();

        gl.glPushMatrix();
        gl.glRotatef(90, 0, 1, 0);

        glu.gluQuadricDrawStyle(q, GLU.GLU_FILL);
        glu.gluCylinder(q, radius, radius, length - cone_length, 8, 1);

        double d = length - cone_length;
        gl.glTranslated(0, 0, d);
        glu.gluCylinder(q, cone_radius, 0, cone_length, 10, 1);

        gl.glPopMatrix();
    }

    public static void drawCircle(GL gl, double radius)
    {
        gl.glLineWidth(1.0f);

        int n = 40;
        gl.glBegin(GL.GL_LINES);

        double y0 = radius;
        double z0 = 0;

        for (int i = 0; i < n + 1; ++i)
        {
            double y1 = radius * Math.cos(2.0f * Math.PI * i / (n));
            double z1 = radius * Math.sin(2.0f * Math.PI * i / (n));
            gl.glVertex3d(0, y0, z0);
            gl.glVertex3d(0, y1, z1);
            y0 = y1;
            z0 = z1;
        }
        gl.glEnd();
        gl.glLineWidth(1.0f);
    }

    public static void drawGrid(GL gl, double size, double step)
    {
        int n = (int) (size / step / 2.0);

        gl.glColor3fv(Constants.GRID_COLOR, 0);
        gl.glBegin(GL.GL_LINES);
        for (int i = 0; i < n+1; ++i)
        {
            gl.glVertex3d(i * step, 0, -size / 2);
            gl.glVertex3d(i * step, 0, +size / 2);
            gl.glVertex3d(-i * step, 0, -size / 2);
            gl.glVertex3d(-i * step, 0, +size / 2);

            gl.glVertex3d(-size / 2, 0, i * step);
            gl.glVertex3d(+size / 2, 0, i * step);
            gl.glVertex3d(-size / 2, 0, -i * step);
            gl.glVertex3d(+size / 2, 0, -i * step);
        }

        gl.glColor3f(0, 0, 0);
        gl.glVertex3d(0, 0, -size / 2);
        gl.glVertex3d(0, 0, +size / 2);
        gl.glVertex3d(-size / 2, 0, 0);
        gl.glVertex3d(+size / 2, 0, 0);

        gl.glEnd();
    }

    public static void drawTriad(GL gl)
    {
        gl.glBegin(GL.GL_LINES);

        gl.glColor3fv(Constants.AXIS_COLOR[0], 0);
        gl.glVertex3d(0, 0, 0);
        gl.glVertex3d(1, 0, 0);

        gl.glColor3fv(Constants.AXIS_COLOR[1], 0);
        gl.glVertex3d(0, 0, 0);
        gl.glVertex3d(0, 1, 0);

        gl.glColor3fv(Constants.AXIS_COLOR[2], 0);
        gl.glVertex3d(0, 0, 0);
        gl.glVertex3d(0, 0, 1);

        gl.glEnd();
    }
}
