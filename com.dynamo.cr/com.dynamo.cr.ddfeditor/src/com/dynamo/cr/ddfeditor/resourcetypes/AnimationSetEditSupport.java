package com.dynamo.cr.ddfeditor.resourcetypes;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.rig.proto.Rig.AnimationSetDesc;
import com.dynamo.rig.proto.Rig.AnimationInstanceDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class AnimationSetEditSupport implements IResourceTypeEditSupport {

    public AnimationSetEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(AnimationSetDesc.getDescriptor().getFullName())) {
            return AnimationSetDesc.newBuilder().build();
        }
        else if (descriptor.getFullName().equals(AnimationInstanceDesc.getDescriptor().getFullName())) {
            return AnimationInstanceDesc.newBuilder().setAnimation("").build();
        }

        return null;
    }

    @Override
    public String getLabelText(Message message) {
        if (message instanceof AnimationInstanceDesc)
        {
            AnimationInstanceDesc msg = (AnimationInstanceDesc) message;
            return FilenameUtils.getBaseName(msg.getAnimation());
        }
        return "";
    }
}
