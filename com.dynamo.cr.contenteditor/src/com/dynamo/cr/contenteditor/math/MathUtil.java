package com.dynamo.cr.contenteditor.math;

import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.ddf.math.DdfMath.Point3;
import com.dynamo.ddf.math.DdfMath.Quat;

public class MathUtil
{

    public static void basisMatrix(Matrix4d transform, int axis)
    {
        transform.setIdentity();

        if (axis == 0)
        {
        }
        else if (axis == 1)
        {
            transform.setColumn(0, 0, 1, 0, 0);
            transform.setColumn(1, 0, 0, 1, 0);
            transform.setColumn(2, 1, 0, 0, 0);
        }
        else if (axis == 2)
        {
            transform.setColumn(0, 0, 0, 1, 0);
            transform.setColumn(1, 1, 0, 0, 0);
            transform.setColumn(2, 0, 1, 0, 0);
        }
    }

    public static Vector4d intersectRayPlane(Vector4d r0, Vector4d rd, Vector4d plane)
    {
        double t = -(MathUtil.dot(plane, r0) + plane.w) / MathUtil.dot(plane, rd);
        Vector4d ret = new Vector4d(rd);
        ret.scaleAdd(t, r0);
        ret.w = 1;
        return ret;
    }

    public static Vector4d closestPointToCircle(Vector4d c, Vector4d plane, double r, Vector4d r0, Vector4d rd)
    {
        Vector4d inter = intersectRayPlane(r0, rd, plane);
        Vector4d tmp = new Vector4d();
        tmp.sub(inter, c);
        tmp.normalize();
        tmp.scale(r);
        tmp.add(c);
        return tmp;
    }

    public static double dot(Vector4d v0, Vector4d v1)
    {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    }

    public static Vector4d cross(Vector4d a, Vector4d b)
    {
        Vector3d a3 = new Vector3d(a.x, a.y, a.z);
        Vector3d b3 = new Vector3d(b.x, b.y, b.z);
        Vector3d tmp = new Vector3d();
        tmp.cross(a3, b3);
        tmp.normalize();
        return new Vector4d(tmp.x, tmp.y, tmp.z, 0);
    }

    public static Vector4d closestLineToLine(Vector4d p0, Vector4d d0, Vector4d p1, Vector4d d1)
    {
        Vector4d u = d0;
        Vector4d v = d1;
        Vector4d w = new Vector4d();
        w.sub(p0, p1);
        double a = u.dot(u); // always >= 0
        double b = u.dot(v);
        double c = v.dot(v); // always >= 0
        double d = u.dot(w);
        double e = v.dot(w);
        double D = a * c - b * b; // always >= 0
        double sc, tc;

        // compute the line parameters of the two closest points
        if (D < 0.00001)
        { // the lines are almost parallel
            sc = 0.0;
            tc = (b > c ? d / b : e / c); // use the largest denominator
        }
        else
        {
            sc = (b * e - c * d) / D;
            tc = (a * e - b * d) / D;
        }

        Vector4d closest0 = new Vector4d(d0);
        closest0.scaleAdd(sc, p0);

        Vector4d closest1 = new Vector4d(d1);
        closest1.scaleAdd(tc, p1);

        Vector4d r = new Vector4d(closest0);
        r.add(closest1);
        r.scale(0.5);
        return r;
    }

    public static Vector4d projectLineToLine(Vector4d p0, Vector4d d0, Vector4d p1, Vector4d d1)
    {
        Vector4d u = d0;
        Vector4d v = d1;
        Vector4d w = new Vector4d();
        w.sub(p0, p1);
        double a = u.dot(u); // always >= 0
        double b = u.dot(v);
        double c = v.dot(v); // always >= 0
        double d = u.dot(w);
        double e = v.dot(w);
        double D = a * c - b * b; // always >= 0
        double sc;

        // compute the line parameters of the two closest points
        if (D < 0.00001)
        { // the lines are almost parallel
            sc = 0.0;
        }
        else
        {
            sc = (b * e - c * d) / D;
        }

        Vector4d closest0 = new Vector4d(d0);
        closest0.scaleAdd(sc, p0);

        return closest0;
    }

