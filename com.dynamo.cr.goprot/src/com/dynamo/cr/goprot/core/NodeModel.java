package com.dynamo.cr.goprot.core;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.goprot.core.ILogger;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.google.inject.Inject;

@Entity(commandFactory = NodeUndoableCommandFactory.class)
public class NodeModel implements INodeWorld, IAdaptable {

    private Node root;
    private final INodeView view;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;
    private final IContainer contentRoot;

    private static PropertyIntrospector<NodeModel, NodeModel> introspector = new PropertyIntrospector<NodeModel, NodeModel>(NodeModel.class, Messages.class);
    public static PropertyIntrospector<Node, NodeModel> nodeIntrospector = new PropertyIntrospector<Node, NodeModel>(Node.class);

    @Inject
    public NodeModel(INodeView view, IOperationHistory history, IUndoContext undoContext, ILogger logger, IContainer contentRoot) {
        this.view = view;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;
        this.contentRoot = contentRoot;
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
}
