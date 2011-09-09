package com.dynamo.cr.properties;

import org.eclipse.swt.widgets.Composite;

public class PropertyDesc<T, U extends IPropertyObjectWorld> implements IPropertyDesc<T, U> {

    private Object id;
    private String name;

    public PropertyDesc(Object id, String name) {
        this.id = id;
        this.name = name;
    }

    @Override
    public IPropertyEditor<T, U> createEditor(Composite parent) {
        return null;
    }

    @Override
    public String getCategory() {
        return null;
    }

    @Override
    public String getDescription() {
        return null;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public Object getId() {
        return id;
    }

}
