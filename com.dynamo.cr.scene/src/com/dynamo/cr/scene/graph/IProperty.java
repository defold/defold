package com.dynamo.cr.scene.graph;

import javax.xml.bind.PropertyException;

/**
 * Property descriptor
 * @author chmu
 *
 */
public interface IProperty
{
    enum EditorType
    {
        TEXT,
        ENUM,
        LIST,
        COMPOUND,
    }

    /**
     * Set update interface for property. IPropertyUpdater.update when getValue() is called
     * @param updater
     * @param sub_properties Apply to subproperties
     */
    void setUpdater(IPropertyUpdater updater, boolean sub_properties);

    /**
     * Is this property independent?
     * Undo information of the parent property of an independent property is stored to
     * ensure that all properties are restored to original values.
     * @return True if independent
     */
    boolean isIndependent();

    /**
     * Get value of this property in node. For mutable types a
     * copy must be returned.
     * @param node Node
     * @return Value
     * @throws PropertyException
     */
    Object getValue(Node node) throws PropertyException;

    /**
     * Get editable value of this property in node
     * @param node Node
     * @return
     * @throws PropertyException
     */
    Object getEditableValue(Node node) throws PropertyException;

    /**
     * Set value with value semantics of this property in node.
     * @param node Node
     * @param value Value to set
     * @return True on success
     * @throws PropertyException
     */
    boolean setValue(Node node, Object value) throws PropertyException;
    // TODO: remove boolean?

    /**
     * Get parent property
     * @return Parent property
     */
    IProperty getParentProperty();

    /**
     * Get sub-properties
     * @return Array of sub-properties
     */
    IProperty[] getSubProperties();

    /**
     * Get enum values for this property
     * @return
     */
    String[] getEnumValues();

    /**
     * Get property name
     * @return Property name
     */
    String getName();

    /**
     * Get member name, ie member name of property in node
     * @return Member name
     */
    String getMemberName();
}
