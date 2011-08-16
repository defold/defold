package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Material.MaterialDesc.ConstantType;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class MaterialEditSupport implements IResourceTypeEditSupport {

    public MaterialEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(MaterialDesc.Constant.getDescriptor().getFullName())) {
            return MaterialDesc.Constant.newBuilder()
                    .setType(ConstantType.CONSTANT_TYPE_USER)
                    .setName("constant_name")
                    .setValue(Vector4.newBuilder().build()).build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        return "";
    }

}
