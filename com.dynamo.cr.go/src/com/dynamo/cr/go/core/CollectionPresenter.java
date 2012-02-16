package com.dynamo.cr.go.core;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.go.core.operations.AddInstanceOperation;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.RemoveChildrenOperation;

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

    public void onAddGameObject(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        // Find selected collection
        CollectionNode parent = findCollectionFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collection in selection.");
        }
        String file = presenterContext.selectFile(Messages.CollectionPresenter_ADD_GAME_OBJECT);
        if (file != null) {
            GameObjectNode gameObject = null;
            try {
                gameObject = (GameObjectNode)loaderContext.loadNode(file);
            } catch (Exception e) {
                presenterContext.logException(e);
                return;
            }
            GameObjectInstanceNode instance = new GameObjectInstanceNode(gameObject);
            instance.setGameObject(file);
            presenterContext.executeOperation(new AddInstanceOperation(parent, instance, presenterContext));
        }
    }

    public void onAddCollection(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        // Find selected collection
        CollectionNode parent = findCollectionFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collection in selection.");
        }
        String file = presenterContext.selectFile(Messages.CollectionPresenter_ADD_SUB_COLLECTION);
        if (file != null) {
            CollectionNode collection = null;
            try {
                collection = (CollectionNode)loaderContext.loadNode(file);
            } catch (Exception e) {
                presenterContext.logException(e);
                return;
            }
            CollectionInstanceNode instance = new CollectionInstanceNode(collection);
            instance.setCollection(file);
            presenterContext.executeOperation(new AddInstanceOperation(parent, instance, presenterContext));
        }
    }

    public void onRemoveInstance(IPresenterContext context) {
        // Find selected components
        IStructuredSelection structuredSelection = context.getSelection();
        Object[] nodes = structuredSelection.toArray();
        List<Node> instances = new ArrayList<Node>(nodes.length);
        for (Object node : nodes) {
            instances.add((Node)node);
        }
        context.executeOperation(new RemoveChildrenOperation(instances, context));
    }

}
