package com.dynamo.cr.properties;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.graphics.RGB;

public class BeanPropertyAccessor implements IPropertyAccessor<Object, IPropertyObjectWorld> {

    private PropertyDescriptor propertyDescriptor;
    private Method isEditable;
    private Method isVisible;

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

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            isEditable = obj.getClass().getMethod(String.format("is%sEditable", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

        try {
            String capProperty = Character.toUpperCase(property.charAt(0)) + property.substring(1);
            isVisible = obj.getClass().getMethod(String.format("is%sVisible", capProperty));
        } catch (NoSuchMethodException e) {
            // pass
        }

    }

    @Override
    public void setValue(Object obj, String property, Object newValue,
            IPropertyObjectWorld world) {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing setter for %s#%s", obj.getClass().getName(), property));
        }

        Object oldValue;
        try {
            oldValue = propertyDescriptor.getReadMethod().invoke(obj);
            // TODO: This merging is rather static. We should perhaps
            // make it more dynamic in the future.
            if (oldValue instanceof Vector4d && newValue instanceof Double[]) {
                mergeVector4dValue(obj, (Vector4d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Vector3d && newValue instanceof Double[]) {
                mergeVector3dValue(obj, (Vector3d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Point3d && newValue instanceof Double[]) {
                mergePoint3dValue(obj, (Point3d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof Quat4d && newValue instanceof Double[]) {
                mergeQuat4dValue(obj, (Quat4d) oldValue, (Double[]) newValue);
            } else if (oldValue instanceof RGB && newValue instanceof Double[]) {
                mergeRGBValue(obj, (RGB) oldValue, (Double[]) newValue);
            }
            else {
                // Generic set
                propertyDescriptor.getWriteMethod().invoke(obj, newValue);
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private void mergeVector4dValue(Object obj, Vector4d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Vector4d toSet = new Vector4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;

        propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeVector3dValue(Object obj, Vector3d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Vector3d toSet = new Vector3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;

        propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergePoint3dValue(Object obj, Point3d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Point3d toSet = new Point3d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;

        propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeQuat4dValue(Object obj, Quat4d oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        Quat4d toSet = new Quat4d();

        toSet.x = delta[0] != null ? delta[0] : oldValue.x;
        toSet.y = delta[1] != null ? delta[1] : oldValue.y;
        toSet.z = delta[2] != null ? delta[2] : oldValue.z;
        toSet.w = delta[3] != null ? delta[3] : oldValue.w;

        propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }

    private void mergeRGBValue(Object obj, RGB oldValue, Double[] delta) throws IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        RGB toSet = new RGB(0, 0, 0);

        toSet.red = (int) (delta[0] != null ? delta[0] : oldValue.red);
        toSet.green = (int) (delta[1] != null ? delta[1] : oldValue.green);
        toSet.blue = (int) (delta[2] != null ? delta[2] : oldValue.blue);

        propertyDescriptor.getWriteMethod().invoke(obj, toSet);
    }


    @Override
    public Object getValue(Object obj, String property,
            IPropertyObjectWorld world) {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing getter for %s#%s", obj.getClass().getName(), property));
        }
        try {
            return propertyDescriptor.getReadMethod().invoke(obj);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public boolean isEditable(Object obj, String property,
            IPropertyObjectWorld world) {
        // First check if the object is editable at all
        boolean editable = true;
        try {
            Method method = obj.getClass().getMethod("isEditable");
            try {
                editable = (Boolean) method.invoke(obj);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } catch (NoSuchMethodException e1) {
            editable = true;
        }
        // If the object is editable, check the property in question
        if (editable) {
            init(obj, property);
            if (isEditable != null) {
                try {
                    return (Boolean) isEditable.invoke(obj);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }
        return editable;
    }

    @Override
    public boolean isVisible(Object obj, String property,
            IPropertyObjectWorld world) {
        init(obj, property);
        if (isVisible != null) {
            try {
                return (Boolean) isVisible.invoke(obj);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            return true;
        }
    }

}
