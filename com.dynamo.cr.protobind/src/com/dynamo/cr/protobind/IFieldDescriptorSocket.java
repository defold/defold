package com.dynamo.cr.protobind;

/**
 * Field descriptor socket. Represents a field. Similar to FieldDescriptor but represents a path to the actual field.
 * Due to the complete path fields can be set from the top value of the message (for nested messages)
 * @author chmu
 *
 */
public interface IFieldDescriptorSocket {

    /**
     * Get the containing message descriptor, ie field type which this field belongs to
     * @return the containing message descriptor
     */
    public abstract IMessageDecriptor getContainingDescriptor();

    /**
     * Get message descriptor for the field if exists.
     * @return message descriptor. null of the field isn't of message type
     */
    public abstract IMessageDecriptor getMessageDescriptor();

    /**
     * Get field name
     * @return the field name
     */
    public abstract String getName();
}
