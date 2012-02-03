package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.InstanceNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;

public class RemoveInstanceOperation extends AbstractSelectOperation {

    final private CollectionNode collection;
    final private InstanceNode instance;

    public RemoveInstanceOperation(InstanceNode instance, IPresenterContext presenterContext) {
        super((instance instanceof GameObjectInstanceNode) ? "Remove Game Object" : "Remove Collection", NodeUtil.getSelectionReplacement(instance), presenterContext);
        this.collection = (CollectionNode)instance.getParent();
        this.instance = instance;
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.removeInstance(this.instance);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.removeInstance(this.instance);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.collection.addInstance(this.instance);
        return Status.OK_STATUS;
    }

}
