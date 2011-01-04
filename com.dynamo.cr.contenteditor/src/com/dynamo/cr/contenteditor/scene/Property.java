package com.dynamo.cr.contenteditor.scene;

import java.lang.reflect.Field;

import javax.xml.bind.PropertyException;

import com.dynamo.cr.contenteditor.scene.PropertyUtil.FieldObjectPair;

public abstract class Property implements IProperty
{
    private final IProperty m_Parent;
    private final String m_Name;
    private final String m_MemberName;
    private boolean m_Independent = true;
    private IPropertyUpdater m_Updater;

    public Property(IProperty parent, String name, String member_name)
    {
        m_Parent = parent;
        m_Name = name;
        m_MemberName = member_name;
    }

    /**
     * Clone value. Mutable values must return a copy
     * @param value Value to clone
     * @return Cloned value
     */
    protected abstract Object cloneValue(Object value);

    /**
     * Do set value with value semantics.
     * @param field Field
     * @param object Object with field
     * @param value Value to set
     * @throws IllegalArgumentException
     * @throws IllegalAccessException
     */
    protected abstract void doSetValue(Field field, Object object, Object value) throws IllegalArgumentException, IllegalAccessException;

    public void setUpdater(IPropertyUpdater updater, boolean sub_properties)
    {
        m_Updater = updater;
        if (sub_properties) {
            IProperty[] sub_props = getSubProperties();
            if (sub_props == null) return;
            for (IProperty p : sub_props) {
                p.setUpdater(updater, sub_properties);
            }
        }
    }

    protected void setIndependent(boolean independent)
    {
        m_Independent = independent;
    }

    @Override
    public boolean isIndependent()
    {
        return m_Independent;
    }

    @Override
    public String[] getEnumValues()
    {
        return null;
    }

    @Override
    public String getMemberName()
    {
        return m_MemberName;
    }

    @Override
    public String getName()
    {
        return m_Name;
    }

    @Override
    public IProperty getParentProperty()
    {
        return m_Parent;
    }

    @Override
    public IProperty[] getSubProperties()
    {
        return null;
    }

    @Override
    public Object getValue(Node node) throws PropertyException
    {
        if (m_Updater != null) {
            m_Updater.update(this);
        }
        FieldObjectPair pair = PropertyUtil.findField(node, this);
        try
        {
            Object ret = pair.m_Field.get(pair.m_Object);
            return cloneValue(ret);
        } catch (Throwable e)
        {
            throw new PropertyException(e.getMessage(), e);
        }
    }

    @Override
    public Object getEditableValue(Node node) throws PropertyException
    {
        return getValue(node);
    }

    @Override
    public boolean setValue(Node node, Object value) throws PropertyException
    {
        FieldObjectPair pair = PropertyUtil.findField(node, this);
        try
        {
            doSetValue(pair.m_Field, pair.m_Object, value);
            node.getScene().propertyChanged(node, this);
            node.propertyChanged(this);
            return true;

        } catch (Throwable e)
        {
            throw new PropertyException(e.getMessage(), e);
        }
    }
}
