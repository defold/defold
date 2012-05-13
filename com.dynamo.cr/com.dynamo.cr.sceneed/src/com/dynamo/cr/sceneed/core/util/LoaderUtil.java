package com.dynamo.cr.sceneed.core.util;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class LoaderUtil {

    public static Vector4d toVector4(Point3 p) {
        return new Vector4d(p.getX(), p.getY(), p.getZ(), 1);
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

    public static <U extends Message> void loadBuilder(U.Builder builder, InputStream stream) throws IOException {
        InputStreamReader reader = new InputStreamReader(stream);
        TextFormat.merge(reader, builder);
    }
}
