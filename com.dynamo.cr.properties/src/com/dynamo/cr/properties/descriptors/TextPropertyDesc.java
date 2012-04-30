package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;

public class TextPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<String, T, U> {

    public TextPropertyDesc(String id, String name, EditorType editorType) {
        super(id, name, editorType);
    }

    @Override
    public String fromString(String text) {
        return text;
    }
}
