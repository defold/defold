package com.dynamo.cr.properties;

import javax.vecmath.Tuple4d;
import javax.vecmath.Vector4d;

import org.eclipse.ui.views.properties.IPropertyDescriptor;

import com.dynamo.cr.properties.internal.DoublePropertyDescriptor;

public class Vector4dEmbeddedSource implements IEmbeddedPropertySource<Vector4d> {

    private IPropertyDescriptor[] propertyDescriptors;

    @Override
    public IPropertyDescriptor[] getPropertyDescriptors() {
        if (propertyDescriptors == null) {
            propertyDescriptors = new IPropertyDescriptor[4];
            propertyDescriptors[0] = new DoublePropertyDescriptor("x", "x");
            propertyDescriptors[1] = new DoublePropertyDescriptor("y", "y");
            propertyDescriptors[2] = new DoublePropertyDescriptor("z", "z");
            propertyDescriptors[3] = new DoublePropertyDescriptor("w", "w");
        }
        return propertyDescriptors;
    }

    @Override
    public Object getPropertyValue(Vector4d object, Object id) {
        char component = ((String) id).charAt(0);
        double componentValue = 0;

        Tuple4d tuple = (Tuple4d) object;

        if (component == 'x') {
            componentValue = tuple.getX();
        }
        else if (component == 'y') {
            componentValue = tuple.getY();
        }
        else if (component == 'z') {
            componentValue = tuple.getZ();
        }
        else if (component == 'w') {
            componentValue = tuple.getW();
        }
        return new Double(componentValue);
    }

    @Override
    public void setPropertyValue(Vector4d object, Object id, Object value) {
        Double doubleValue = (Double) value;
        char component = ((String) id).charAt(0);

        if (component == 'x') {
            object.setX(doubleValue);
        }
        else if (component == 'y') {
            object.setY(doubleValue);
        }
        else if (component == 'z') {
            object.setZ(doubleValue);
        }
        else if (component == 'w') {
            object.setW(doubleValue);
        }
    }

}
