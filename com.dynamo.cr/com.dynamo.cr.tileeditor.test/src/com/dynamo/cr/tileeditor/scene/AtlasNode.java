package com.dynamo.cr.tileeditor.scene;

import com.dynamo.cr.sceneed.core.Node;

@SuppressWarnings("serial")
public class AtlasNode extends Node {
    @Override
    protected void childAdded(Node child) {
        updateStatus();
    }
}
