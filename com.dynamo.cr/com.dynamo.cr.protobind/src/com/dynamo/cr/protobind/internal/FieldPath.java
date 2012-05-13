package com.dynamo.cr.protobind.internal;

import com.dynamo.cr.protobind.IFieldPath;
import com.dynamo.cr.protobind.IMessageDecriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor.Type;

public class FieldPath implements IFieldPath {

    public FieldDescriptor fieldDescriptor;
    public IMessageDecriptor descriptor;
    public MessageDescriptor messageDescriptor;
    public FieldPath parent;

    public FieldPath(IMessageDecriptor descriptor, FieldDescriptor fieldDescriptor) {
        this.descriptor = descriptor;
        this.fieldDescriptor = fieldDescriptor;

        if (fieldDescriptor.getType() == Type.MESSAGE)
            this.messageDescriptor = new MessageDescriptor(fieldDescriptor.getMessageType());
    }

    @Override
    public IMessageDecriptor getContainingDescriptor() {
        return descriptor;
    }

    @Override
    public IMessageDecriptor getMessageDescriptor() {
        return messageDescriptor;
    }

    @Override
    public String getName() {
        return fieldDescriptor.getName();
    }

    @Override
    public FieldDescriptor getFieldDescriptor() {
        return fieldDescriptor;
    }

}
