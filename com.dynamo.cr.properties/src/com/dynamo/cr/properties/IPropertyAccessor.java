package com.dynamo.cr.properties;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.runtime.IStatus;

public interface IPropertyAccessor<T, U extends IPropertyObjectWorld> {
    public void setValue(T obj, String property, Object value, U world) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException;
    public Object getValue(T obj, String property, U world) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException;
    public IStatus getStatus(T obj, String property, U world) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException;
}
