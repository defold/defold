package com.dynamo.cr.protobind.internal;

import com.dynamo.cr.protobind.IFieldDescriptorSocket;
import com.dynamo.cr.protobind.IMessageDecriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor.Type;

public class FieldDescriptorSocket implements IFieldDescriptorSocket {

    public FieldDescriptor fieldDescriptor;
    public IMessageDecriptor descriptorSocket;
    public MessageDescriptor messageDescriptorSocket;
    public FieldDescriptorSocket parent;

    public FieldDescriptorSocket(IMessageDecriptor descriptorSocket, FieldDescriptor fieldDescriptor) {
        this.descriptorSocket = descriptorSocket;
        this.fieldDescriptor = fieldDescriptor;

        if (fieldDescriptor.getType() == Type.MESSAGE)
            this.messageDescriptorSocket = new MessageDescriptor(fieldDescriptor.getMessageType());
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IFieldDescriptorSocket#getContainingDescriptor()
     */
    @Override
    public IMessageDecriptor getContainingDescriptor() {
        return descriptorSocket;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IFieldDescriptorSocket#getMessageDescriptor()
     */
    @Override
    public IMessageDecriptor getMessageDescriptor() {
        return messageDescriptorSocket;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IFieldDescriptorSocket#getName()
     */
    @Override
    public String getName() {
        return fieldDescriptor.getName();
    }

}
