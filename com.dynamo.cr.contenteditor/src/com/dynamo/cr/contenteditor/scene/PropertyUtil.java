package com.dynamo.cr.contenteditor.scene;

import java.lang.reflect.Field;

import javax.xml.bind.PropertyException;

public class PropertyUtil
{
    public static class FieldObjectPair
    {
        public FieldObjectPair(Field field, Object object)
        {
            m_Field = field;
            m_Object = object;
        }
        Field m_Field;
        Object m_Object;
    }

    private static Field findDeclaredField(Class<?> klass, String name) throws PropertyException
    {
        Class<?> c = klass;
        while (c != Object.class)
        {
            Field f = null;
            try
            {
                f = c.getDeclaredField(name);
                return f;
            } catch (Throwable e)
            {
            }
            c = c.getSuperclass();
        }

        throw new PropertyException("No such field: " + name);
    }

    public static FieldObjectPair findField(Node node, IProperty property)
    {
        String[] path = new String[16];

        int i = 0;
        IProperty p = property.getParentProperty();
        while (p != null)
        {
            path[i++] = p.getMemberName();
            p = p.getParentProperty();
        }
        path[i++] = property.getMemberName();
        int path_len = i;

        Object obj = node;
        Field f = null;
        for (int j = 0; j < path_len; ++j)
        {
            try
            {
                f = findDeclaredField(obj.getClass(), path[j]);
                f.setAccessible(true);

                if (j != path_len - 1)
                    obj = f.get(obj);

            } catch (Throwable e)
            {
                e.printStackTrace();
            }
        }

        return new FieldObjectPair(f, obj);
    }

    public static boolean setValue(Node node, IProperty property, Object value) throws PropertyException
    {
        FieldObjectPair pair = PropertyUtil.findField(node, property);
        try
        {
            pair.m_Field.set(pair.m_Object, value);
        } catch (Throwable e)
        {
            throw new PropertyException(e.getMessage(), e);
        }
        return true;
    }

    public static Object getValue(Node node, Property property) throws PropertyException
    {
        FieldObjectPair pair = PropertyUtil.findField(node, property);
        try
        {
            Object ret = pair.m_Field.get(pair.m_Object);
            return ret;
        } catch (Throwable e)
        {
            throw new PropertyException(e.getMessage(), e);
        }
    }

    public static boolean isPropertyOf(IProperty sub_property, IProperty property)
    {
        IProperty p = sub_property;
        while (p != null)
        {
            if (p == property)
                return true;
            p = p.getParentProperty();
        }
        return false;
    }
}
