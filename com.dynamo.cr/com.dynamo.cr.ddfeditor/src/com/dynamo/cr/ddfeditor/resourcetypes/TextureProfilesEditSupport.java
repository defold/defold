package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.graphics.proto.Graphics.PathSettings;
import com.dynamo.graphics.proto.Graphics.PlatformProfile;
import com.dynamo.graphics.proto.Graphics.TextureFormatAlternative;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class TextureProfilesEditSupport implements IResourceTypeEditSupport {

    public TextureProfilesEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(TextureProfile.getDescriptor().getFullName())) {
            return TextureProfile.newBuilder().setName("unnamed").build();
        }
        else if (descriptor.getFullName().equals(PlatformProfile.getDescriptor().getFullName())) {
            TextureFormatAlternative tmpFormat = TextureFormatAlternative.newBuilder().setFormat(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA).setCompressionLevel(TextureFormatAlternative.CompressionLevel.BEST).build();
            return PlatformProfile.newBuilder().setOs(PlatformProfile.OS.OS_ID_GENERIC).setMipmaps(true).addFormats(tmpFormat).build();
        }
        else if (descriptor.getFullName().equals(TextureFormatAlternative.getDescriptor().getFullName())) {
            return TextureFormatAlternative.newBuilder().setFormat(TextureImage.TextureFormat.TEXTURE_FORMAT_RGBA).setCompressionLevel(TextureFormatAlternative.CompressionLevel.BEST).build();
        }
        else if (descriptor.getFullName().equals(PathSettings.getDescriptor().getFullName())) {
            return PathSettings.newBuilder().setPath("**").setProfile("Default").build();
        }

        return null;
    }

    @Override
    public String getLabelText(Message message) {

        if (message instanceof TextureProfile)
        {
            TextureProfile texProfile = (TextureProfile) message;
            return texProfile.getName();
        }
        else if (message instanceof PlatformProfile) {
            PlatformProfile platformProfile = (PlatformProfile) message;
            return platformProfile.getOs().toString();
        }
        else if (message instanceof TextureFormatAlternative) {
            TextureFormatAlternative formatAlternative = (TextureFormatAlternative) message;
            return formatAlternative.getFormat().toString();
        }
        else if (message instanceof PathSettings) {
            PathSettings pathSetting = (PathSettings) message;
            return pathSetting.getProfile() + ": " + pathSetting.getPath();
        }

        return "";
    }
}
