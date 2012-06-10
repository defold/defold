package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public interface IPropertyDesc<T, U extends IPropertyObjectWorld> {

    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot);

    public String getCategory();

    public String getDescription();

    public String getName();

    public String getId();

}
