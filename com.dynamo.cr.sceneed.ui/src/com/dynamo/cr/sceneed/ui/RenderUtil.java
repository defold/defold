package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.media.opengl.glu.GLUquadric;

public class RenderUtil {

    public static void drawSphere(GL gl, GLU glu, float radius, int slices, int stacks) {
        GLUquadric quadric = glu.gluNewQuadric();
        glu.gluSphere(quadric, radius, slices, stacks);
        glu.gluDeleteQuadric(quadric);
    }

    static int[] cubeIndices = new int[] { 4, 5, 6, 7, 2, 3, 7, 6, 0, 4, 7, 3, 0, 1, 5, 4, 1, 5, 6, 2, 0, 3, 2, 1 };

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
        for (int i = 0; i < cubeIndices.length; i+=4)
        {
            gl.glVertex3dv(v[cubeIndices[i+0]], 0);
            gl.glVertex3dv(v[cubeIndices[i+1]], 0);
            gl.glVertex3dv(v[cubeIndices[i+2]], 0);
            gl.glVertex3dv(v[cubeIndices[i+3]], 0);
        }
        gl.glEnd();
    }

    public static void drawCapsule(GL gl, GLU glu, float radius, float height, int slices, int stacks) {
        GLUquadric quadric = glu.gluNewQuadric();

        gl.glPushMatrix();

        gl.glPushMatrix();
        gl.glTranslatef(0, height * 0.5f, 0);
        gl.glRotatef(90, 1, 0, 0);
        glu.gluCylinder(quadric, radius, radius, height, slices, stacks);
        gl.glPopMatrix();

        gl.glTranslatef(0, -height * 0.5f, 0);
        glu.gluSphere(quadric, radius, slices, stacks);
        gl.glTranslatef(0, height, 0);
        glu.gluSphere(quadric, radius, slices, stacks);

        gl.glPopMatrix();

        glu.gluDeleteQuadric(quadric);
    }


}
