package com.dynamo.cr.properties;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

public class BeanPropertyAccessor implements IPropertyAccessor<Object, IPropertyObjectWorld> {

    private static class Methods {
        PropertyDescriptor propertyDescriptor;
        Method isEditable;
        Method isVisible;
        Method isOverridden;
        Method getOptions;
        Method reset;
    }

    private Map<String, Methods> methodsMap = new HashMap<String, Methods>();
    private Class<?> cachedForClass = null;

    private Methods init(Object obj, String property) {

        if (cachedForClass != null && cachedForClass != obj.getClass()) {
            System.err.println("WARNING: Flushing BeanPropertyAccessor class. Accessor used with two different classes");
            // Flush cache
            methodsMap = new HashMap<String, BeanPropertyAccessor.Methods>();
        }

        Methods methods = methodsMap.get(property);
        if (methods != null) {
            // Return already cached
            return methods;
        }

        methods = new Methods();
        methodsMap.put(property, methods);
        // Store the class we cache for
        cachedForClass = obj.getClass();

        try {
            BeanInfo beanInfo = Introspector.getBeanInfo(obj.getClass());
            PropertyDescriptor[] propertyDescriptors = beanInfo.getPropertyDescriptors();
            for (PropertyDescriptor propertyDescriptor : propertyDescriptors) {
                if (propertyDescriptor.getName().equals(property)) {
                    methods.propertyDescriptor = propertyDescriptor;
                }
            }
        } catch (IntrospectionException e) {
            throw new RuntimeException(e);
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            methods.isEditable = obj.getClass().getMethod(String.format("is%sEditable", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            methods.isVisible = obj.getClass().getMethod(String.format("is%sVisible", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            methods.isOverridden = obj.getClass().getMethod(String.format("is%sOverridden", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            methods.getOptions = obj.getClass().getMethod(String.format("get%sOptions", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            methods.reset = obj.getClass().getMethod(String.format("reset%s", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        return methods;
    }

    @Override
    public void setValue(Object obj, String property, Object newValue,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        if (methods.propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing setter for %s#%s", obj.getClass().getName(), property));
        }

        Object oldValue;
        try {
            oldValue = methods.propertyDescriptor.getReadMethod().invoke(obj);
            Object mergedValue = PropertyUtil.mergeValue(oldValue, newValue);
            methods.propertyDescriptor.getWriteMethod().invoke(obj, mergedValue);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public Object getValue(Object obj, String property,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        if (methods.propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing getter for %s#%s", obj.getClass().getName(), property));
        }
        try {
            return methods.propertyDescriptor.getReadMethod().invoke(obj);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public boolean isEditable(Object obj, String property,
            IPropertyObjectWorld world) {

        Methods methods = init(obj, property);
        boolean editable = true;
        try {
            editable = methods.isEditable == null || (Boolean) methods.isEditable.invoke(obj);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        return editable;
    }

    @Override
    public boolean isVisible(Object obj, String property,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        boolean visible = true;
        try {
            visible = methods.isVisible == null || (Boolean) methods.isVisible.invoke(obj);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        return visible;
    }

    @Override
    public boolean isOverridden(Object obj, String property,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        boolean overridden = false;
        try {
            if(methods.isOverridden != null) {
                overridden = methods.isOverridden != null && (Boolean) methods.isOverridden.invoke(obj);
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        return overridden;
    }

    @Override
    public Object[] getPropertyOptions(Object obj, String property,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        if (methods.getOptions != null) {
            try {
                return (Object[]) methods.getOptions.invoke(obj);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            return new Object[] {};
        }
    }

    @Override
    public void resetValue(Object obj, String property,
            IPropertyObjectWorld world) {
        Methods methods = init(obj, property);
        try {
            if(methods.reset != null) {
                methods.reset.invoke(obj);
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

}
