package com.dynamo.cr.goeditor;

import javax.inject.Inject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.goeditor.operations.AddComponentOperation;
import com.dynamo.cr.goeditor.operations.RemoveComponentOperation;
import com.google.protobuf.Message;

public class GameObjectPresenter implements IGameObjectView.Presenter {

    @Inject private GameObjectModel model;

    @Inject private IGameObjectView view;

    @Inject private IOperationHistory undoHistory;

    @Inject private IUndoContext undoContext;

    @Inject private ILogger logger;

    private void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.undoHistory.execute(operation, null, null);
        } catch (final ExecutionException e) {
            logger.logException(e);
        }

        if (status != Status.OK_STATUS) {
            logger.logException(status.getException());
        }
    }

    @Override
    public void onAddResourceComponent() {
        String resource = view.openAddResourceComponentDialog();
        if (resource != null) {
            AddComponentOperation operation = new AddComponentOperation(new ResourceComponent(resource), model);
            executeOperation(operation);
        }
    }

    @Override
    public void onAddEmbeddedComponent() {
        Message message = view.openAddEmbeddedComponentDialog();
        if (message != null) {
            AddComponentOperation operation = new AddComponentOperation(new EmbeddedComponent(message), model);
            executeOperation(operation);
        }
    }

    @Override
    public void onRemoveComponent(Component component) {
        RemoveComponentOperation operation = new RemoveComponentOperation(component, model);
        executeOperation(operation);
    }

}
