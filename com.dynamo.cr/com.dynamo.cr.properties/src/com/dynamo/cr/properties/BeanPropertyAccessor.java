package com.dynamo.cr.properties;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.types.ValueSpread;

public class BeanPropertyAccessor implements IPropertyAccessor<Object, IPropertyObjectWorld> {

    private static class Methods {
        PropertyDescriptor propertyDescriptor;
        Method isEditable;
        Method isVisible;
        Method getOptions;
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
            methods.getOptions = obj.getClass().getMethod(String.format("get%sOptions", capProperty));
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
            // TODO: This merging is rather static. We should perhaps
            // make it more dynamic in the future.
            if (oldValue instanceof Vector4d && newValue instanceof Double[]) {
                mergeVector4dValue(methods, obj, (Vector4d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Vector3d && newValue instanceof Double[]) {
                mergeVector3dValue(methods, obj, (Vector3d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Point3d && newValue instanceof Double[]) {
                mergePoint3dValue(methods, obj, (Point3d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Quat4d && newValue instanceof Double[]) {
                mergeQuat4dValue(methods, obj, (Quat4d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof RGB && newValue instanceof Double[]) {
                mergeRGBValue(methods, obj, (RGB) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof ValueSpread && newValue instanceof Double[]) {
                mergeValueSpread(methods, obj, (ValueSpread) oldValue, (Double[]) newValue);
            }
            else {
                // Generic set
                methods.propertyDescriptor.getWriteMethod().invoke(obj, newValue);
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private void mergeVector4dValue(Methods methods, Object obj, Vector4d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Vector4d toSet = new Vector4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeVector3dValue(Methods methods, Object obj, Vector3d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Vector3d toSet = new Vector3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergePoint3dValue(Methods methods, Object obj, Point3d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Point3d toSet = new Point3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeQuat4dValue(Methods methods, Object obj, Quat4d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Quat4d toSet = new Quat4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeRGBValue(Methods methods, Object obj, RGB oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        RGB toSet = new RGB(0, 0, 0);

        toSet.red = (int) (delta[0] != null ? delta[0] : oldValue.red);
        toSet.green = (int) (delta[1] != null ? delta[1] : oldValue.green);
        toSet.blue = (int) (delta[2] != null ? delta[2] : oldValue.blue);

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeValueSpread(Methods methods, Object obj,
            ValueSpread oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        ValueSpread toSet = new ValueSpread(oldValue);
        if (delta[0] != null) {
            toSet.setValue(delta[0]);
        }
        if (delta[1] != null) {
            toSet.setSpread(delta[1]);
        }

        methods.propertyDescriptor.getWriteMethod().invoke(obj, toSet);
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
        // Not currently supported in this accessor
        return false;
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
    public void resetValue(Object obj, String property, IPropertyObjectWorld world) {
        throw new RuntimeException(String.format("The property %s is not possible to reset.", property));
    }

}
