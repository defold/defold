package com.dynamo.cr.goprot.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.List;

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
    private List<PropertyChangeListener> listeners;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;

    @Inject
    public NodeModel(IOperationHistory history, IUndoContext undoContext, ILogger logger) {
        this.listeners = new ArrayList<PropertyChangeListener>();
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
        }
    }

    public void addListener(PropertyChangeListener listener) {
        if (!this.listeners.contains(listener)) {
            this.listeners.add(listener);
        }
    }

    public void removeListener(PropertyChangeListener listener) {
        this.listeners.remove(listener);
    }

    public void firePropertyChangeEvent(PropertyChangeEvent propertyChangeEvent) {
        for (PropertyChangeListener listener : this.listeners) {
            listener.propertyChange(propertyChangeEvent);
        }
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
