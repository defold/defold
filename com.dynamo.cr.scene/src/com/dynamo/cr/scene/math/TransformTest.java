package com.dynamo.cr.scene.math;
/*
import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.junit.Test;
*/
public class TransformTest
{
/*
    @Test
    public final void testMul1()
    {
        Transform t1 = new Transform();
        AxisAngle4d rot1 = new AxisAngle4d(1, 0, 0, 2);
        Transform t2 = new Transform();
        AxisAngle4d rot2 = new AxisAngle4d(0, 1, 0, 1);

        t1.setRotation(rot1);
        t1.setTranslation(1, 2, 3);
        t2.setRotation(rot2);
        t2.setTranslation(4, 7, 5);
        Transform t = new Transform();
        t.mul(t1, t2);

        System.out.println(t);

        Matrix4d m1 = new Matrix4d();
        m1.set(rot1);
        m1.setColumn(3, 1, 2, 3, 1);
        Matrix4d m2 = new Matrix4d();
        m2.set(rot2);
        m2.setColumn(3, 4, 7, 5, 1);

        Matrix4d m = new Matrix4d();
        m.mul(m1, m2);
        Quat4d q = new Quat4d();
        q.set(m);
        System.out.print(q);
        System.out.print(",");
        Vector4d tt = new Vector4d();
        m.getColumn(3, tt);
        System.out.println(tt);

        //System.out.println(m);
    }

    @Test
    public void testInvert()
    {
        System.out.println("testInvert");
        Transform t = new Transform();
        t.setRotation(new AxisAngle4d(1,0,0,2));
        t.setTranslation(1, 5, 3);

        Transform t_inv = new Transform(t);
        t_inv.invert();

        Transform tmp = new Transform();
        tmp.mul(t, t_inv);
        System.out.println(tmp);

    }
    */

}
