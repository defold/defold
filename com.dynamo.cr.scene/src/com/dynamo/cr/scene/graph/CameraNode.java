package com.dynamo.cr.scene.graph;

import java.io.IOException;

import javax.media.opengl.GL;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.cr.scene.resource.CameraResource;
import com.dynamo.cr.scene.resource.Resource;

public class CameraNode extends ComponentNode<CameraResource> {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource,
                    Node parent, Scene scene, INodeFactory factory)
                    throws IOException, CreateException, CoreException {
                return new CameraNode(identifier, (CameraResource)resource, scene);
            }
        };
    }

    public CameraNode(String identifier, CameraResource resource, Scene scene) {
        super(identifier, resource, scene);
        this.resource = resource;
    }

    @Override
    public void draw(DrawContext context) {


        double[] minMax0 = new double[4];
        double[] minMax1 = new double[4];

        CameraDesc cameraDesc = resource.getCameraDesc();
        double fakeFar = 3;

        minMax0[3] = cameraDesc.getNearZ() * Math.tan(cameraDesc.getFov() * 1);
        minMax0[2] = -minMax0[3];
        minMax0[0] = minMax0[2] * cameraDesc.getAspectRatio();
        minMax0[1] = minMax0[3] * cameraDesc.getAspectRatio();

        minMax1[3] = fakeFar * Math.tan(cameraDesc.getFov() * 1);
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
