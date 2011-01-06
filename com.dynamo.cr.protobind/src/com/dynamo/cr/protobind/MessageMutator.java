package com.dynamo.cr.protobind;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.protobind.internal.FieldDescriptorSocket;
import com.dynamo.cr.protobind.internal.MessageDescriptor;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;
import com.google.protobuf.Message.Builder;

/**
 * Wrapper class for google protocol buffers to mutate messages
 * @author chmu
 *
 * @param <T> A message type
 */
public class MessageMutator<T extends Message> {
    private Message.Builder builder;
    private Descriptor descriptorType;
    private IMessageDecriptor descriptorSocket;

    /**
     * Constructs a new message
     * @param message Original message content
     */
    public MessageMutator(T message) {
        this.builder = message.newBuilderForType().mergeFrom(message);
        this.descriptorType = message.getDescriptorForType();
        this.descriptorSocket = new MessageDescriptor(this.descriptorType);
    }

    /**
     * Get the {@link MessageDescriptor}
     * @return the descriptor socket
     */
    public IMessageDecriptor getDescriptorSocket() {
        return descriptorSocket;
    }

    /**
     * Get the {@link Descriptor}
     * @return the descriptor
     */
    public Descriptor getDescriptorType() {
        return descriptorType;
    }

    /**
     * Get field value. The descriptor can represent a field at arbitrary depth
     * @param fieldDescriptor the {@link FieldDescriptor} to get value for
     * @return the field value
     */
    public Object getField(FieldDescriptorSocket fieldDescriptor) {
        return doGetField(builder, fieldDescriptor);
    }

    /**
     * Set field value. The descriptor can represent a field at arbitrary depth
     * @param fieldDescriptor the {@link FieldDescriptor} to set value for
     * @param value value to set
     */
    public void setField(FieldDescriptorSocket fieldDescriptor, Object value) {
        ArrayList<FieldDescriptor> descriptorList = new ArrayList<FieldDescriptor>(32);
        getTopDownDescriptorList(fieldDescriptor, descriptorList);
        doSetField(builder, value, descriptorList, 0);
    }

    /**
     * Build a copy of the current message
     * @return a message
     */
    @SuppressWarnings("unchecked")
    public T build() {
        return (T) builder.clone().build();
    }

    Object genericGetField(Object object, FieldDescriptor fieldDescriptor) {
        if (object instanceof Message) {
            Message msg = (Message) object;
            return msg.getField(fieldDescriptor);
        }
        else {
            Builder bld = (Builder) object;
            return bld.getField(fieldDescriptor);
        }
    }

    Object doGetField(Object object, FieldDescriptorSocket fieldDescriptor) {
        FieldDescriptorSocket parent = fieldDescriptor.parent;

        if (parent == null) {
            return genericGetField(object, fieldDescriptor.fieldDescriptor);
        }
        else {
            Object tmp = doGetField(object, parent);
            return genericGetField(tmp, fieldDescriptor.fieldDescriptor);
        }
    }

    Object genericSetField(Object object, Object value, FieldDescriptor fieldDescriptor) {

        if (value instanceof Builder) {
            Builder b = (Builder) value;
            value = b.build();
        }

        if (object instanceof Message) {
            Message msg = (Message) object;
            return msg.newBuilderForType().mergeFrom(msg).setField(fieldDescriptor, value);
        }
        else {
            Builder bld= (Builder) object;
            return bld.setField(fieldDescriptor, value);
        }
    }

    Object doSetField(Object object, Object value, ArrayList<FieldDescriptor> descriptorList, int i) {

        if (descriptorList.size() - 1 == i) {
            return genericSetField(object, value, descriptorList.get(i));
        }
        else {
            Object sub = genericGetField(object, descriptorList.get(i));
            Object tmp = doSetField(sub, value, descriptorList, i + 1);
            return genericSetField(object, tmp, descriptorList.get(i));
        }
    }

    void getTopDownDescriptorList(FieldDescriptorSocket fieldDescriptor, List<FieldDescriptor> lst) {
        if (fieldDescriptor.parent != null) {
            getTopDownDescriptorList(fieldDescriptor.parent, lst);
        }
        lst.add(fieldDescriptor.fieldDescriptor);
    }
}
