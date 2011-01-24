package com.dynamo.cr.contenteditor.scene;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.media.opengl.glu.GLUquadric;

import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.resource.LightResource;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightType;

public class LightNode extends ComponentNode {

    private LightResource lightResource;

    public LightNode(Scene scene, String resource, LightResource lightResource) {
        super(scene, resource);
        this.lightResource = lightResource;
    }

    @Override
    public void draw(DrawContext context) {
        GL gl = context.m_GL;

        gl.glPushAttrib(GL.GL_POLYGON_BIT | GL.GL_ENABLE_BIT);
        gl.glDisable(GL.GL_LIGHTING);

        LightDesc lightDesc = lightResource.getLightDesc();
        GLU glu = new GLU();
        GLUquadric quadric = glu.gluNewQuadric();
        gl.glColor3f(0.89f, 0.93f, 0.1f);

        if (lightDesc.getType() == LightType.POINT) {
            gl.glEnable(GL.GL_BLEND);
            gl.glColor4f(0.89f, 0.93f, 0.1f, 0.4f);
            glu.gluSphere(quadric, 0.1, 20, 8);
        }
        else if (lightDesc.getType() == LightType.SPOT) {
            gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);

            double fakeFar = 4;
            double delta = fakeFar * Math.tan(lightDesc.getConeAngle());

            gl.glTranslated(0, 0, fakeFar);
            glu.gluDisk(quadric, delta, delta, 40, 4);
            gl.glTranslated(0, 0, -fakeFar);

            gl.glBegin(GL.GL_LINES);
            gl.glVertex3d(0, 0, 0);
            gl.glVertex3d(-delta, 0, fakeFar);

            gl.glVertex3d(0, 0, 0);
            gl.glVertex3d(delta, 0, fakeFar);

            gl.glVertex3d(0, 0, 0);
            gl.glVertex3d(0, delta, fakeFar);

            gl.glVertex3d(0, 0, 0);
            gl.glVertex3d(0, -delta, fakeFar);

            gl.glEnd();
        }
        glu.gluDeleteQuadric(quadric);

        gl.glEnable(GL.GL_LIGHTING);
        gl.glPopAttrib();
    }

}
