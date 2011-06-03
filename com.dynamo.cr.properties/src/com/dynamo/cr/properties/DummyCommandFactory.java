package com.dynamo.cr.properties;

public class DummyCommandFactory implements ICommandFactory<Object, IPropertyObjectWorld> {

    @Override
    public void createCommand(Object obj, String property,
            IPropertyAccessor<Object, IPropertyObjectWorld> accesor,
            Object oldValue, Object newValue, IPropertyObjectWorld world) {
    }

}
