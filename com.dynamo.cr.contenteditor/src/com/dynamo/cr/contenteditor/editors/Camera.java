package com.dynamo.cr.contenteditor.editors;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4d;

public class Camera
{
    public enum Type
    {
        PERSPECTIVE,
        ORTHOGRAPHIC
    }

    private final Vector3f m_Position = new Vector3f();
    private final Quat4d m_Rotation = new Quat4d();

    private final Matrix4d m_ViewMatrix = new Matrix4d();
    private final Matrix4d m_Projection = new Matrix4d();
    private final int[] m_Viewport = new int[4];

    private double m_ZNear = 1;
    private double m_ZFar = 2000;
    private double m_Aspect = 1;

    private Type m_Type;
    private double m_Fov = 30;

    public Camera(Type type)
    {
        m_Type = type;
        float distance = 44.0f;
        m_Position.set(0.0f, 0.0f, 1.0f);
        m_Position.scale(distance);
        m_Rotation.set(0.0, 0.0, 0.0, 1.0);
        updateViewMatrix();
    }

    public void setAspect(double aspect)
    {
        m_Aspect = aspect;
        updateProjectionMatrix();
    }

    public double getAspect()
    {
        return m_Aspect;
    }

    public void setFov(double fov)
    {
        m_Fov = fov;
        updateProjectionMatrix();
    }

    public double getFov()
    {
        return m_Fov;
    }

    public double getNearZ()
    {
        return m_ZNear;
    }

    public double getFarZ()
    {
        return m_ZFar;
    }

    public void setPerspective(double fov, double aspect, double znear, double zfar)
    {
        m_Fov = fov;
        m_ZNear = znear;
        m_ZFar = zfar;
        m_Aspect = aspect;
        m_Type = Type.PERSPECTIVE;

        double xmin, xmax, ymin, ymax;

        ymax = znear * Math.tan(fov * Math.PI / 360.0);
        ymin = -ymax;
        xmin = ymin * aspect;
        xmax = ymax * aspect;

        setFrustum(xmin, xmax, ymin, ymax, znear, zfar);
    }

    public void setOrthographic(double fov, double aspect, double znear, double zfar)
    {
        m_Fov = fov;
        m_ZNear = znear;
        m_ZFar = zfar;
        m_Aspect = aspect;
        m_Type = Type.ORTHOGRAPHIC;

        double left = -fov / 2;
        double right = fov / 2;

        double bottom = left / aspect;
        double top = right / aspect;

        setOrtho(left, right, bottom, top, znear, zfar);
    }

    public void setOrtho2D(double left, double right, double bottom, double top)
    {
        setOrtho(left, right, bottom, top, -1.0, 1.0);
    }

    public void setOrtho(double left, double right, double bottom, double top, double near, double far)
    {
        Matrix4d m = new Matrix4d();

        m.m00 = 2.0 / (right-left);
        m.m01 = 0.0;
        m.m02 = 0.0;
        m.m03 = -(right+left) / (right-left);

        m.m10 = 0.0;
        m.m11 = 2.0 / (top-bottom);
        m.m12 = 0.0;
        m.m13 = -(top+bottom) / (top-bottom);

        m.m20 = 0.0;
        m.m21 = 0.0;
        m.m22 = -2.0 / (far - near);
        m.m23 = -(far+near) / (far-near);

        m.m30 = 0.0;
        m.m31 = 0.0;
        m.m32 = 0.0;
        m.m33 = 1.0;

        m_Projection.set(m);
    }

    private void setFrustum(double left, double right, double bottom, double top, double nearval, double farval)
    {
        double x, y, a, b, c, d;
        Matrix4d m = new Matrix4d();

        x = (2.0 * nearval) / (right - left);
        y = (2.0 * nearval) / (top - bottom);
        a = (right + left) / (right - left);
        b = (top + bottom) / (top - bottom);
        c = -(farval + nearval) / (farval - nearval);
        d = -(2.0 * farval * nearval) / (farval - nearval);

        m.m00 = x;
        m.m01 = 0.0;
        m.m02 = a;
        m.m03 = 0.0;
        m.m10 = 0.0;
        m.m11 = y;
        m.m12 = b;
        m.m13 = 0.0;
        m.m20 = 0.0;
        m.m21 = 0.0;
        m.m22 = c;
        m.m23 = d;
        m.m30 = 0.0;
        m.m31 = 0.0;
        m.m32 = -1.0;
        m.m33 = 0.0;

        m_Projection.set(m);
    }

    public void setViewport(int x, int y, int w, int h)
    {
        m_Viewport[0] = x;
        m_Viewport[1] = y;
        m_Viewport[2] = w;
        m_Viewport[3] = h;
    }

    public void getViewport(int[] view_port)
    {
        for (int i = 0; i < 4; ++i)
            view_port[i] = m_Viewport[i];
    }

    private void updateViewMatrix()
    {
        Matrix4d m = new Matrix4d();
        m.setIdentity();
        m.set(m_Rotation);

        Vector3f pos = new Vector3f(m_Position);
        m.transpose();
        m.transform(pos);
        pos.negate();
        m.setColumn(3, pos.x, pos.y, pos.z, 1.0);

        m_ViewMatrix.set(m);
    }

