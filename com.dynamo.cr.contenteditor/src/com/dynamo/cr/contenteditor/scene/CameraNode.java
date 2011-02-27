package com.dynamo.cr.contenteditor.scene;

import javax.media.opengl.GL;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.cr.contenteditor.editors.DrawContext;
import com.dynamo.cr.contenteditor.resource.CameraResource;

public class CameraNode extends ComponentNode {

    private CameraResource cameraResource;

    public CameraNode(String resource, Scene scene, CameraResource cameraResource) {
        super(resource, scene);
        this.cameraResource = cameraResource;
    }

    @Override
    public void draw(DrawContext context) {


        double[] minMax0 = new double[4];
        double[] minMax1 = new double[4];

        CameraDesc cameraDesc = cameraResource.getCameraDesc();
        double fakeFar = 3;

        minMax0[3] = cameraDesc.getNearZ() * Math.tan(cameraDesc.getFOV() * 1);
        minMax0[2] = -minMax0[3];
        minMax0[0] = minMax0[2] * cameraDesc.getAspectRatio();
        minMax0[1] = minMax0[3] * cameraDesc.getAspectRatio();

        minMax1[3] = fakeFar * Math.tan(cameraDesc.getFOV() * 1);
        minMax1[2] = -minMax1[3];
        minMax1[0] = minMax1[2] * cameraDesc.getAspectRatio();
        minMax1[1] = minMax1[3] * cameraDesc.getAspectRatio();

        GL gl = context.m_GL;

        gl.glDisable(GL.GL_LIGHTING);

        gl.glBegin(GL.GL_LINES);
        gl.glColor3f(0.9f, 0.9f, 0.9f);
        // Draw lines
        gl.glVertex3f(0, 0, 0);
        gl.glVertex3d(minMax1[0], minMax1[2], -fakeFar);

        gl.glVertex3f(0, 0, 0);
        gl.glVertex3d(minMax1[1], minMax1[2], -fakeFar);

        gl.glVertex3f(0, 0, 0);
        gl.glVertex3d(minMax1[0], minMax1[3], -fakeFar);

        gl.glVertex3f(0, 0, 0);
        gl.glVertex3d(minMax1[1], minMax1[3], -fakeFar);
        gl.glEnd();

        // Draw near plane
        gl.glBegin(GL.GL_LINE_LOOP);
        gl.glVertex3d(minMax0[0], minMax0[2], -cameraDesc.getNearZ());
        gl.glVertex3d(minMax0[1], minMax0[2], -cameraDesc.getNearZ());
        gl.glVertex3d(minMax0[1], minMax0[3], -cameraDesc.getNearZ());
        gl.glVertex3d(minMax0[0], minMax0[3], -cameraDesc.getNearZ());
        gl.glEnd();

        // Draw fake far plane
        gl.glBegin(GL.GL_LINE_LOOP);
        gl.glVertex3d(minMax1[0], minMax1[2], -fakeFar);
        gl.glVertex3d(minMax1[1], minMax1[2], -fakeFar);
        gl.glVertex3d(minMax1[1], minMax1[3], -fakeFar);
        gl.glVertex3d(minMax1[0], minMax1[3], -fakeFar);
        gl.glEnd();

        gl.glEnable(GL.GL_LIGHTING);


    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
