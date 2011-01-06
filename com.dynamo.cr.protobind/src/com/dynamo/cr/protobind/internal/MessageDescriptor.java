package com.dynamo.cr.protobind.internal;

import java.util.List;

import com.dynamo.cr.protobind.IMessageDecriptor;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;

public class MessageDescriptor implements IMessageDecriptor {

    Descriptor descriptorType;
    FieldDescriptorSocket[] fieldDescriptors;

    public MessageDescriptor(Descriptor descriptorType) {
        this.descriptorType = descriptorType;

        List<FieldDescriptor> fields = descriptorType.getFields();
        this.fieldDescriptors = new FieldDescriptorSocket[fields.size()];
        int i = 0;
        for (FieldDescriptor fd : fields) {
            fieldDescriptors[i] = new FieldDescriptorSocket(this, fd);
            ++i;
        }

        resolveParents(this, null);
    }

    static void resolveParents(MessageDescriptor descriptor, FieldDescriptorSocket parent) {
        for (FieldDescriptorSocket f : descriptor.fieldDescriptors) {
            f.parent = parent;
            if (f.messageDescriptorSocket != null)
                resolveParents(f.messageDescriptorSocket, f);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IMessageDecriptor#getFieldDescriptors()
     */
    @Override
    public FieldDescriptorSocket[] getFieldDescriptors() {
        return fieldDescriptors;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IMessageDecriptor#getName()
     */
    @Override
    public String getName() {
        return descriptorType.getName();
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IMessageDecriptor#findFieldByName(java.lang.String)
     */
    @Override
    public FieldDescriptorSocket findFieldByName(String name) {
        for (FieldDescriptorSocket f : fieldDescriptors) {
            if (f.getName().equals(name))
                return f;
        }
        return null;
    }
}
