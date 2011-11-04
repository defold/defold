package com.dynamo.cr.goprot;

import javax.inject.Inject;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.Node;
import com.dynamo.cr.properties.IFormPropertySheetPage;

public class NodeView implements INodeView {

    @Inject private INodeOutlinePage outline;
    @Inject private IFormPropertySheetPage propertySheetPage;

    @Override
    public void updateNode(Node node) {
        this.outline.update(node);
        this.propertySheetPage.refresh();
    }

}
