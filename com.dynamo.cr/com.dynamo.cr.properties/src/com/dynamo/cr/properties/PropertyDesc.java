package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public class PropertyDesc<T, U extends IPropertyObjectWorld> implements IPropertyDesc<T, U> {

    private String id;
    private String name;

    public PropertyDesc(String id, String name) {
        this.id = id;
        this.name = name;
    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
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
    public String getId() {
        return id;
    }

}
