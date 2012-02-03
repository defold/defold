package com.dynamo.cr.go.core;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.go.core.operations.AddInstanceOperation;
import com.dynamo.cr.go.core.operations.RemoveInstanceOperation;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class CollectionPresenter implements ISceneView.INodePresenter<CollectionNode> {

    private CollectionNode findCollectionFromSelection(IStructuredSelection selection) {
        Object[] nodes = selection.toArray();
        CollectionNode parent = null;
        for (Object node : nodes) {
            if (node instanceof CollectionNode) {
                parent = (CollectionNode)node;
                break;
            } else if (node instanceof InstanceNode) {
                parent = (CollectionNode)((Node)node).getParent();
                break;
            }
        }
        return parent;
    }

    public void onAddGameObject(IPresenterContext presenterContext) {
        // Find selected collection
        // TODO: Support multi selection
        CollectionNode parent = findCollectionFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collection in selection.");
        }
        presenterContext.executeOperation(new AddInstanceOperation(parent, new GameObjectInstanceNode(null), presenterContext));
    }

    public void onAddCollection(IPresenterContext presenterContext) {
        // Find selected collection
        // TODO: Support multi selection
        CollectionNode parent = findCollectionFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collection in selection.");
        }
        presenterContext.executeOperation(new AddInstanceOperation(parent, new CollectionInstanceNode(null), presenterContext));
    }

    public void onRemoveInstance(IPresenterContext context) {
        // Find selected components
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = context.getSelection();
        Object[] nodes = structuredSelection.toArray();
        InstanceNode instance = null;
        for (Object node : nodes) {
            if (node instanceof InstanceNode) {
                instance = (InstanceNode)node;
                break;
            }
        }
        context.executeOperation(new RemoveInstanceOperation(instance, context));
    }

}
