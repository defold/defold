package com.defold.editor.pipeline;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;

public class MathUtil {
    public static Point3d ddfToVecmath(Point3 p) {
        return new Point3d(p.getX(), p.getY(), p.getZ());
    }

    public static Vector3d ddfToVecmath(Vector3 p) {
        return new Vector3d(p.getX(), p.getY(), p.getZ());
    }

    public static Quat4d ddfToVecmath(Quat q) {
        return new Quat4d(q.getX(), q.getY(), q.getZ(), q.getW());
    }

    public static Point3 vecmathToDDF(Point3d p) {
        Point3.Builder b = Point3.newBuilder();
        return b.setX((float)p.getX()).setY((float)p.getY()).setZ((float)p.getZ()).build();
    }

    public static Quat vecmathToDDF(Quat4d q) {
        Quat.Builder b = Quat.newBuilder();
        return b.setX((float)q.getX()).setY((float)q.getY()).setZ((float)q.getZ()).setW((float)q.getW()).build();
    }

    public static Vector3 vecmathToDDF(Vector3d p) {
        Vector3.Builder b = Vector3.newBuilder();
        return b.setX((float)p.getX()).setY((float)p.getY()).setZ((float)p.getZ()).build();
    }

    public static void rotate(Quat4d rotation, Point3d p) {
        Quat4d q = new Quat4d();
        // Needed to avoid normalization
        q.set(new Vector4d(p));
        q.mul(rotation, q);
        q.mulInverse(rotation);
        p.set(q.getX(), q.getY(), q.getZ());
    }

    /**
     * Transforms p by the inverse of the transform defined by position and rotation.
     * @param position
     * @param rotation
     * @param p
     */
    public static void invTransform(Point3d position, Quat4d rotation, Point3d p) {
        p.sub(position);
        Quat4d q = new Quat4d(rotation);
        q.conjugate();
        rotate(q, p);
    }

    /**
     * Transforms q by the inverse of the transform defined by the rotation.
     * @param rotation
     * @param q
     */
    public static void invTransform(Quat4d rotation, Quat4d q) {
        Quat4d q1 = new Quat4d(rotation);
        q1.conjugate();
        q.mul(q1, q);
    }

}
