package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;

public class IntegerPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Integer, T, U> {

    public IntegerPropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory, editorType);
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
