package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public class TextPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<String, T, U> {

    public TextPropertyDesc(String id, String name) {
        super(id, name);
    }

    @Override
    public String fromString(String text) {
        return text;
    }
}
