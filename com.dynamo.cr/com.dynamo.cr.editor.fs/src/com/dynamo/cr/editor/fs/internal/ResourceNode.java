package com.dynamo.cr.editor.fs.internal;

import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.protocol.proto.Protocol.ResourceType;

public class ResourceNode {

    /// Flags set to 1 if the meta-data for this node is known
    public final static int STATE_METADATA_KNOWN = (1 << 0);
    /// Flags set to 1 if children for this node is known
    public final static int STATE_METADATA_CHILDREN_KNOWN = (1 << 1);

    public String path;
    public String name;
    public ResourceType type;
    public long lastModified;
    public int size;

    public int state = 0;

    List<ResourceNode> children = new ArrayList<ResourceNode>();
}
