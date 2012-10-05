package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;

public class FloatPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Float, T, U> {

    public FloatPropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory, editorType);
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
