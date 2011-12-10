package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;

public class AddComponentOperation extends AbstractSelectOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component, IPresenterContext presenterContext) {
        super("Add Component", component, presenterContext);
        this.gameObject = gameObject;
        this.component = component;
        String id = "";
        if (component instanceof RefComponentNode) {
            String path = ((RefComponentNode)component).getComponent();
            int index = path.lastIndexOf('.');
            if (index >= 0) {
                id = path.substring(index + 1);
            }
        } else {
            ComponentTypeNode componentType = (ComponentTypeNode)component.getChildren().get(0);
            id = this.gameObject.getModel().getExtension(componentType.getClass());
        }
        id = NodeUtil.getUniqueId(this.gameObject, id, new NodeUtil.IdFetcher<Node>() {
            @Override
            public String getId(Node child) {
                return ((ComponentNode)child).getId();
            }
        });
        this.component.setId(id);
    }

    @Override
    protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        return Status.OK_STATUS;
    }

    @Override
    protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        return Status.OK_STATUS;
    }

}
