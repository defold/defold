package com.dynamo.cr.contenteditor.math;

import javax.vecmath.Vector3d;


public class AABB
{
    public Vector3d m_Min = new Vector3d();
    public Vector3d m_Max = new Vector3d();

    public AABB()
    {
        setZero();
    }

    public void transform(Transform transform)
    {
        Vector3d min = new Vector3d(m_Min);
        Vector3d max = new Vector3d(m_Max);

        Vector3d half_extents = new Vector3d();
        half_extents.sub(max, min);
        half_extents.scale(0.5);

        Vector3d center = new Vector3d();
        center.add(min, max);
        center.scale(0.5);

//        min.sub(center, half_extents);
//        max.add(center, half_extents);

        min.set(half_extents);
        min.scale(-1);
        max.set(half_extents);

        boolean testa = true;
        if (testa)
        {
            Vector3d e1 = new Vector3d(half_extents.x, half_extents.y, half_extents.z);
            Vector3d e2 = new Vector3d(half_extents.x, -half_extents.y, half_extents.z);
            Vector3d e3 = new Vector3d(half_extents.x, half_extents.y, -half_extents.z);
            Vector3d e4 = new Vector3d(half_extents.x, -half_extents.y, -half_extents.z);

            transform.transform(e1);
            transform.transform(e2);
            transform.transform(e3);
            transform.transform(e4);

            min.x = e1.x;
            min.y = e1.y;
            min.z = e1.z;
            max.x = e1.x;
            max.y = e1.y;
            max.z = e1.z;

            Vector3d[] es = new Vector3d[] { e1, e2, e3, e4 };
            for (Vector3d e : es)
            {
                min.x = Math.min(min.x, e.x);
                min.y = Math.min(min.y, e.y);
                min.z = Math.min(min.z, e.z);
                min.x = Math.min(min.x, -e.x);
                min.y = Math.min(min.y, -e.y);
                min.z = Math.min(min.z, -e.z);

                max.x = Math.max(max.x, e.x);
                max.y = Math.max(max.y, e.y);
                max.z = Math.max(max.z, e.z);
                max.x = Math.max(max.x, -e.x);
                max.y = Math.max(max.y, -e.y);
                max.z = Math.max(max.z, -e.z);

            }

        }

        else
        {
            // TODO: Hmm bug... det Šr basvektorerna vi ska transformera...
            transform.transform(min);
            transform.transform(max);

        }

        half_extents.sub(min, max);
        half_extents.absolute();
        half_extents.scale(0.5);

        m_Min.sub(center, half_extents);
        m_Max.add(center, half_extents);

        Vector3d t = new Vector3d();
        transform.getTranslation(t);

        m_Min.add(t);
        m_Max.add(t);
    }

    public void union(AABB aabb)
    {
        m_Min.x = Math.min(m_Min.x, aabb.m_Min.x);
        m_Min.y = Math.min(m_Min.y, aabb.m_Min.y);
        m_Min.z = Math.min(m_Min.z, aabb.m_Min.z);

        m_Max.x = Math.max(m_Max.x, aabb.m_Max.x);
        m_Max.y = Math.max(m_Max.y, aabb.m_Max.y);
        m_Max.z = Math.max(m_Max.z, aabb.m_Max.z);
    }

    public void union(float x, float y, float z)
    {
        m_Min.x = Math.min(m_Min.x, x);
        m_Min.y = Math.min(m_Min.y, y);
        m_Min.z = Math.min(m_Min.z, z);

        m_Max.x = Math.max(m_Max.x, x);
        m_Max.y = Math.max(m_Max.y, y);
        m_Max.z = Math.max(m_Max.z, z);
    }

    public void setZero()
    {
        m_Min.set(0, 0, 0);
        m_Max.set(0, 0, 0);
//        m_Min.set(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE);
  //      m_Max.set(-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE);
    }

    public void setIdentity()
    {
        m_Min.set(Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE);
        m_Max.set(-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE);
    }

    @Override
    public String toString()
    {
        return String.format("(%s, %s)", m_Min, m_Max);
    }

    public void set(AABB aabb)
    {
        m_Min.set(aabb.m_Min);
        m_Max.set(aabb.m_Max);
    }
}
