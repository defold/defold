package com.dynamo.cr.properties;

import java.beans.BeanInfo;
import java.beans.IntrospectionException;
import java.beans.Introspector;
import java.beans.PropertyDescriptor;
import java.lang.reflect.InvocationTargetException;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.swt.graphics.RGB;

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
    public void setValue(Object obj, String property, Object newValue,
            IPropertyObjectWorld world)
                    throws IllegalArgumentException, IllegalAccessException,
                    InvocationTargetException {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing setter for %s#%s", obj.getClass().getName(), property));
        }

        Object oldValue = propertyDescriptor.getReadMethod().invoke(obj);

        // TODO: This merging is rather static. We should perhaps
        // make it more dynamic in the future.
        if (oldValue instanceof Vector4d && newValue instanceof Double[]) {
            mergeVector4dValue(obj, (Vector4d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof Vector3d && newValue instanceof Double[]) {
            mergeVector3dValue(obj, (Vector3d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof Quat4d && newValue instanceof Double[]) {
            mergeQuat4dValue(obj, (Quat4d) oldValue, (Double[]) newValue);
        } else if (oldValue instanceof RGB && newValue instanceof Double[]) {
            mergeRGBValue(obj, (RGB) oldValue, (Double[]) newValue);
        }
        else {
            // Generic set
            propertyDescriptor.getWriteMethod().invoke(obj, newValue);
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
            IPropertyObjectWorld world)
                    throws IllegalArgumentException, IllegalAccessException,
                    InvocationTargetException {
        init(obj, property);
        if (propertyDescriptor == null) {
            throw new RuntimeException(String.format("Missing getter for %s#%s", obj.getClass().getName(), property));
        }
        return propertyDescriptor.getReadMethod().invoke(obj);
    }

    @Override
    public IStatus getStatus(Object obj, String property,
            IPropertyObjectWorld world) throws IllegalArgumentException,
            IllegalAccessException, InvocationTargetException {
        return null;
    }
}
