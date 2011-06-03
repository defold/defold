package com.dynamo.cr.properties.internal;

import java.lang.reflect.Method;

import org.eclipse.jface.viewers.CellEditor;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.views.properties.PropertyDescriptor;

import com.google.protobuf.ProtocolMessageEnum;

public class ProtoEnumDescriptor extends PropertyDescriptor {
    private Enum<?>[] enumValues;

    public ProtoEnumDescriptor(Class<? extends ProtocolMessageEnum> type, String id, String displayName) {
        super(id, displayName);

        try {
            Method values = type.getMethod("values");
            this.enumValues = (Enum<?>[]) values.invoke(null);

        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public CellEditor createPropertyEditor(Composite parent) {
        ProtoEnumCellEditor editor = new ProtoEnumCellEditor(parent, enumValues);
        return editor;
    }
}