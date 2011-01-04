package com.dynamo.cr.contenteditor.math;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

public class Transform
{
    private Vector4d m_Translation = new Vector4d();
    private Quat4d m_Rotation = new Quat4d();

    public Transform()
    {
        setIdentity();
    }

    public Transform(float x, float y, float z, float rx, float ry, float rz, float rw)
    {
        m_Translation.set(x, y, z, 0);
        m_Rotation.set(rx, ry, rz, rw);
    }

    public Transform(Transform transform)
    {
        m_Translation.set(transform.m_Translation);
        m_Rotation.set(transform.m_Rotation);
    }

    public Matrix4d getTransform()
    {
        Matrix4d m = new Matrix4d();
        m.setIdentity();
        m.setRotation(m_Rotation);
        m.setColumn(3, m_Translation);
        m.m33 = 1;
        return m;
    }

    public void setIdentity()
    {
        m_Translation.set(0, 0, 0, 0);
        m_Rotation.set(0, 0, 0, 1);
    }

    public void set(Transform transform)
    {
        m_Translation.set(transform.m_Translation);
        m_Rotation.set(transform.m_Rotation);
    }


    /*
        T(p,q,t) = q2 * p2 *  q2' + t2

        q1 * p1 * q1' + t1 =

        q1 * ( q2 * p2 *  q2' + t2 ) * q1' + t1

        q1 * q2 * p2 * q2' * q1'   +   q1 * t2 * q1' + t1

     */

    public void mul(Transform transform1, Transform transform2)
    {
        Quat4d q1 = new Quat4d(transform1.m_Rotation);
        Quat4d q2 = new Quat4d(transform2.m_Rotation);

        Quat4d q1_conj = new Quat4d(transform1.m_Rotation);
        Quat4d q2_conj = new Quat4d(transform2.m_Rotation);
        q1_conj.conjugate();
        q2_conj.conjugate();

        Quat4d t1 = new Quat4d();
        t1.x = transform1.m_Translation.x;
        t1.y = transform1.m_Translation.y;
        t1.z = transform1.m_Translation.z;
        t1.w = transform1.m_Translation.w;

        Quat4d t2 = new Quat4d();
        t2.x = transform2.m_Translation.x;
        t2.y = transform2.m_Translation.y;
        t2.z = transform2.m_Translation.z;
        t2.w = transform2.m_Translation.w;

        Quat4d t_prim = new Quat4d();

        m_Rotation.mul(q1, q2);
        t_prim.mul(q1, t2);
        t_prim.mul(t_prim, q1_conj);
        t_prim.add(t1);
        m_Translation.set(t_prim);
    }

    @Override
    public String toString()
    {
        return String.format("%s,%s", m_Rotation.toString(), m_Translation.toString());
    }

    public void setTranslation(Vector4d translation)
    {
        m_Translation.x = translation.x;
        m_Translation.y = translation.y;
        m_Translation.z = translation.z;
    }

    public void setRotation(Quat4d rotation)
    {
        m_Rotation.set(rotation);
    }

    public void getTranslation(Vector4d translation)
    {
        translation.set(m_Translation);
    }

    public void getRotation(Quat4d rotation)
    {
        rotation.set(m_Rotation);
    }

    public void setRotation(AxisAngle4d axis_angle)
    {
        m_Rotation.set(axis_angle);
    }

    public void setTranslation(double tx, double ty, double tz)
    {
        m_Translation.x = tx;
        m_Translation.y = ty;
        m_Translation.z = tz;
    }

    public void invert()
    {
        Quat4d tmp = new Quat4d();
        tmp.x = -m_Translation.x;
        tmp.y = -m_Translation.y;
        tmp.z = -m_Translation.z;
        tmp.w = -m_Translation.w;

        tmp.mul(tmp, m_Rotation);
        m_Rotation.inverse();
        tmp.mul(m_Rotation, tmp);

        m_Translation.set(tmp);
    }

    public void transform(Vector3d v)
    {
        Quat4d tmp = new Quat4d();
        tmp.x = v.x;
        tmp.y = v.y;
        tmp.z = v.z;
        tmp.w = 0;

        Quat4d r_conj = new Quat4d(m_Rotation);
        r_conj.conjugate();

        tmp.mul(m_Rotation, tmp);
        tmp.mul(tmp, r_conj);

        v.set(tmp.x, tmp.y, tmp.z);
    }

    public void getTranslation(Vector3d translation)
    {
        translation.x = m_Translation.x;
        translation.y = m_Translation.y;
        translation.z = m_Translation.z;
    }
}
