package com.dynamo.cr.properties.descriptors;

import javax.vecmath.Quat4d;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class Quat4PropertyDesc<T, U extends IPropertyObjectWorld> extends ArrayPropertyDesc<Quat4d, T, U> {

    public Quat4PropertyDesc(String id, String name, String catgory) {
        super(id, name, catgory, 4);
    }

    @Override
    protected boolean isComponentEqual(Quat4d v1, Quat4d v2, int component) {
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
    protected double[] valueToArray(Quat4d value) {
        return new double[] {value.x, value.y, value.z, value.w};
    }

}
