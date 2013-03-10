package com.dynamo.cr.go.core;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.go.core.operations.AddInstanceOperation;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class CollectionPresenter implements ISceneView.INodePresenter<CollectionNode> {

    private static Logger logger = LoggerFactory.getLogger(CollectionPresenter.class);
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
        GameObjectNode instance = new GameObjectNode();
        presenterContext.executeOperation(new AddInstanceOperation(parent, instance, presenterContext));
    }

    public void onAddGameObjectFromFile(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        // Find selected collection
        CollectionNode parent = findCollectionFromSelection(presenterContext.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No collection in selection.");
        }
        String file = presenterContext.selectFile(Messages.CollectionPresenter_ADD_GAME_OBJECT, new String[] {"go"});
        if (file != null) {
            GameObjectNode gameObject = null;
            try {
                gameObject = (GameObjectNode)loaderContext.loadNode(file);
            } catch (Exception e) {
                // TODO: Dialog here?
                logger.error("Error occurred while adding game-object", e);
                return;
            }
            RefGameObjectInstanceNode instance = new RefGameObjectInstanceNode(gameObject);
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
        String file = presenterContext.selectFile(Messages.CollectionPresenter_ADD_SUB_COLLECTION, new String[] {"collection"});
        if (file != null) {
            CollectionNode collection = null;
            try {
                collection = (CollectionNode)loaderContext.loadNode(file);
            } catch (Exception e) {
                // TODO: Dialog here?
                logger.error("Error occurred while adding collection", e);
                return;
            }
            CollectionInstanceNode instance = new CollectionInstanceNode(collection);
            instance.setCollection(file);
            presenterContext.executeOperation(new AddInstanceOperation(parent, instance, presenterContext));
        }
    }

}
