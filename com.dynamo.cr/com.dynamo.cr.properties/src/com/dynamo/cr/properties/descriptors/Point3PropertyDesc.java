package com.dynamo.cr.properties.descriptors;

import javax.vecmath.Point3d;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class Point3PropertyDesc<T, U extends IPropertyObjectWorld> extends ArrayPropertyDesc<Point3d, T, U> {

    public Point3PropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory, 3);
    }

    @Override
    protected boolean isComponentEqual(Point3d v1, Point3d v2, int component) {
        switch (component) {
        case 0:
            return v1.x == v2.x;
        case 1:
            return v1.y == v2.y;
        case 2:
            return v1.z == v2.z;
        default:
            assert false;
        }
        return false;
    }

    @Override
    protected double[] valueToArray(Point3d value) {
        return new double[] {value.x, value.y, value.z};
    }

}
