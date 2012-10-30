package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.util.NumberUtil;

public class FloatPropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Float, T, U> {

    public FloatPropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory, editorType);
    }

    @Override
    public Float fromString(String text) {
        try {
            return NumberUtil.parseFloat(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    @Override
    public String scalarToString(Float value) {
        return NumberUtil.formatFloat(value);
    }

    @Override
    public Class<?> getTypeClass() {
        return Float.class;
    }

}
