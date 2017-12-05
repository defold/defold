package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class RenderEditSupport implements IResourceTypeEditSupport {

    public RenderEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {

        if (descriptor.getFullName().equals(RenderPrototypeDesc.MaterialDesc.getDescriptor().getFullName())) {
            return RenderPrototypeDesc.MaterialDesc.newBuilder().setMaterial("unnamed.material").setName("unnamed").build();
        }
        if (descriptor.getFullName().equals(RenderPrototypeDesc.TextureDesc.getDescriptor().getFullName())) {
            return RenderPrototypeDesc.TextureDesc.newBuilder().setTexture("unnamed.atlas").setName("unnamed").build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        return "";
    }

}
