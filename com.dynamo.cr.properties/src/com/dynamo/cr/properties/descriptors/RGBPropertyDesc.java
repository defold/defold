package com.dynamo.cr.properties.descriptors;

import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class RGBPropertyDesc<T, U extends IPropertyObjectWorld> extends ArrayPropertyDesc<RGB, T, U> {

    public RGBPropertyDesc(Object id, String name) {
        super(id, name, 3);
    }

    @Override
    protected boolean isComponentEqual(RGB v1, RGB v2, int component) {
        switch (component) {
        case 0:
            return v1.red == v2.red;
        case 1:
            return v1.green == v2.green;
        case 2:
            return v1.blue == v2.blue;
        default:
            assert false;
        }
        return false;
    }

    @Override
    protected double[] valueToArray(RGB value) {
        return new double[] {value.red, value.green, value.blue};
    }

}
