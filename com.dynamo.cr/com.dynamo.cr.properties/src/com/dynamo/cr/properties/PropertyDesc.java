package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public class PropertyDesc<T, U extends IPropertyObjectWorld> implements IPropertyDesc<T, U> {

    private String id;
    private String name;
    private String category;
    private Double min = -Double.MAX_VALUE;
    private Double max = Double.MAX_VALUE;

    public PropertyDesc(String id, String name, String category) {
        this.id = id;
        this.name = name;

        if (category == null) {
            throw new IllegalArgumentException(String.format("Category must not be null (id: '%s')", id));
        }

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

    @Override
    public double getMin() {
        return min;
    }

    @Override
    public void setMin(double min) {
        this.min = min;
    }

    @Override
    public double getMax() {
        return max;
    }

    @Override
    public void setMax(double max) {
        this.max = max;
    }

}
