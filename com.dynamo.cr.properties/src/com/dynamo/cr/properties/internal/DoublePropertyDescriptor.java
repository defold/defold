package com.dynamo.cr.properties.internal;

import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.views.properties.PropertyDescriptor;


public class DoublePropertyDescriptor extends PropertyDescriptor {

    public DoublePropertyDescriptor(Object id, String displayName) {
        super(id, displayName);
    }

    @Override
    public CellEditor createPropertyEditor(Composite parent) {
        return new DoubleCellEditor(parent);
    }
}