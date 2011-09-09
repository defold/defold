package com.dynamo.cr.properties;

import org.eclipse.swt.widgets.Composite;

public interface IPropertyDesc<T, U extends IPropertyObjectWorld> {

    public IPropertyEditor<T, U> createEditor(Composite parent);

    public String getCategory();

    public String getDescription();

    public String getName();

    public Object getId();

}
