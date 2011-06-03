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
        String textValue = (String) super.doGetValue();
        return Double.parseDouble(textValue);
    }
}