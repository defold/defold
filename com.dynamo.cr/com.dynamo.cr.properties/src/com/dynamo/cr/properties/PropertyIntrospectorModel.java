package com.dynamo.cr.properties;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;

public class PropertyIntrospectorModel<T, U extends IPropertyObjectWorld> implements IPropertyModel<T, U> {

    private T object;
    private U world;
    private PropertyIntrospector<T, U> staticIntrospector;
    private DynamicPropertyIntrospector<T, U> dynamicIntrospector;

    public PropertyIntrospectorModel(T source, U world, PropertyIntrospector<T, U> staticIntrospector) {
        this.object = source;
        this.world = world;
        this.staticIntrospector = staticIntrospector;
        this.dynamicIntrospector = new DynamicPropertyIntrospector<T, U>(source, world, this.staticIntrospector);
    }

    @SuppressWarnings({ "unchecked", "rawtypes" })
    @Override
    public IPropertyDesc<T, U>[] getPropertyDescs() {
        List<IPropertyDesc> lst = new ArrayList<IPropertyDesc>(64);
        for (IPropertyDesc<T, U> pd : staticIntrospector.getDescriptors()) {
            lst.add(pd);
        }

        for (IPropertyDesc<T, U> pd : dynamicIntrospector.getDescriptors()) {
            lst.add(pd);
        }

        return lst.toArray(new IPropertyDesc[lst.size()]);
    }

    @Override
    public ICommandFactory<T, U> getCommandFactory() {
        return staticIntrospector.getCommandFactory();
    }

    @Override
    public U getWorld() {
        return world;
    }

    private IPropertyAccessor<T, U> getPropertyAccessor(Object id) {
        IPropertyAccessor<T, U> accessor;
        if (staticIntrospector.hasProperty(id))
            accessor = staticIntrospector.getAccessor();
        else
            accessor = dynamicIntrospector.getAccessor();

        return accessor;
    }

    @Override
    public Object getPropertyValue(Object id) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        Object value = accessor.getValue(object, (String) id, world);
        return value;
    }

    @Override
    public IUndoableOperation setPropertyValue(Object id, Object value) {
        return setPropertyValue(id, value, false);
    }

    @Override
    public IUndoableOperation setPropertyValue(Object id, Object value,
            boolean force) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        Object oldValue = getPropertyValue(id);
        boolean overridden = isPropertyOverridden(id);
        IUndoableOperation operation = staticIntrospector.getCommandFactory().create(object, (String) id, accessor, oldValue, value, overridden, world, force);
        return operation;
    }

    @Override
    public IUndoableOperation resetPropertyValue(Object id) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        Object oldValue = getPropertyValue(id);
        IUndoableOperation operation = staticIntrospector.getCommandFactory().createReset(object, (String) id, accessor, oldValue, world);
        return operation;
    }

    @Override
    public boolean isPropertyEditable(Object id) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        return accessor.isEditable(object, (String) id, world);
    }

    @Override
    public boolean isPropertyVisible(Object id) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        return accessor.isVisible(object, (String) id, world);
    }

    @Override
    public boolean isPropertyOverridden(Object id) {
        IPropertyAccessor<T, U> accessor = getPropertyAccessor(id);
        return accessor.isOverridden(object, (String) id, world);
    }

    @Override
    public IStatus getStatus() {
        IStatus staticStatus = staticIntrospector.getObjectStatus(object, world);
        IStatus dynamicStatus = dynamicIntrospector.getObjectStatus(object, world);
        MultiStatus status = new MultiStatus("com.dynamo.cr.properties", IStatus.OK, null, null);

        if (!staticStatus.isOK())
            status.merge(staticStatus);
        if (!dynamicStatus.isOK())
            status.merge(dynamicStatus);

        return status;
    }

    @Override
    public IStatus getPropertyStatus(Object id) {
        if (staticIntrospector.hasProperty(id))
            return staticIntrospector.getPropertyStatus(object, world, id);
        else
            return dynamicIntrospector.getPropertyStatus(object, world, id);
    }

    @Override
    public Object[] getPropertyOptions(String id) {
        if (staticIntrospector.hasProperty(id))
            return staticIntrospector.getPropertyOptions(object, world, id);
        else
            return dynamicIntrospector.getPropertyOptions(object, world, id);
    }
}

