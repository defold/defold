package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.gui.proto.Gui;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class ModelEditSupport implements IResourceTypeEditSupport {

    public ModelEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(Gui.SceneDesc.FontDesc.getDescriptor().getFullName())) {
            return Gui.SceneDesc.FontDesc.newBuilder().setFont("unnamed.font").setName("unnamed").build();
        }
        return null;
    }

}
