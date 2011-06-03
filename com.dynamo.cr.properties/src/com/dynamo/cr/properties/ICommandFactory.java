package com.dynamo.cr.properties;

public interface ICommandFactory<T, U extends IPropertyObjectWorld> {

    public void createCommand(T obj, String property,
            IPropertyAccessor<T, U> accessor,
            Object oldValue, Object newValue, U world);
}
