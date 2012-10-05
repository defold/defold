package com.dynamo.cr.properties.descriptors;

import javax.vecmath.Vector3d;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class Vector3PropertyDesc<T, U extends IPropertyObjectWorld> extends ArrayPropertyDesc<Vector3d, T, U> {

    public Vector3PropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory, 3);
    }

    @Override
    protected boolean isComponentEqual(Vector3d v1, Vector3d v2, int component) {
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
    protected double[] valueToArray(Vector3d value) {
        return new double[] {value.x, value.y, value.z};
    }

}
