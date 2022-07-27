// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.util;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Matrix4f;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Quat4f;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector3f;
import javax.vecmath.Vector4d;

import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Matrix4;
import com.dynamo.proto.DdfMath.Transform;

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

    public static Transform vecmathToDDF(Vector3d t, Quat4d r, Vector3d s) {
        Transform.Builder b = Transform.newBuilder();
        return b.setTranslation(vecmathToDDF(t)).setRotation(vecmathToDDF(r)).setScale(vecmathToDDF(s)).build();
    }

    public static Matrix4 vecmathToDDF(Matrix4d m) {
        Matrix4.Builder b = Matrix4.newBuilder();
        b.setM00((float)m.m00); b.setM01((float)m.m01); b.setM02((float)m.m02); b.setM03((float)m.m03);
        b.setM10((float)m.m10); b.setM11((float)m.m11); b.setM12((float)m.m12); b.setM13((float)m.m13);
        b.setM20((float)m.m20); b.setM21((float)m.m21); b.setM22((float)m.m22); b.setM23((float)m.m23);
        b.setM30((float)m.m30); b.setM31((float)m.m31); b.setM32((float)m.m32); b.setM33((float)m.m33);
        return b.build();
    }

    public static Transform vecmathIdentityTransform() {
        Transform.Builder b = Transform.newBuilder();
        return b.setTranslation(vecmathToDDF(new Vector3d(0, 0, 0))).setRotation(vecmathToDDF(new Quat4d(0, 0, 0, 1))).setScale(vecmathToDDF(new Vector3d(1,1,1))).build();
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
     * @param mv2 Matrix4f of vecmath2 flavor
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

    /**
     * Convert a Matrix4f between different vecmath versions.
     * @param mv1 Matrix4f of vecmath1 flavor
     * @return A new vecmath2 Matrix4f
     */
    public static org.openmali.vecmath2.Matrix4f vecmath1ToVecmath2(Matrix4f mv1) {
        org.openmali.vecmath2.Matrix4f mv2 = new org.openmali.vecmath2.Matrix4f();
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                mv2.set(r, c, mv1.getElement(r, c));
            }
        }
        return mv2;
    }

    // Get quaternion rotation from a matrix
    // From: http://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
    public static void quaternionFromMatrix(Matrix3d m, Quat4d r)
    {
        double tr = m.m00 + m.m11 + m.m22;

        if (tr > 0.0) {
            double S = Math.sqrt(tr + 1.0) * 2.0; // S=4*rotation.w
            r.w = 0.25 * S;
            r.x = (m.m21 - m.m12) / S;
            r.y = (m.m02 - m.m20) / S;
            r.z = (m.m10 - m.m01) / S;
        } else if ((m.m00 > m.m11)&(m.m00 > m.m22)) {
            double S = Math.sqrt(1.0 + m.m00 - m.m11 - m.m22) * 2.0; // S=4*rotation.x
            r.w = (m.m21 - m.m12) / S;
            r.x = 0.25 * S;
            r.y = (m.m01 + m.m10) / S;
            r.z = (m.m02 + m.m20) / S;
        } else if (m.m11 > m.m22) {
            double S = Math.sqrt(1.0 + m.m11 - m.m00 - m.m22) * 2.0; // S=4*rotation.y
            r.w = (m.m02 - m.m20) / S;
            r.x = (m.m01 + m.m10) / S;
            r.y = 0.25 * S;
            r.z = (m.m12 + m.m21) / S;
        } else {
            double S = Math.sqrt(1.0 + m.m22 - m.m00 - m.m11) * 2.0; // S=4*rotation.z
            r.w = (m.m10 - m.m01) / S;
            r.x = (m.m02 + m.m20) / S;
            r.y = (m.m12 + m.m21) / S;
            r.z = 0.25 * S;
        }
    }

    public static void decompose(Matrix4d m, Vector3d p, Quat4d r, Vector3d s) {
        m.get(p);

        // Get rotation and scale sub matrix
        Matrix3d rotScale = new Matrix3d();
        m.getRotationScale(rotScale);

        Vector3d rsColumns[] = new Vector3d[]{ new Vector3d(), new Vector3d(), new Vector3d() };
        rotScale.getColumn(0, rsColumns[0]);
        rotScale.getColumn(1, rsColumns[1]);
        rotScale.getColumn(2, rsColumns[2]);

        // Extract the scaling factors
        s.setX(rsColumns[0].length());
        s.setY(rsColumns[1].length());
        s.setZ(rsColumns[2].length());

        Matrix3d rotmat = new Matrix3d(m.m00 / s.x, m.m01 / s.y, m.m02 / s.z,
                                       m.m10 / s.x, m.m11 / s.y, m.m12 / s.z,
                                       m.m20 / s.x, m.m21 / s.y, m.m22 / s.z);

        // Get quaternion from rotation matrix
        quaternionFromMatrix(rotmat, r);
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
