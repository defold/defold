package com.dynamo.bob.util;

import java.nio.FloatBuffer;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix3f;
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

    /**
     * Convert a Matrix4f between different vecmath versions.
     * @param m Matrix4f of vecmath2 flavor
     * @return A new vecmath1 Matrix4f
     */
    public static Matrix4f vecmath2ToVecmath1(org.openmali.vecmath2.Matrix4f mv2) {
        Matrix4f mv1 = new Matrix4f();
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                mv1.setElement(r, c, mv2.get(r, c));
            }
        }
        return mv1;
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

    public static void decompose(Matrix4d m, Vector3d p, Quat4d r, Vector3d s) {
        m.get(p);

        // get rotation and scale sub matrix 
        Matrix3d rotScale = new Matrix3d();
        m.getRotationScale(rotScale);

        Vector3d rsColumns[] = new Vector3d[]{ new Vector3d(), new Vector3d(), new Vector3d() };
        rotScale.getColumn(0, rsColumns[0]);
        rotScale.getColumn(1, rsColumns[1]);
        rotScale.getColumn(2, rsColumns[2]);

        // extract the scaling factors
        s.setX(rsColumns[0].length());
        s.setY(rsColumns[1].length());
        s.setZ(rsColumns[2].length());

        // TODO Figure out how to handle negative scaling correctly.
        // if (rotScale.determinant() < 0) {
        //     s.scale(-1.0);
        // }

        // undo any scaling on the rotscale matrix
        if(s.getX() != 0.0) {
            rsColumns[0].scale(1.0 / s.getX());
        }
        if(s.getY() != 0.0) {
            rsColumns[1].scale(1.0 / s.getY());
        }
        if(s.getZ() != 0.0) {
            rsColumns[2].scale(1.0 / s.getZ());
        }

        r.set(rotScale);
    }

    public static void decompose(Matrix4f m, Vector3f p, Quat4f r, Vector3f s) {
        Vector3d dP = new Vector3d();
        Quat4d dR = new Quat4d();
        Vector3d dS = new Vector3d();
        decompose(new Matrix4d(m), dP, dR, dS);
        p.set(dP);
        r.set(dR);
        s.set(dS);
    }
}
