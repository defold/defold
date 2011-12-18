package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class FloatPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Float, T, U> {

    public FloatPropertyDesc(String id, String name) {
        super(id, name);
    }

    @Override
    public Float fromString(String text) {
        try {
            return Float.parseFloat(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
