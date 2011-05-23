package com.dynamo.cr.guieditor.property;

public interface ICommandFactory<T, U extends IPropertyObjectWorld> {

    public void createCommand(T obj, String property,
            IPropertyAccessor<T, U> accesor,
            Object oldValue, Object newValue, U world);
}
