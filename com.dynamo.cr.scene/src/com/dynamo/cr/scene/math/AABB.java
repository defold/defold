package com.dynamo.cr.scene.math;

import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;


public class AABB
{
    public Vector3d m_Min = new Vector3d();
    public Vector3d m_Max = new Vector3d();
    private boolean m_Identity;

    public AABB()
    {
        setIdentity();
    }

    public void transform(Transform transform)
    {
        if (m_Identity) {
            return;
        }

        Vector3d p0 = new Vector3d(m_Min.x, m_Min.y, m_Min.z);
        Vector3d p1 = new Vector3d(m_Min.x, m_Min.y, m_Max.z);
        Vector3d p2 = new Vector3d(m_Min.x, m_Max.y, m_Min.z);
        Vector3d p3 = new Vector3d(m_Min.x, m_Max.y, m_Max.z);
        Vector3d p4 = new Vector3d(m_Max.x, m_Min.y, m_Min.z);
        Vector3d p5 = new Vector3d(m_Max.x, m_Min.y, m_Max.z);
        Vector3d p6 = new Vector3d(m_Max.x, m_Max.y, m_Min.z);
        Vector3d p7 = new Vector3d(m_Max.x, m_Max.y, m_Max.z);

        Vector3d[] points = new Vector3d[] { p0, p1, p2, p3, p4, p5, p6, p7 };
        m_Identity = true;
        for (Vector3d p : points) {
            transform.transform(p);
            union((float) p.x, (float) p.y, (float) p.z);
        }

        Vector3d t = new Vector3d();
        transform.getTranslation(t);

        m_Min.add(t);
        m_Max.add(t);
    }

    public void union(AABB aabb)
    {
        if (aabb.m_Identity)
            return;

        m_Identity = false;
        m_Min.x = Math.min(m_Min.x, aabb.m_Min.x);
        m_Min.y = Math.min(m_Min.y, aabb.m_Min.y);
        m_Min.z = Math.min(m_Min.z, aabb.m_Min.z);

        m_Max.x = Math.max(m_Max.x, aabb.m_Max.x);
        m_Max.y = Math.max(m_Max.y, aabb.m_Max.y);
        m_Max.z = Math.max(m_Max.z, aabb.m_Max.z);
    }

    public void union(float x, float y, float z)
    {
        m_Identity = false;
        m_Min.x = Math.min(m_Min.x, x);
        m_Min.y = Math.min(m_Min.y, y);
        m_Min.z = Math.min(m_Min.z, z);

        m_Max.x = Math.max(m_Max.x, x);
        m_Max.y = Math.max(m_Max.y, y);
        m_Max.z = Math.max(m_Max.z, z);
    }

    public boolean isIdentity() {
        return m_Identity;
    }

    public void setIdentity()
    {
        m_Identity = true;
        m_Min.set(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE);
        m_Max.set(-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE);
    }

    @Override
    public String toString()
    {
        return String.format("(%s, %s)", m_Min, m_Max);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof AABB) {
            AABB aabb = (AABB) obj;
            return m_Min.equals(aabb.m_Min) && m_Max.equals(aabb.m_Max);
        }
        else {
            return super.equals(obj);
        }
    }

    public void set(AABB aabb)
    {
        m_Identity = aabb.m_Identity;
        m_Min.set(aabb.m_Min);
        m_Max.set(aabb.m_Max);
    }
}
