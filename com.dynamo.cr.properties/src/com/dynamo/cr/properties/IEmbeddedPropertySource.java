package com.dynamo.cr.properties;

import org.eclipse.ui.views.properties.IPropertyDescriptor;

public interface IEmbeddedPropertySource<T> {
    public IPropertyDescriptor[] getPropertyDescriptors();
    public Object getPropertyValue(T object, Object id);
    public void setPropertyValue(T object, Object id, Object value);
}
