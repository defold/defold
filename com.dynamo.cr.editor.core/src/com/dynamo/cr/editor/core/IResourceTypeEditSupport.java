package com.dynamo.cr.editor.core;

import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public interface IResourceTypeEditSupport {

    Message getTemplateMessageFor(Descriptor descriptor);
}
