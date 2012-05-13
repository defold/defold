package com.dynamo.cr.sceneed.core;

import java.io.Serializable;

import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

@SuppressWarnings("serial")
public class AABB implements Serializable {
    public Vector3d min = new Vector3d();
    public Vector3d max = new Vector3d();
    private boolean isIdentity;

    public AABB() {
        setIdentity();
    }

    public void transform(Matrix4d transform) {
        if (isIdentity) {
            return;
        }

        Vector3d p0 = new Vector3d(min.x, min.y, min.z);
        Vector3d p1 = new Vector3d(min.x, min.y, max.z);
        Vector3d p2 = new Vector3d(min.x, max.y, min.z);
        Vector3d p3 = new Vector3d(min.x, max.y, max.z);
        Vector3d p4 = new Vector3d(max.x, min.y, min.z);
        Vector3d p5 = new Vector3d(max.x, min.y, max.z);
        Vector3d p6 = new Vector3d(max.x, max.y, min.z);
        Vector3d p7 = new Vector3d(max.x, max.y, max.z);

        Vector3d[] points = new Vector3d[] { p0, p1, p2, p3, p4, p5, p6, p7 };
        isIdentity = true;
        for (Vector3d p : points) {
            transform.transform(p);
            union(p.x, p.y, p.z);
        }

        Vector4d tmp = new Vector4d();
        transform.getColumn(3, tmp);

        Vector3d t = new Vector3d(tmp.getX(), tmp.getY(), tmp.getZ());
        min.add(t);
        max.add(t);
    }

    public void union(AABB aabb) {
        if (aabb.isIdentity)
            return;

        boolean identity = isIdentity;
        isIdentity = false;

        if (identity) {
            min.set(aabb.min);
            max.set(aabb.max);
        } else {
            min.x = Math.min(min.x, aabb.min.x);
            min.y = Math.min(min.y, aabb.min.y);
            min.z = Math.min(min.z, aabb.min.z);

            max.x = Math.max(max.x, aabb.max.x);
            max.y = Math.max(max.y, aabb.max.y);
            max.z = Math.max(max.z, aabb.max.z);
        }
    }

    public void union(double x, double y, double z) {
        boolean identity = isIdentity;
        isIdentity = false;

        if (identity) {
            min.x = x;
            min.y = y;
            min.z = z;
            max.x = x;
            max.y = y;
            max.z = z;
        } else {
            min.x = Math.min(min.x, x);
            min.y = Math.min(min.y, y);
            min.z = Math.min(min.z, z);

            max.x = Math.max(max.x, x);
            max.y = Math.max(max.y, y);
            max.z = Math.max(max.z, z);
        }
    }

    public boolean isIdentity() {
        return isIdentity;
    }

    public void setIdentity() {
        isIdentity = true;
        min.set(Double.MAX_VALUE, Double.MAX_VALUE, Double.MAX_VALUE);
        max.set(-Double.MAX_VALUE, -Double.MAX_VALUE, -Double.MAX_VALUE);
    }

    @Override
    public String toString() {
        return String.format("(%s, %s)", min, max);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof AABB) {
            AABB aabb = (AABB) obj;
            return min.equals(aabb.min) && max.equals(aabb.max);
        } else {
            return super.equals(obj);
        }
    }

    public void set(AABB aabb) {
        isIdentity = aabb.isIdentity;
        min.set(aabb.min);
        max.set(aabb.max);
    }

    public Point3d getMin() {
        return new Point3d(min);
    }

    public Point3d getMax() {
        return new Point3d(max);
    }

}
