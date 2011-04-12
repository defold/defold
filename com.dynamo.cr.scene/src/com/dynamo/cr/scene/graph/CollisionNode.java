package com.dynamo.cr.scene.graph;

import java.io.IOException;

import javax.media.opengl.GL;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.CollisionResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.GLUtil;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.physics.proto.Physics.ConvexShape.Type;

public class CollisionNode extends ComponentNode {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource,
                    Node parent, Scene scene, INodeFactory factory)
                    throws IOException, CreateException, CoreException {
                return new CollisionNode(identifier, (CollisionResource)resource, scene);
            }
        };
    }

    private CollisionResource collisionResource;
    private boolean invalid = false;

    public CollisionNode(String identifier, CollisionResource collisionResource, Scene scene) {
        super(identifier, scene);
        this.collisionResource = collisionResource;
    }

    @Override
    public void draw(DrawContext context) {
        if (invalid)
            return;

        GL gl = context.m_GL;
        gl.glPushAttrib(GL.GL_ENABLE_BIT);
        gl.glEnable(GL.GL_BLEND);

        ConvexShape shape = collisionResource.getConvexShapeResource().getConvexShape();
        if (shape.getShapeType() == Type.TYPE_BOX) {
            if (shape.getDataCount() != 3) {
                invalid = true;
                System.err.println("Invalid box shape");
            }
            else {
                float w_e = shape.getData(0);
                float h_e = shape.getData(1);
                float d_e = shape.getData(2);

                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
                if (context.isSelected(this))
                    gl.glColor3fv(Constants.SELECTED_COLOR, 0);
                else
                    gl.glColor3fv(Constants.OBJECT_COLOR, 0);
                GLUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);

                if (context.isSelected(this)) {
                }

                gl.glColor4f(255.0f / 255.0f, 247.0f / 255.0f, 73.0f/255.0f, 0.4f);
                GLUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
            }

        } else {
            System.err.println("Warning: Unsupported shape type: " + shape.getShapeType());
            invalid = true;
        }

        gl.glPopAttrib();
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
