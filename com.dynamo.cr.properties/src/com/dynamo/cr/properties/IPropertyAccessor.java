package com.dynamo.cr.properties;


public interface IPropertyAccessor<T, U extends IPropertyObjectWorld> {
    public void setValue(T obj, String property, Object value, U world);
    public Object getValue(T obj, String property, U world);
    public boolean isEditable(T obj, String property, U world);
    public boolean isVisible(T obj, String property, U world);
}
