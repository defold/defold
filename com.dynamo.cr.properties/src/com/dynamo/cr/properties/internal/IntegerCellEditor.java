package com.dynamo.cr.properties.internal;

import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.swt.widgets.Composite;

class IntegerCellEditor extends TextCellEditor {

    public IntegerCellEditor(Composite parent) {
        super(parent);
    }

    @Override
    protected void doSetValue(Object value) {
        Integer integerValue = (Integer) value;
        super.doSetValue(integerValue.toString());
    }

    @Override
    protected Object doGetValue() {
        // TODO: Tried validator but without luck
        // Null value is handled in EmbeddedPropertySourceProxy#setPropertyValue
        String textValue = (String) super.doGetValue();
        try {
            return Integer.parseInt(textValue);
        } catch (Throwable e) {
            return null;
        }
    }
}