    private void updateProjectionMatrix()
    {
        switch (m_Type)
        {
        case PERSPECTIVE:
            setPerspective(m_Fov, m_Aspect, m_ZNear, m_ZFar);
            break;

        case ORTHOGRAPHIC:
            setOrthographic(m_Fov, m_Aspect, m_ZNear, m_ZFar);
            break;
        default:
            assert(false);
        }
    }

    public void getViewMatrix(Matrix4d m)
    {
        m.set(m_ViewMatrix);
    }

    public void getProjectionMatrix(Matrix4d m)
    {
        m.set(m_Projection);
    }

    public double[] getProjectionMatrixArray()
    {
        return toGLMatrixArray(m_Projection);
    }

    public double[] getViewMatrixArray()
    {
        Matrix4d m = new Matrix4d();
        getViewMatrix(m);
        return toGLMatrixArray(m);
    }

    public Point3d project(double objx, double objy, double objz)
    {
        /* matrice de transformation */
        //GLdouble in[4], out[4];

        Vector4d in = new Vector4d();
        /* initilise la matrice et le vecteur a transformer */
        in.x = objx;
        in.y = objy;
        in.z = objz;
        in.w = 1.0;

        Vector4d out = new Vector4d();
        Matrix4d proj = new Matrix4d();
        Matrix4d model = new Matrix4d();
        getProjectionMatrix(proj);
        getViewMatrix(model);

        model.transform(in, out);
        proj.transform(out, in);

        //transform_point(out, model, in);
        //transform_point(in, proj, out);

        /* d'ou le resultat normalise entre -1 et 1 */
        if (in.w == 0.0)
            throw new java.lang.ArithmeticException();
           //return null;

        in.x /= in.w;
        in.y /= in.w;
        in.z /= in.w;

        double[] ret = new double[3];
        /* en coordonnees ecran */
        ret[0] = m_Viewport[0] + (1 + in.x) * m_Viewport[2] / 2;
        ret[1] = m_Viewport[1] + (1 + in.y) * m_Viewport[3] / 2;
        /* entre 0 et 1 suivant z */
        ret[2] = (1 + in.z) / 2;

        // Transform from "OpenGL" to "screen" y
        ret[1] = (m_Viewport[3] - m_Viewport[0]) - ret[1] - 1;

        return new Point3d(ret);
    }

    /*
     * http://www.opengl.org/resources/faq/technical/transformations.htm
     */

    /*
The GLU library provides the gluUnProject() function for this purpose.
You'll need to read the depth buffer to obtain the input screen coordinate Z value at the X,Y location of interest. This can be coded as follows:
GLdouble z;

glReadPixels (x, y, 1, 1, GL_DEPTH_COMPONENT, GL_DOUBLE, &z);
Note that x and y are OpenGL-centric with (0,0) in the lower-left corner.
You'll need to provide the screen space X, Y, and Z values as input to gluUnProject() with the ModelView matrix, Projection matrix, and viewport that were current at the time the specific pixel of interest was rendered.
     */

    public Vector4d unProject(double winx, double winy, double winz)
    {
        // double m[16], A[16];
        // double in[4], out[4];

        Vector4d in = new Vector4d();

        // Transform from "screen" to "OpenGL"
        winy = (m_Viewport[3] - m_Viewport[0]) - winy - 1;

        in.x = (winx - m_Viewport[0]) * 2 / m_Viewport[2] - 1.0;
        in.y = (winy - m_Viewport[1]) * 2 / m_Viewport[3] - 1.0;
        in.z = 2 * winz - 1.0;
        in.w = 1.0;

        Matrix4d proj = new Matrix4d();
        Matrix4d model = new Matrix4d();
        getProjectionMatrix(proj);
        getViewMatrix(model);

        Matrix4d A = new Matrix4d();
        A.mul(proj, model);
        //A.mul(model, proj);
        A.invert();

        /* calcul transformation inverse */
        // matmul(A, proj, model);
        // invert_matrix(A, m);

        /* d'ou les coordonnees objets */

        Vector4d out = new Vector4d();
        A.transform(in, out);

        // transform_point(out, m, in);
        if (out.w == 0.0)
            throw new java.lang.ArithmeticException();

        return new Vector4d( out.x / out.w, out.y / out.w, out.z / out.w, 1 );

        /*
         * *objx = out[0] / out[3];objy = out[1] / out[3];objz = out[2] /
         * out[3]; return GL_TRUE; }
         */
    }

    public void rotate(Quat4d q)
    {
        Quat4d tmp = new Quat4d();
        tmp.set(q);
        tmp.normalize();
        m_Rotation.mul(tmp);
        updateViewMatrix();
    }

    public void rotate(AxisAngle4d a)
    {
        Quat4d q = new Quat4d();
        q.set(a);
        rotate(q);
    }

    public void getRotation(Quat4d r)
    {
        r.set(m_Rotation);
    }

    public void setRotation(Quat4d rotation)
    {
        m_Rotation.set(rotation);
        updateViewMatrix();
    }

    public void move(double x, double y, double z)
    {
        m_Position.x += x;
        m_Position.y += y;
        m_Position.z += z;
        updateViewMatrix();
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

    public void setPosition(double x, double y, double z)
    {
        m_Position.x = (float) x;
        m_Position.y = (float) y;
        m_Position.z = (float) z;

        updateViewMatrix();
    }

    public Vector4d getPosition()
    {
        return new Vector4d(m_Position.x, m_Position.y, m_Position.z, 1.0);
    }

    public Type getType()
    {
        return m_Type;
    }
}
