package com.dynamo.cr.properties;

import org.eclipse.swt.widgets.Control;

public interface IPropertyEditor<T, U extends IPropertyObjectWorld> {

    public Control getControl();

    public void setModels(IPropertyModel<T, U>[] models);

    public void refresh();

}
