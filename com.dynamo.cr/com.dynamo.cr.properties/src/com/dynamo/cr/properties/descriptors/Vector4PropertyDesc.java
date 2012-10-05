package com.dynamo.cr.properties.descriptors;

import javax.vecmath.Vector4d;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class Vector4PropertyDesc<T, U extends IPropertyObjectWorld> extends ArrayPropertyDesc<Vector4d, T, U> {

    public Vector4PropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory,  4);
    }

    @Override
    protected boolean isComponentEqual(Vector4d v1, Vector4d v2, int component) {
        switch (component) {
        case 0:
            return v1.x == v2.x;
        case 1:
            return v1.y == v2.y;
        case 2:
            return v1.z == v2.z;
        case 3:
            return v1.w == v2.w;
        default:
            assert false;
        }
        return false;
    }

    @Override
    protected double[] valueToArray(Vector4d value) {
        return new double[] {value.x, value.y, value.z, value.w};
    }

}
