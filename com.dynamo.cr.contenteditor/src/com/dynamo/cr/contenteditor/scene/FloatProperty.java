package com.dynamo.cr.contenteditor.scene;

import java.lang.reflect.Field;

import javax.xml.bind.PropertyException;

public class FloatProperty extends Property
{

    public FloatProperty(IProperty parent, String name, String member_name, boolean independent)
    {
        super(parent, name, member_name);
        setIndependent(independent);
    }

    @Override
    public Object getEditableValue(Node node) throws PropertyException
    {
        Double x = (Double) getValue(node);
        return String.format("%f", x);
    }

    @Override
    public boolean setValue(Node node, Object value) throws PropertyException
    {
        if (value instanceof String)
        {
            String string_value = (String) value;
            try
            {
                Double f = Double.valueOf(string_value);
                return super.setValue(node, f);
            }
            catch (NumberFormatException e)
            {
                throw new PropertyException(e.getMessage(), e);
            }

        }
        else if (value instanceof Number)
        {
            Number number_value = (Number) value;
            return super.setValue(node, number_value.floatValue());
        }
        else
        {
            throw new PropertyException("Unable to coerce " + value + " to float");
        }
    }

    @Override
    protected Object cloneValue(Object ret)
    {
        // Immutable value
        return ret;
    }

    @Override
    protected void doSetValue(Field field, Object object, Object value) throws IllegalArgumentException,
            IllegalAccessException
    {
        // Immutable value
        field.set(object, value);
    }
}
