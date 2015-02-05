package com.dynamo.cr.sceneed.core.util;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import javax.vecmath.Point3d;
import javax.vecmath.Point4d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.graphics.RGB;

import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.proto.DdfMath.Vector4;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class LoaderUtil {

    public static Vector4d toVector4(Point3 p) {
        return new Vector4d(p.getX(), p.getY(), p.getZ(), 1);
    }

    public static Vector4d toVector4(Vector4 p) {
        return new Vector4d(p.getX(), p.getY(), p.getZ(), p.getW());
    }

    public static Vector3d toVector3(Vector3 p) {
        return new Vector3d(p.getX(), p.getY(), p.getZ());
    }

    public static Vector3d toVector3(Vector4 p) {
        return new Vector3d(p.getX(), p.getY(), p.getZ());
    }

    public static Quat4d toQuat4(Quat q) {
        return new Quat4d(q.getX(), q.getY(), q.getZ(), q.getW());
    }

    public static Point3 toPoint3(Vector4d v) {
        return Point3.newBuilder()
            .setX((float) v.getX())
            .setY((float) v.getY())
            .setZ((float) v.getZ()).build();
    }

    public static Point3 toPoint3(Point3d v) {
        return Point3.newBuilder()
            .setX((float) v.getX())
            .setY((float) v.getY())
            .setZ((float) v.getZ()).build();
    }

    public static Vector3 toVector3(Vector3d p) {
        return Vector3.newBuilder().setX((float) p.getX()).setY((float) p.getY()).setZ((float) p.getZ()).build();
    }

    public static Vector4 toVector4(RGB rgb, double alpha) {
        float factor = 1.0f / 255.0f;
        return Vector4.newBuilder().setX(rgb.red * factor).setY(rgb.green * factor).setZ(rgb.blue * factor)
                .setW((float) alpha).build();
    }

    public static Vector4 toVector4(Point3d p) {
        return Vector4.newBuilder().setX((float) p.getX()).setY((float) p.getY()).setZ((float) p.getZ()).setW(1.0f)
                .build();
    }

    public static Vector4 toVector4(Vector3d p) {
        return Vector4.newBuilder().setX((float) p.getX()).setY((float) p.getY()).setZ((float) p.getZ()).setW(1.0f)
                .build();
    }

    public static Vector4 toVector4(Vector4d p) {
        return Vector4.newBuilder().setX((float) p.getX()).setY((float) p.getY()).setZ((float) p.getZ()).setW((float) p.getW())
                .build();
    }

    public static Quat toQuat(Quat4d q) {
        return Quat.newBuilder()
            .setX((float) q.getX())
            .setY((float) q.getY())
            .setZ((float) q.getZ())
            .setW((float) q.getW()).build();
    }

    public static Point3d toPoint3d(Point3 p) {
        return new Point3d(p.getX(), p.getY(), p.getZ());
    }

    public static Point3d toPoint3d(Vector4 p) {
        return new Point3d(p.getX(), p.getY(), p.getZ());
    }

    public static Point4d toPoint4d(Vector4 p) {
        return new Point4d(p.getX(), p.getY(), p.getZ(), p.getW());
    }

    public static RGB toRGB(Vector4 v) {
        float factor = 255.0f;
        return new RGB((int) (v.getX() * factor), (int) (v.getY() * factor), (int) (v.getZ() * factor));
    }

    public static <U extends Message> void loadBuilder(U.Builder builder, InputStream stream) throws IOException {
        InputStreamReader reader = new InputStreamReader(stream);
        TextFormat.merge(reader, builder);
    }
}
