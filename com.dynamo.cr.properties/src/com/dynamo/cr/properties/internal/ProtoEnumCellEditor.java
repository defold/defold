package com.dynamo.cr.properties.internal;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.ComboBoxCellEditor;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;

import com.google.protobuf.ProtocolMessageEnum;

public class ProtoEnumCellEditor extends ComboBoxCellEditor {
    private Enum<?>[] enumItems;

    public ProtoEnumCellEditor(Composite parent, Enum<?>[] enumItems) {
        super(parent, new String[] {}, SWT.READ_ONLY);
        List<String> itemList = new ArrayList<String>(enumItems.length);
        for (Enum<?> e : enumItems) {
            itemList.add(e.toString());
        }
        setItems(itemList.toArray(new String[itemList.size()]));
        this.enumItems = enumItems;
    }

    @Override
    protected void doSetValue(Object value) {
        ProtocolMessageEnum enumValue = (ProtocolMessageEnum) value;
        super.doSetValue(enumValue.getNumber());
    }

    @Override
    protected Object doGetValue() {
        Object value = super.doGetValue();
        Integer integerValue = (Integer) value;
        Object ret = enumItems[integerValue];
        return ret;
    }
}