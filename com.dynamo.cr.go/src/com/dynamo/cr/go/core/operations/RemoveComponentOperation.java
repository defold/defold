package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;

public class RemoveComponentOperation extends AbstractSelectOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;

    public RemoveComponentOperation(ComponentNode component, IPresenterContext presenterContext) {
        super("Remove Component", NodeUtil.getSelectionReplacement(component), presenterContext);
        this.gameObject = (GameObjectNode)component.getParent();
        this.component = component;
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        return Status.OK_STATUS;
    }

}
