package com.dynamo.cr.editor.core;

import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.Message;

public interface IResourceType {

    String getId();

    String getName();

    String getFileExtension();

    byte[] getTemplateData();

    Message createTemplateMessage();

    Class<GeneratedMessage> getMessageClass();

    Descriptor getMessageDescriptor();

    boolean isEmbeddable();
}
