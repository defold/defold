package com.dynamo.cr.sceneed.core;

import javax.annotation.PreDestroy;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.core.ILogger;
import com.google.inject.Inject;

@Entity(commandFactory = NodeUndoableCommandFactory.class)
public class NodeModel implements INodeWorld, IAdaptable, IOperationHistoryListener {

    private Node root;
    private final INodeView view;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;
    private final IContainer contentRoot;
    private IStructuredSelection selection;
    private int undoRedoCounter;

    private static PropertyIntrospector<NodeModel, NodeModel> introspector = new PropertyIntrospector<NodeModel, NodeModel>(NodeModel.class, Messages.class);
    public static PropertyIntrospector<Node, NodeModel> nodeIntrospector = new PropertyIntrospector<Node, NodeModel>(Node.class);

    @Inject
    public NodeModel(INodeView view, IOperationHistory history, IUndoContext undoContext, ILogger logger, IContainer contentRoot) {
        this.view = view;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;
        this.contentRoot = contentRoot;
        this.selection = new StructuredSelection();
        this.undoRedoCounter = 0;
    }

    @PreDestroy
    public void dispose() {
        this.history.removeOperationHistoryListener(this);
    }

    @Inject
    public void init() {
        this.history.addOperationHistoryListener(this);
    }

    public Node getRoot() {
        return this.root;
    }

    public void setRoot(Node root) {
        if (this.root != root) {
            this.root = root;
            if (root != null) {
                root.setModel(this);
            }
            this.view.setRoot(root);
            if (root != null) {
                setSelection(new StructuredSelection(this.root));
            } else {
                setSelection(new StructuredSelection());
            }
            setUndoRedoCounter(0);
        }
    }

    public IStructuredSelection getSelection() {
        return this.selection;
    }

    public void setSelection(IStructuredSelection selection) {
        if (!this.selection.equals(selection)) {
            this.selection = selection;
            this.view.updateSelection(this.selection);
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

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<NodeModel, NodeModel>(this, this, introspector);
        }
        return null;
    }

    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }

    public void setUndoRedoCounter(int undoRedoCounter) {
        boolean prevDirty = this.undoRedoCounter != 0;
        boolean dirty = undoRedoCounter != 0;
        // NOTE: We must set undoRedoCounter before we call setDirty.
        // The "framework" might as for isDirty()
        this.undoRedoCounter = undoRedoCounter;
        if (prevDirty != dirty) {
            this.view.setDirty(dirty);
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }
        int type = event.getEventType();
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            setUndoRedoCounter(undoRedoCounter + 1);
            break;
        case OperationHistoryEvent.UNDONE:
            setUndoRedoCounter(undoRedoCounter - 1);
            break;
        }
    }

}
