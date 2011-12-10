package com.dynamo.cr.go.core.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeUtil;

public class AddComponentOperation extends AbstractOperation {

    final private GameObjectNode gameObject;
    final private ComponentNode component;
    final private IStructuredSelection oldSelection;

    public AddComponentOperation(GameObjectNode gameObject, ComponentNode component) {
        super("Add Component");
        this.gameObject = gameObject;
        this.component = component;
        this.oldSelection = this.gameObject.getModel().getSelection();
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
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        this.gameObject.getModel().setSelection(new StructuredSelection(this.component));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.addComponent(this.component);
        this.gameObject.getModel().setSelection(new StructuredSelection(this.component));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.gameObject.removeComponent(this.component);
        this.gameObject.getModel().setSelection(this.oldSelection);
        return Status.OK_STATUS;
    }

}
