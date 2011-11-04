package com.dynamo.cr.goprot.core;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.goprot.core.ILogger;
import com.google.inject.Inject;

public class NodeModel {
    private Node root;
    private final INodeView view;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;

    @Inject
    public NodeModel(INodeView view, IOperationHistory history, IUndoContext undoContext, ILogger logger) {
        this.view = view;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;
    }

    public Node getRoot() {
        return this.root;
    }

    public void setRoot(Node root) {
        if (this.root != root) {
            this.root = root;
            root.setModel(this);
            notifyChange(root);
        }
    }

    public void notifyChange(Node node) {
        this.view.updateNode(node);
    }

    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.history.execute(operation, null, null);
        } catch (final ExecutionException e) {
            this.logger.logException(e);
        }

        if (status != Status.OK_STATUS) {
            this.logger.logException(status.getException());
        }
    }
}
