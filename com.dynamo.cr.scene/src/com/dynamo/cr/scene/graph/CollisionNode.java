package com.dynamo.cr.scene.graph;

import java.io.IOException;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.CollisionResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.GLUtil;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.physics.proto.Physics.ConvexShape.Type;

public class CollisionNode extends ComponentNode<CollisionResource> {

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

    public CollisionNode(String identifier, CollisionResource collisionResource, Scene scene) {
        super(identifier, collisionResource, scene);

        if (collisionResource.getConvexShapeResource() == null) {
            setError(ERROR_FLAG_RESOURCE_ERROR, "The convex shape resource '" + collisionResource.getCollisionDesc().getCollisionShape() + "' could not be loaded.");
        } else {
            ConvexShape shape = collisionResource.getConvexShapeResource().getConvexShape();
            if (shape != null) {
                if (shape.getShapeType() == ConvexShape.Type.TYPE_BOX) {
                    float w_e = shape.getData(0);
                    float h_e = shape.getData(1);
                    float d_e = shape.getData(2);
                    m_AABB.union(-w_e, -h_e, -d_e);
                    m_AABB.union(w_e, h_e, d_e);
                } else if (shape.getShapeType() == ConvexShape.Type.TYPE_SPHERE) {
                    float r = shape.getData(0);
                    m_AABB.union(-r, -r, -r);
                    m_AABB.union(r, r, r);
                } else if (shape.getShapeType() == ConvexShape.Type.TYPE_CAPSULE) {
                    float r = shape.getData(0);
                    float h = shape.getData(1);
                    m_AABB.union(0, h, 0);
                    m_AABB.union(0, -h, 0);
                    m_AABB.union(r, 0, r);
                    m_AABB.union(-r, 0, -r);
                } else {
                    setError(ERROR_FLAG_RESOURCE_ERROR, "Unsupported shape type: " + shape.getShapeType());
                }
            }
        }
    }

    @Override
    public void draw(DrawContext context) {
        if (!isOk())
            return;

        GL gl = context.m_GL;
        GLU glu = context.m_GLU;
        gl.glPushAttrib(GL.GL_ENABLE_BIT);
        gl.glEnable(GL.GL_BLEND);

        ConvexShape shape = this.resource.getConvexShapeResource().getConvexShape();
        if (shape.getShapeType() == Type.TYPE_BOX) {
            float w_e = shape.getData(0);
            float h_e = shape.getData(1);
            float d_e = shape.getData(2);

            if ((getFlags() & FLAG_GHOST) == FLAG_GHOST) {
                gl.glColor3fv(Constants.GHOST_COLOR, 0);
                GLUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
            }
            else {
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
                if (context.isSelected(this))
                    gl.glColor3fv(Constants.SELECTED_COLOR, 0);
                else
                    gl.glColor3fv(Constants.OBJECT_COLOR, 0);

                GLUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);

                gl.glColor4fv(Constants.CONVEX_SHAPE_COLOR, 0);
                GLUtil.drawCube(gl, -w_e, -h_e, -d_e, w_e, h_e, d_e);
            }
        } else if (shape.getShapeType() == Type.TYPE_SPHERE) {
            float r = shape.getData(0);

            final int slices = 16;
            final int stacks = 6;

            if ((getFlags() & FLAG_GHOST) == FLAG_GHOST) {
                gl.glColor3fv(Constants.GHOST_COLOR, 0);
                GLUtil.drawSphere(gl, glu, r, slices, stacks);
            }
            else {
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
                if (context.isSelected(this))
                    gl.glColor3fv(Constants.SELECTED_COLOR, 0);
                else
                    gl.glColor3fv(Constants.OBJECT_COLOR, 0);

                GLUtil.drawSphere(gl, glu, r, slices, stacks);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);

                gl.glColor4fv(Constants.CONVEX_SHAPE_COLOR, 0);
                GLUtil.drawSphere(gl, glu, r, slices, stacks);
            }
        } else if (shape.getShapeType() == Type.TYPE_CAPSULE) {
            float r = shape.getData(0);
            float h = shape.getData(1);

            final int slices = 16;
            final int stacks = 6;

            if ((getFlags() & FLAG_GHOST) == FLAG_GHOST) {
                gl.glColor3fv(Constants.GHOST_COLOR, 0);

                GLUtil.drawCapsule(gl, glu, r, h, slices, stacks);
            }
            else {
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);
                if (context.isSelected(this))
                    gl.glColor3fv(Constants.SELECTED_COLOR, 0);
                else
                    gl.glColor3fv(Constants.OBJECT_COLOR, 0);

                GLUtil.drawCapsule(gl, glu, r, h, slices, stacks);
                gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_FILL);

                gl.glColor4fv(Constants.CONVEX_SHAPE_COLOR, 0);
                GLUtil.drawCapsule(gl, glu, r, h, slices, stacks);
            }
        }

        gl.glPopAttrib();
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
