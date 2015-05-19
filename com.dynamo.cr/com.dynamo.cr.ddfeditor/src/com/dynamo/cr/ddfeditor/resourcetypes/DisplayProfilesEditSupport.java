package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.render.proto.Render.DisplayProfile;
import com.dynamo.render.proto.Render.DisplayProfileQualifier;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class DisplayProfilesEditSupport implements IResourceTypeEditSupport {

    public DisplayProfilesEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(DisplayProfile.getDescriptor().getFullName())) {
            return DisplayProfile.newBuilder().setName("unnamed").build();
        }
        else if (descriptor.getFullName().equals(DisplayProfileQualifier.getDescriptor().getFullName())) {
            return DisplayProfileQualifier.newBuilder().setWidth(1280).setHeight(720).build();
        }

        return null;
    }

    @Override
    public String getLabelText(Message message) {
        if (message instanceof DisplayProfile)
        {
            DisplayProfile displayProfile = (DisplayProfile) message;
            return displayProfile.getName();
        }

        return "";
    }
}
