package com.dynamo.bob.util;

import java.nio.FloatBuffer;

import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3d;
import javax.vecmath.Point3f;
import javax.vecmath.Quat4d;
import javax.vecmath.Quat4f;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;
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

    public static Matrix4f vecmath2ToVecmath1(org.openmali.vecmath2.Matrix4f m) {
        float[] values = new float[16];
        FloatBuffer b = FloatBuffer.wrap(values);
        m.writeToBuffer(b, false, false);
        return new Matrix4f(values);
    }

    public static Matrix4d floatToDouble(Matrix4f mf) {
        Matrix4d md = new Matrix4d();
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                md.setElement(r, c, mf.getElement(r, c));
            }
        }
        return md;
    }

    public static void decompose(Matrix4d m, Point3d p, Quat4d r, Vector3d s) {
        // TODO FIX ME!
    }

    public static void decompose(Matrix4f m, Point3f p, Quat4f r, Vector3f s) {
        // TODO FIX ME!

        /*
        // extract translation
        Vector3f pv = new Vector3f();
        m.get(pv);
        p.set(pv);

        // extract the rows of the matrix
        Matrix3f rotScale = new Matrix3f();
        m.getRotationScale(rotScale);

        Vector3f vRows[] = new Vector3f[3];{
                new Vector3f(mat.get(0,0),mat.get(1,0),mat.get(2,0)),
                new Vector3f(mat.get(0,1),mat.get(1,1),mat.get(2,1)),
                new Vector3f(mat.get(0,2),mat.get(1,2),mat.get(2,2))
        };

        // extract the scaling factors
        scaling.setX(rotScale.get);
        scaling.setY(vRows[1].length());
        scaling.setZ(vRows[2].length());

        // and the sign of the scaling
        // TODO This can't possibly be correct? It has to be possible for one dimension to be negative with the others positive?
        if (rotScale.determinant() < 0) {
            scaling.setX(-scaling.getX());
            scaling.setY(-scaling.getY());
            scaling.setZ(-scaling.getZ());
        }

        // and remove all scaling from the matrix
        if(scaling.getX() != 0.0f)
        {
            vRows[0] = new Vector3f(vRows[0].div(scaling.getX()));
        }
        if(scaling.getY() != 0.0f)
        {
            vRows[1] = new Vector3f(vRows[1].div(scaling.getY()));
        }
        if(scaling.getZ() != 0.0f)
        {
            vRows[2] = new Vector3f(vRows[2].div(scaling.getZ()));
        }

        // build a 3x3 rotation matrix
        Matrix3f rotmat = new Matrix3f(vRows[0].getX(),vRows[1].getX(),vRows[2].getX(),
                                    vRows[0].getY(),vRows[1].getY(),vRows[2].getY(),
                                    vRows[0].getZ(),vRows[1].getZ(),vRows[2].getZ());

        // and generate the rotation quaternion from it
        rotation.set(rotmat);
        */
    }

}
