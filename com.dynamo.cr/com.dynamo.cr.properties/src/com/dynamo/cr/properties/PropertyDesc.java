package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public class PropertyDesc<T, U extends IPropertyObjectWorld> implements IPropertyDesc<T, U> {

    private String id;
    private String name;
    private String category;

    public PropertyDesc(String id, String name, String category) {
        this.id = id;
        this.name = name;
        this.category = category;
    }

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
        return null;
    }

    @Override
    public String getCategory() {
        return category;
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
