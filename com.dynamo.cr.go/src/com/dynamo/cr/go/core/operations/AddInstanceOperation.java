package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.CollectionInstanceNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;

public class AddInstanceOperation extends AbstractSelectOperation {

    final private CollectionNode collection;
    final private InstanceNode instance;

    public AddInstanceOperation(CollectionNode collection, InstanceNode instance, IPresenterContext presenterContext) {
        super((instance instanceof GameObjectInstanceNode) ? "Add Game Object" : "Add Collection", instance, presenterContext);
        this.collection = collection;
        this.instance = instance;
        String id = "";
        String path = "";
        if (instance instanceof GameObjectInstanceNode) {
            path = ((GameObjectInstanceNode)instance).getGameObject();
        } else if (instance instanceof CollectionInstanceNode) {
            path = ((CollectionInstanceNode)instance).getCollection();
        }
        IPath p = new Path(path).removeFileExtension();
        id = p.lastSegment();
        id = NodeUtil.getUniqueId(this.collection, id, new NodeUtil.IdFetcher<Node>() {
            @Override
            public String getId(Node child) {
                return ((InstanceNode)child).getId();
            }
        });
        this.instance.setId(id);
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.addInstance(this.instance);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.addInstance(this.instance);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.removeInstance(this.instance);
        return Status.OK_STATUS;
    }

}
