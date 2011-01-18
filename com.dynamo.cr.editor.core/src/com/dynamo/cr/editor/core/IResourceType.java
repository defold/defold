package com.dynamo.cr.editor.core;

import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

/**
 * Resource type description interface
 * @author chmu
 *
 */
public interface IResourceType {

    /**
     * Get resource type identifier
     * @return identifier
     */
    String getId();

    /**
     * Get resource type name
     * @return name
     */
    String getName();

    /**
     * Get resource type file extension
     * @return file extension
     */
    String getFileExtension();

    /**
     * Get resource type template data.
     * @return template data. null if no template data is defined for this type.
     */
    byte[] getTemplateData();

    /**
     * Create google protobuf message from template data.
     * @return message. null if this resource type isn't a protobuf type or if no template data exists for this type.
     */
    Message createTemplateMessage();

    /**
     * Get google protobuf message class
     * @return message class. null of not defined
     */
    Class<GeneratedMessage> getMessageClass();

    /**
     * Get google protobuf message descriptor
     * @return descriptor. null if not defined
     */
    Descriptor getMessageDescriptor();

    /**
     * Is the resource type embeddable in a Game Object prototype etc.
     * @return true if embeddable
     */
    boolean isEmbeddable();
}
