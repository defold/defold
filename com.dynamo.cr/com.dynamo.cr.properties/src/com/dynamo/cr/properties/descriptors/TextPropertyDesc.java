package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;

public class TextPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<String, T, U> {

    public TextPropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory, editorType);
    }

    @Override
    public String fromString(String text) {
        return text;
    }

    @Override
    public String scalarToString(String value) {
        return value;
    }

    @Override
    public Class<?> getTypeClass() {
        return String.class;
    }

}