    public static void basisMatrix(Quat4d rotation, int axis)
    {
        Matrix4d t = new Matrix4d();
        basisMatrix(t, axis);
        rotation.set(t);
    }

    public static void quatToEuler(Quat4d quat, Tuple4d euler)
    {
        double heading;
        double attitude;
        double bank;

        double test = quat.x * quat.y + quat.z * quat.w;
        if (test > 0.499)
        { // singularity at north pole
            heading = 2 * Math.atan2(quat.x, quat.w);
            attitude = Math.PI / 2;
            bank = 0;
        }
        else if (test < -0.499)
        { // singularity at south pole
            heading = -2 * Math.atan2(quat.x, quat.w);
            attitude = -Math.PI / 2;
            bank = 0;
        }
        else
        {
            double sqx = quat.x * quat.x;
            double sqy = quat.y * quat.y;
            double sqz = quat.z * quat.z;
            heading = Math.atan2(2 * quat.y * quat.w - 2 * quat.x * quat.z, 1 - 2 * sqy - 2 * sqz);
            attitude = Math.asin(2 * test);
            bank = Math.atan2(2 * quat.x * quat.w - 2 * quat.y * quat.z, 1 - 2 * sqx - 2 * sqz);
        }

        euler.x = bank * 180 / Math.PI;
        euler.y = heading * 180 / Math.PI;
        euler.z = attitude * 180 / Math.PI;
        euler.w = 0;
    }

    public static void eulerToQuat(Tuple4d euler, Quat4d quat)
    {
        double bank = euler.x * Math.PI / 180;
        double heading = euler.y * Math.PI / 180;
        double attitude = euler.z * Math.PI / 180;

        double c1 = Math.cos(heading/2);
        double s1 = Math.sin(heading/2);
        double c2 = Math.cos(attitude/2);
        double s2 = Math.sin(attitude/2);
        double c3 = Math.cos(bank/2);
        double s3 = Math.sin(bank/2);
        double c1c2 = c1*c2;
        double s1s2 = s1*s2;
        double w =c1c2*c3 - s1s2*s3;
        double x =c1c2*s3 + s1s2*c3;
        double y =s1*c2*c3 + c1*s2*s3;
        double z =c1*s2*c3 - s1*c2*s3;

        /*
        // Assuming the angles are in radians.
        double c1 = Math.cos(heading);
        double s1 = Math.sin(heading);
        double c2 = Math.cos(attitude);
        double s2 = Math.sin(attitude);
        double c3 = Math.cos(bank);
        double s3 = Math.sin(bank);
        double w = Math.sqrt(1.0 + c1 * c2 + c1*c3 - s1 * s2 * s3 + c2*c3) / 2.0;
        double w4 = (4.0 * w);
        double x = (c2 * s3 + c1 * s3 + s1 * s2 * c3) / w4 ;
        double y = (s1 * c2 + s1 * c3 + c1 * s2 * s3) / w4 ;
        double z = (-s1 * s3 + c1 * s2 * c3 +s2) / w4 ;
*/
        quat.x = x;
        quat.y = y;
        quat.z = z;
        quat.w = w;
    }

    public static Vector4d toVector4(Point3 p) {
        return new Vector4d(p.m_X, p.m_Y, p.m_Z, 1);
    }

    public static Quat4d toQuat4(Quat q) {
        return new Quat4d(q.m_X, q.m_Y, q.m_Z, q.m_W);
    }

    public static Point3 toPoint3(Vector4d translation) {
        Point3 ret = new Point3();
        ret.m_X = (float) translation.x;
        ret.m_Y = (float) translation.y;
        ret.m_Z = (float) translation.z;
        return ret;
    }

    public static Quat toQuat(Quat4d rotation) {
        Quat quat = new Quat();
        quat.m_X = (float) rotation.x;
        quat.m_Y = (float) rotation.y;
        quat.m_Z = (float) rotation.z;
        quat.m_W = (float) rotation.w;
        return quat;
    }
}
