package com.dynamo.cr.contenteditor.scene;

import java.lang.reflect.Field;

import javax.vecmath.Quat4d;
import javax.vecmath.Tuple4d;
import javax.vecmath.Vector4d;

public class Vector4Property extends Property
{
    private final IProperty[] m_SubProperties;

    public Vector4Property(IProperty parent, String name, String member_name, boolean skip_w, boolean independent_components)
    {
        super(parent, name, member_name);

        if (skip_w)
        {
            m_SubProperties = new IProperty[] { new FloatProperty(this, "x", "x", independent_components), new FloatProperty(this, "y", "y", independent_components),
                    new FloatProperty(this, "z", "z", independent_components), };
        }
        else
        {
            m_SubProperties = new IProperty[] { new FloatProperty(this, "x", "x", independent_components), new FloatProperty(this, "y", "y", independent_components),
                    new FloatProperty(this, "z", "z", independent_components), new FloatProperty(this, "w", "w", independent_components) };

        }
    }

    @Override
    public IProperty[] getSubProperties()
    {
        return m_SubProperties;
    }

    @Override
    protected Object cloneValue(Object value)
    {
        Tuple4d t = null;
        if (value instanceof Vector4d)
        {
            t = new Vector4d();
        }
        else if (value instanceof Quat4d)
        {
            t = new Quat4d();
        }
        t.set((Tuple4d) value);
        return t;
    }

    @Override
    protected void doSetValue(Field field, Object object, Object value) throws IllegalArgumentException, IllegalAccessException
    {
        Tuple4d v = (Tuple4d) field.get(object);
        v.set((Tuple4d) value);
    }
}
