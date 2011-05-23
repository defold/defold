package com.dynamo.cr.guieditor.property;

import javax.vecmath.Tuple4d;
import javax.vecmath.Vector4d;

import org.eclipse.ui.views.properties.IPropertyDescriptor;
import org.eclipse.ui.views.properties.TextPropertyDescriptor;

public class Vector4dEmbeddedSource implements IEmbeddedPropertySource<Vector4d> {

    private IPropertyDescriptor[] propertyDescriptors;

    @Override
    public IPropertyDescriptor[] getPropertyDescriptors() {
        if (propertyDescriptors == null) {
            propertyDescriptors = new IPropertyDescriptor[4];
            propertyDescriptors[0] = new TextPropertyDescriptor("x", "x");
            propertyDescriptors[1] = new TextPropertyDescriptor("y", "y");
            propertyDescriptors[2] = new TextPropertyDescriptor("z", "z");
            propertyDescriptors[3] = new TextPropertyDescriptor("w", "w");
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
        return Double.toString(componentValue);
    }

    @Override
    public void setPropertyValue(Vector4d object, Object id, Object value) {
        String stringValue = (String) value;
        double doubleValue = Double.parseDouble(stringValue);
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
