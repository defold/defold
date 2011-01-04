package com.dynamo.cr.contenteditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.contenteditor.editors.IEditor;
import com.dynamo.cr.contenteditor.scene.Node;

public class DeleteOperation extends AbstractOperation {

    private Node[] nodes;
    private Node[] oldParents;

    public DeleteOperation(IEditor editor, Node[] nodes) {
        super("Delete GameObject");
        this.nodes = new Node[nodes.length];
        this.oldParents = new Node[nodes.length];
        for (int i = 0; i < nodes.length; ++i) {
            this.nodes[i] = nodes[i];
            this.oldParents[i] = nodes[i].getParent();
        }
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (int i = 0; i < nodes.length; ++i) {
            oldParents[i] = nodes[i].getParent();
            nodes[i].getParent().removeNode(nodes[i]);
        }

        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        return execute(monitor, info);
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        for (int i = 0; i < nodes.length; ++i) {
            oldParents[i].addNode(nodes[i]);
        }
        return Status.OK_STATUS;
    }

}
