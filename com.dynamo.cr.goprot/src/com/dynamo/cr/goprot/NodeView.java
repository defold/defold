package com.dynamo.cr.goprot;

import javax.inject.Inject;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.Node;

public class NodeView implements INodeView {

    @SuppressWarnings("unused")
    @Inject private NodeEditor editor;
    @Inject private INodeOutlinePage outline;

    @Override
    public void updateNode(Node node) {
        outline.update(node);
    }

}
