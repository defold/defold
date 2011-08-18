package com.dynamo.cr.properties.internal;

import org.eclipse.jface.viewers.TextCellEditor;
import org.eclipse.swt.widgets.Composite;

public class DoubleCellEditor extends TextCellEditor {

    public DoubleCellEditor(Composite parent) {
        super(parent);
    }

    @Override
    protected void doSetValue(Object value) {
        Double integerValue = (Double) value;
        super.doSetValue(integerValue.toString());
    }

    @Override
    protected Object doGetValue() {
        // TODO: Tried validator but without luck
        // Null value is handled in EmbeddedPropertySourceProxy#setPropertyValue
        String textValue = (String) super.doGetValue();
        try {
            return Double.parseDouble(textValue);
        } catch (Throwable e) {
            return null;
        }
    }
}
