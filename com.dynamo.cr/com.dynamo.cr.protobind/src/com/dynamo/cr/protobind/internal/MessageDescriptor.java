package com.dynamo.cr.protobind.internal;

import java.util.List;

import com.dynamo.cr.protobind.IMessageDecriptor;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;

public class MessageDescriptor implements IMessageDecriptor {

    Descriptor descriptorType;
    FieldPath[] fieldPaths;

    public MessageDescriptor(Descriptor descriptorType) {
        this.descriptorType = descriptorType;

        List<FieldDescriptor> fields = descriptorType.getFields();
        this.fieldPaths = new FieldPath[fields.size()];
        int i = 0;
        for (FieldDescriptor fd : fields) {
            fieldPaths[i] = new FieldPath(this, fd);
            ++i;
        }

        resolveParents(this, null);
    }

    static void resolveParents(MessageDescriptor descriptor, FieldPath parent) {
        for (FieldPath f : descriptor.fieldPaths) {
            f.parent = parent;
            if (f.messageDescriptor != null)
                resolveParents(f.messageDescriptor, f);
        }
    }

    @Override
    public FieldPath[] getFieldPaths() {
        return fieldPaths;
    }

    @Override
    public String getName() {
        return descriptorType.getName();
    }

    @Override
    public FieldPath findFieldByName(String name) {
        for (FieldPath f : fieldPaths) {
            if (f.getName().equals(name))
                return f;
        }
        return null;
    }

    @Override
    public Descriptor getDescriptor() {
        return descriptorType;
    }
}
