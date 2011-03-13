package com.dynamo.cr.ddfeditor;

import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.MessageNode;

public interface IProtoListener {
    public void fieldChanged(MessageNode message, IPath field);
}
