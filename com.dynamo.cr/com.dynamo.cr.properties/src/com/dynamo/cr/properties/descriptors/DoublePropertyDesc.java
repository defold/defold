package com.dynamo.cr.properties.descriptors;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.util.NumberUtil;

public class DoublePropertyDesc<T, U extends IPropertyObjectWorld> extends ScalarPropertyDesc<Double, T, U> {

    public DoublePropertyDesc(String id, String name, String catgory, EditorType editorType) {
        super(id, name, catgory, editorType);
    }

    @Override
    public Double fromString(String text) {
        try {
            return NumberUtil.parseDouble(text);
        } catch (NumberFormatException e) {
            return null;
        }
    }

    @Override
    public String scalarToString(Double value) {
        return NumberUtil.formatDouble(value);
    }

    @Override
    public Class<?> getTypeClass() {
        return Double.class;
    }

}
