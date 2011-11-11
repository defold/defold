package com.dynamo.cr.sceneed;

import javax.inject.Inject;

import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.properties.IFormPropertySheetPage;
import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.Node;

public class NodeView implements INodeView {

    @Inject private INodeOutlinePage outline;
    @Inject private IFormPropertySheetPage propertySheetPage;
    @Inject private ISelectionProvider selectionProvider;
    @Inject private SceneView sceneView;

    @Override
    public void setRoot(Node root) {
        this.outline.setInput(root);
        this.sceneView.setRoot(root);
    }

    @Override
    public void updateNode(Node node) {
        this.outline.update(node);
        this.propertySheetPage.refresh();
        this.sceneView.refresh();
    }

    @Override
    public void updateSelection(IStructuredSelection selection) {
        // Update all selection providers
        this.selectionProvider.setSelection(selection);
        this.outline.setSelection(selection);
    }

}
