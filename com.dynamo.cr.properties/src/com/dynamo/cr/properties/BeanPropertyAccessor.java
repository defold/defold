package com.dynamo.cr.properties;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.InvocationTargetException;

public class BeanPropertyAccessor implements IPropertyAccessor<Object, IPropertyObjectWorld> {

    private PropertyDescriptor propertyDescriptor;

    private void init(Object obj, String property) {

        if (propertyDescriptor != null && propertyDescriptor.getName().equals(property))
            return;

        try {
            BeanInfo beanInfo = Introspector.getBeanInfo(obj.getClass());
            PropertyDescriptor[] propertyDescriptors = beanInfo.getPropertyDescriptors();
            for (PropertyDescriptor propertyDescriptor : propertyDescriptors) {
                if (propertyDescriptor.getName().equals(property)) {
                    this.propertyDescriptor = propertyDescriptor;
                }
            }
        } catch (IntrospectionException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void setValue(Object obj, String property, Object value,
            IPropertyObjectWorld world)
                    throws IllegalArgumentException, IllegalAccessException,
                    InvocationTargetException {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing setter for %s#%s", obj.getClass().getName(), property));
        }
        propertyDescriptor.getWriteMethod().invoke(obj, value);

    }

    @Override
    public Object getValue(Object obj, String property,
            IPropertyObjectWorld world)
                    throws IllegalArgumentException, IllegalAccessException,
                    InvocationTargetException {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing getter for %s#%s", obj.getClass().getName(), property));
        }
        return propertyDescriptor.getReadMethod().invoke(obj);
    }
}
