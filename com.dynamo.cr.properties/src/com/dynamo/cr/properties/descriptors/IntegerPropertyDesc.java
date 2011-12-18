package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class IntegerPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Integer, T, U> {

    public IntegerPropertyDesc(String id, String name) {
        super(id, name);
    }

    @Override
    public Integer fromString(String text) {
        try {
            return Integer.parseInt(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }
}
