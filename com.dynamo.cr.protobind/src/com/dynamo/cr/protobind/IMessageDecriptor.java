package com.dynamo.cr.protobind;

import com.dynamo.cr.protobind.internal.FieldDescriptorSocket;

/**
 * A wrapper for the message descriptor type.
 * @author chmu
 *
 */
public interface IMessageDecriptor {

    /**
     * Get field descriptor sockets
     * @return an array of field descriptor sockets
     */
    public abstract FieldDescriptorSocket[] getFieldDescriptors();

    /**
     * Get the message type name
     * @return the message type name
     */
    public abstract String getName();

    /**
     * Find field by name
     * @param name name of the field to get
     * @return the field descriptor socket. null of the field doesn't exists
     */
    public abstract FieldDescriptorSocket findFieldByName(String name);

}