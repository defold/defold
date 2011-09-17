package com.dynamo.cr.properties;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;

public class PropertyIntrospectorModel<T, U extends IPropertyObjectWorld> implements IPropertyModel<T, U> {

    private T object;
    private U world;
    private PropertyIntrospector<T, U> introspector;

    public PropertyIntrospectorModel(T source, U world, PropertyIntrospector<T, U> introspector) {
        this.object = source;
        this.world = world;
        this.introspector = introspector;
    }

    @Override
    public IPropertyDesc<T, U>[] getPropertyDescs() {
        return introspector.getDescriptors();
    }

    @Override
    public ICommandFactory<T, U> getCommandFactory() {
        return introspector.getCommandFactory();
    }

    @Override
    public U getWorld() {
        return world;
    }

    @Override
    public Object getPropertyValue(Object id) {
        return introspector.getPropertyValue(object, world, id);
    }

    @Override
    public IUndoableOperation setPropertyValue(Object id, Object value) {
        return introspector.setPropertyValue(object, world, id, value);
    }

    @Override
    public IStatus getPropertyStatus(Object id) {
        return introspector.getPropertyStatus(object, world, id);
    }

    @Override
    public boolean isOk() {
        return introspector.isOk(object, world);
    }

}
