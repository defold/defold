package com.dynamo.cr.sceneed.core;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

public class Camera {

    public enum Type {
        PERSPECTIVE, ORTHOGRAPHIC
    }

    private Vector3d position = new Vector3d();
    private Quat4d rotation = new Quat4d();

    private Matrix4d viewMatrix = new Matrix4d();
    private Matrix4d projection = new Matrix4d();
    private int[] viewport;

    private double zNear;
    private double zFar;
    private double aspect;

    private Type type;
    private double fov;

    public Camera(Type type) {
        this.type = type;
        this.zNear = 1;
        this.zFar = 2000;
        this.fov = 30;
        this.aspect = 1;
        double distance = 44.0;
        this.position.set(0.0, 0.0, 1.0);
        this.position.scale(distance);
        this.rotation.set(0.0, 0.0, 0.0, 1.0);
        reset();
    }

    public void reset() {
        this.viewport = new int[4];
        updateViewMatrix();
        updateProjectionMatrix();
    }

    public void setAspect(double aspect) {
        this.aspect = aspect;
        updateProjectionMatrix();
    }

    public double getAspect() {
        return aspect;
    }

    public void setFov(double fov) {
        this.fov = fov;
        updateProjectionMatrix();
    }

    public double getFov() {
        return fov;
    }

    public double getNearZ() {
        return zNear;
    }

    public double getFarZ() {
        return zFar;
    }

    public void setPerspective(double fov, double aspect, double znear,
            double zfar) {
        this.fov = fov;
        this.zNear = znear;
        this.zFar = zfar;
        this.aspect = aspect;
        this.type = Type.PERSPECTIVE;

        double xmin, xmax, ymin, ymax;

        ymax = znear * Math.tan(fov * Math.PI / 360.0);
        ymin = -ymax;
        xmin = ymin * aspect;
        xmax = ymax * aspect;

        setFrustum(xmin, xmax, ymin, ymax, znear, zfar);
    }

    public void setOrthographic(double fov, double aspect, double znear,
            double zfar) {
        this.fov = fov;
        this.zNear = znear;
        this.zFar = zfar;
        this.aspect = aspect;
        this.type = Type.ORTHOGRAPHIC;

        double left = -fov / 2;
        double right = fov / 2;

        double bottom = left / aspect;
        double top = right / aspect;

        setOrtho(left, right, bottom, top, znear, zfar);
    }

    public void setOrtho2D(double left, double right, double bottom, double top) {
        setOrtho(left, right, bottom, top, -1.0, 1.0);
    }

    public void setOrtho(double left, double right, double bottom, double top,
            double near, double far) {
        Matrix4d m = new Matrix4d();

        m.m00 = 2.0 / (right - left);
        m.m01 = 0.0;
        m.m02 = 0.0;
        m.m03 = -(right + left) / (right - left);

        m.m10 = 0.0;
        m.m11 = 2.0 / (top - bottom);
        m.m12 = 0.0;
        m.m13 = -(top + bottom) / (top - bottom);

        m.m20 = 0.0;
        m.m21 = 0.0;
        m.m22 = -2.0 / (far - near);
        m.m23 = -(far + near) / (far - near);

        m.m30 = 0.0;
        m.m31 = 0.0;
        m.m32 = 0.0;
        m.m33 = 1.0;

        projection.set(m);
    }

    private void setFrustum(double left, double right, double bottom,
            double top, double nearval, double farval) {
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

        projection.set(m);
    }

    public void setViewport(int x, int y, int w, int h) {
        this.viewport[0] = x;
        this.viewport[1] = y;
        this.viewport[2] = w;
        this.viewport[3] = h;
    }

    public void getViewport(int[] view_port) {
        for (int i = 0; i < 4; ++i)
            view_port[i] = viewport[i];
    }

    private void updateViewMatrix() {
        Matrix4d m = new Matrix4d();
        m.setIdentity();
        m.set(rotation);

        Vector3d pos = new Vector3d(position);
        m.transpose();
        m.transform(pos);
        pos.negate();
        m.setColumn(3, pos.x, pos.y, pos.z, 1.0);

        viewMatrix.set(m);
    }

    private void updateProjectionMatrix() {
        switch (type) {
        case PERSPECTIVE:
            setPerspective(fov, aspect, zNear, zFar);
            break;

        case ORTHOGRAPHIC:
            setOrthographic(fov, aspect, zNear, zFar);
            break;
        default:
            assert (false);
        }
    }

    public void getViewMatrix(Matrix4d m) {
        m.set(viewMatrix);
    }

    public void getProjectionMatrix(Matrix4d m) {
        m.set(projection);
    }

    public double[] getProjectionMatrixArray() {
        return toGLMatrixArray(projection);
    }

    public double[] getViewMatrixArray() {
        Matrix4d m = new Matrix4d();
        getViewMatrix(m);
        return toGLMatrixArray(m);
    }

    public Point3d project(double objx, double objy, double objz) {
        Vector4d in = new Vector4d();
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

        if (in.w == 0.0)
            throw new java.lang.ArithmeticException();

        in.x /= in.w;
        in.y /= in.w;
        in.z /= in.w;

        double[] ret = new double[3];

        ret[0] = viewport[0] + (1 + in.x) * viewport[2] / 2;
        ret[1] = viewport[1] + (1 + in.y) * viewport[3] / 2;
        ret[2] = (1 + in.z) / 2;

        // Transform from "OpenGL" to "screen" y
        ret[1] = (viewport[3] - viewport[0]) - ret[1] - 1;

        return new Point3d(ret);
    }

    public Vector4d unProject(double winx, double winy, double winz) {
        Vector4d in = new Vector4d();

        // Transform from "screen" to "OpenGL"
        winy = (viewport[3] - viewport[0]) - winy - 1;

        in.x = (winx - viewport[0]) * 2 / viewport[2] - 1.0;
        in.y = (winy - viewport[1]) * 2 / viewport[3] - 1.0;
        in.z = 2 * winz - 1.0;
        in.w = 1.0;

        Matrix4d proj = new Matrix4d();
        Matrix4d model = new Matrix4d();
        getProjectionMatrix(proj);
        getViewMatrix(model);

        Matrix4d A = new Matrix4d();
        A.mul(proj, model);
        A.invert();

        Vector4d out = new Vector4d();
        A.transform(in, out);

        if (out.w == 0.0)
            throw new java.lang.ArithmeticException();

        return new Vector4d(out.x / out.w, out.y / out.w, out.z / out.w, 1);
    }

    public void rotate(Quat4d q) {
        Quat4d tmp = new Quat4d();
        tmp.set(q);
        tmp.normalize();
        rotation.mul(tmp);
        updateViewMatrix();
    }

    public void rotate(AxisAngle4d a) {
        Quat4d q = new Quat4d();
        q.set(a);
        rotate(q);
    }

    public void getRotation(Quat4d r) {
        r.set(rotation);
    }

    public void setRotation(Quat4d rotation) {
        rotation.set(rotation);
        updateViewMatrix();
    }

    public void move(double x, double y, double z) {
        position.x += x;
        position.y += y;
        position.z += z;
        updateViewMatrix();
    }

    private static double[] toGLMatrixArray(Matrix4d in) {
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

    public void setPosition(double x, double y, double z) {
        this.position.x = x;
        this.position.y = y;
        this.position.z = z;

        updateViewMatrix();
    }

    public Vector4d getPosition() {
        return new Vector4d(position.x, position.y, position.z, 1.0);
    }

    public Type getType() {
        return type;
    }
}
