package com.dynamo.cr.properties.internal;

import org.eclipse.ui.views.properties.IPropertyDescriptor;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IEmbeddedPropertySource;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;

public class EmbeddedPropertySourceProxy<T, U extends IPropertyObjectWorld, V> implements IPropertySource {
    private IEmbeddedPropertySource<V> embeddedPropertySource;
    private T parent;
    private String property;
    private IPropertyAccessor<T, U> accessor;
    private ICommandFactory<T, U> commandFactory;
    private U world;

    public EmbeddedPropertySourceProxy(
            IEmbeddedPropertySource<V> embeddedPropertySource,
            T parent, String property, IPropertyAccessor<T, U> accessor,
            ICommandFactory<T, U> commandFactory, U world) {

        this.embeddedPropertySource = embeddedPropertySource;
        this.parent = parent;
        this.property = property;
        this.accessor = accessor;
        this.commandFactory = commandFactory;
        this.world = world;
    }

    @Override
    public Object getEditableValue() {
        try {
            return this.accessor.getValue(parent, property, world);
        } catch (Throwable e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public IPropertyDescriptor[] getPropertyDescriptors() {
        return embeddedPropertySource.getPropertyDescriptors();
    }

    @Override
    public Object getPropertyValue(Object id) {
        try {
            @SuppressWarnings("unchecked")
            V object = (V) this.accessor.getValue(parent, property, world);
            return embeddedPropertySource.getPropertyValue(object, id);
        } catch (Throwable e) {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    public boolean isPropertySet(Object id) {
        return false;
    }

    @Override
    public void resetPropertyValue(Object id) {
    }

    @SuppressWarnings("unchecked")
    @Override
    public void setPropertyValue(Object id, Object value) {
        try {
            Object oldValue = this.accessor.getValue(parent, property, world);
            Object newValue = this.accessor.getValue(parent, property, world);
            embeddedPropertySource.setPropertyValue((V) newValue, id, value);
            this.commandFactory.createCommand(parent, property, this.accessor, oldValue, newValue, world);

        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

}
