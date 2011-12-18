package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class DoublePropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Double, T, U> {

    public DoublePropertyDesc(String id, String name) {
        super(id, name);
    }

    @Override
    public Double fromString(String text) {
        try {
            return Double.parseDouble(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
