package com.dynamo.cr.scene.graph;

import java.io.IOException;

import javax.media.opengl.GL;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.math.AABB;
import com.dynamo.cr.scene.math.Transform;
import com.dynamo.cr.scene.resource.CollisionResource;
import com.dynamo.cr.scene.resource.CollisionShapeResource;
import com.dynamo.cr.scene.resource.ConvexShapeResource;
import com.dynamo.cr.scene.resource.Resource;
import com.dynamo.cr.scene.resource.TileGridResource;
import com.dynamo.cr.scene.util.Constants;
import com.dynamo.cr.scene.util.GLUtil;
import com.dynamo.physics.proto.Physics.CollisionShape;
import com.dynamo.physics.proto.Physics.CollisionShape.Shape;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.physics.proto.Physics.ConvexShape.Type;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;

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

        Resource collisionShapeResource = collisionResource.getCollisionShapeResource();
        if (collisionShapeResource == null) {
            setError(ERROR_FLAG_RESOURCE_ERROR, "The collision shape resource '" + collisionResource.getCollisionDesc().getCollisionShape() + "' could not be loaded.");
        } else {
            if (collisionShapeResource instanceof ConvexShapeResource) {
                ConvexShapeResource convexShapeResource = (ConvexShapeResource)collisionShapeResource;
                ConvexShape shape = convexShapeResource.getConvexShape();
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
            } else if (collisionShapeResource instanceof CollisionShapeResource) {
                CollisionShapeResource shapeResource = (CollisionShapeResource)collisionShapeResource;
                CollisionShape shape = shapeResource.getConvexShape();
                int n = shape.getShapesCount();
                for (int i = 0; i < n; ++i)
                {
                    Shape s = shape.getShapes(i);
                    Matrix4d t = getShapeTransform(s);
                    switch (s.getShapeType()) {
                    case TYPE_BOX:
                        float w_e = shape.getData(s.getIndex());
                        float h_e = shape.getData(s.getIndex()+1);
                        float d_e = shape.getData(s.getIndex()+2);
                        Vector3d e = new Vector3d(w_e, h_e, d_e);
                        Vector3d ne = new Vector3d(e);
                        ne.negate();
                        t.transform(e);
                        t.transform(ne);
                        m_AABB.union((float)e.getX(), (float)e.getY(), (float)e.getZ());
                        m_AABB.union((float)ne.getX(), (float)ne.getY(), (float)ne.getZ());
                        break;
                    case TYPE_SPHERE:
                        float r = shape.getData(s.getIndex());
                        e = new Vector3d(r, r, r);
                        ne = new Vector3d(e);
                        ne.negate();
                        t.transform(e);
                        t.transform(ne);
                        m_AABB.union((float)e.getX(), (float)e.getY(), (float)e.getZ());
                        m_AABB.union((float)ne.getX(), (float)ne.getY(), (float)ne.getZ());
                        break;
                    case TYPE_CAPSULE:
                        r = shape.getData(s.getIndex());
                        float h = shape.getData(s.getIndex()+1);
                        AABB aabb = new AABB();
                        aabb.union(0, h, 0);
                        aabb.union(0, -h, 0);
                        aabb.union(r, 0, r);
                        aabb.union(-r, 0, -r);
                        aabb.transform(new Transform(t));
                        m_AABB.union(aabb);
                    default:
                        setError(ERROR_FLAG_RESOURCE_ERROR, "Unsupported shape type: " + s.getShapeType());
                    }
                }
            } else if (collisionShapeResource instanceof TileGridResource) {
                TileGridResource tileGridResource = (TileGridResource)collisionShapeResource;
                m_AABB.set(tileGridResource.getAABB());
            } else {
                setError(ERROR_FLAG_RESOURCE_ERROR, "The collision shape resource '" + collisionResource.getCollisionDesc().getCollisionShape() + "' must either be a convex shape or a tile grid.");
            }
        }
    }

    private Matrix4d getShapeTransform(Shape shape) {
        Quat qp = shape.getRotation();
        Quat4d q = new Quat4d(qp.getX(), qp.getY(), qp.getZ(), qp.getW());
        Point3 pp = shape.getPosition();
        Vector3d v = new Vector3d(pp.getX(), pp.getY(), pp.getZ());
        return new Matrix4d(q, v, 1.0f);
    }

    @Override
    public void draw(DrawContext context) {
        if (!isOk())
            return;

        GL gl = context.m_GL;
        gl.glPushAttrib(GL.GL_ENABLE_BIT | GL.GL_DEPTH_BUFFER_BIT);
        gl.glEnable(GL.GL_BLEND);
        gl.glDepthMask(false);

        Resource collisionShapeResource = this.resource.getCollisionShapeResource();
        if (collisionShapeResource instanceof ConvexShapeResource) {
            ConvexShape shape = ((ConvexShapeResource)collisionShapeResource).getConvexShape();
            if (shape.getShapeType() == Type.TYPE_BOX) {
                float w_e = shape.getData(0);
                float h_e = shape.getData(1);
                float d_e = shape.getData(2);
                drawBox(context, w_e, h_e, d_e);
            } else if (shape.getShapeType() == Type.TYPE_SPHERE) {
                float r = shape.getData(0);
                drawSphere(context, r);
            } else if (shape.getShapeType() == Type.TYPE_CAPSULE) {
                float r = shape.getData(0);
                float h = shape.getData(1);
                drawCapsule(context, r, h);
            }
        } else if (collisionShapeResource instanceof CollisionShapeResource) {
            CollisionShape shape = ((CollisionShapeResource)collisionShapeResource).getConvexShape();
            int n = shape.getShapesCount();
            for (int i = 0; i < n; ++i) {
                Shape s = shape.getShapes(i);
                Matrix4d m = getShapeTransform(s);
                gl.glPushMatrix();
                gl.glLoadMatrixd(toGLMatrixArray(m), 0);
                switch (s.getShapeType()) {
                case TYPE_BOX:
                    float w_e = shape.getData(0);
                    float h_e = shape.getData(1);
                    float d_e = shape.getData(2);
                    drawBox(context, w_e, h_e, d_e);
                    break;
                case TYPE_SPHERE:
                    float r = shape.getData(0);
                    drawSphere(context, r);
                    break;
                case TYPE_CAPSULE:
                    r = shape.getData(0);
                    float h = shape.getData(1);
                    drawCapsule(context, r, h);
                    break;
                }
                gl.glPopMatrix();
            }
    } else if (collisionShapeResource instanceof TileGridResource) {
            // TODO Render hulls
        }

        gl.glPopAttrib();
    }

    private void drawBox(DrawContext context, float h_e, float w_e, float d_e) {
        GL gl = context.m_GL;
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
    }

    private void drawSphere(DrawContext context, float r) {
        final int slices = 16;
        final int stacks = 6;

        GL gl = context.m_GL;
        GLU glu = context.m_GLU;

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
    }

    private void drawCapsule(DrawContext context, float r, float h) {
        final int slices = 16;
        final int stacks = 6;

        GL gl = context.m_GL;
        GLU glu = context.m_GLU;

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

    private static double[] toGLMatrixArray(Matrix4d in)
    {
        Matrix4d m = new Matrix4d();
        m.set(in);
        m.transpose();

        double[] ret = new double[16];
        int i = 0;
        ret[i++] = m.m00;
        ret[i++] = m.m01;
        ret[i++] = m.m02;
        ret[i++] = m.m03;

        ret[i++] = m.m10;
        ret[i++] = m.m11;
        ret[i++] = m.m12;
        ret[i++] = m.m13;

        ret[i++] = m.m20;
        ret[i++] = m.m21;
        ret[i++] = m.m22;
        ret[i++] = m.m23;

        ret[i++] = m.m30;
        ret[i++] = m.m31;
        ret[i++] = m.m32;
        ret[i++] = m.m33;

        return ret;
    }

    @Override
    protected boolean verifyChild(Node child) {
        return false;
    }
}
