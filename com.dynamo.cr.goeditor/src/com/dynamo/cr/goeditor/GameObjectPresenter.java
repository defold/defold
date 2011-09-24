package com.dynamo.cr.goeditor;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.goeditor.operations.AddComponentOperation;
import com.dynamo.cr.goeditor.operations.RemoveComponentOperation;
import com.dynamo.cr.goeditor.operations.SetComponentIdOperation;

public class GameObjectPresenter implements IGameObjectView.Presenter, IOperationHistoryListener {

    @Inject private GameObjectModel model;
    @Inject private IGameObjectView view;
    @Inject IResourceTypeRegistry resourceTypeRegistry;
    @Inject private IOperationHistory undoHistory;
    @Inject private IUndoContext undoContext;
    @Inject private ILogger logger;

    private int undoRedoCounter;

    @PreDestroy
    public void dispose() {
        undoHistory.removeOperationHistoryListener(this);
    }

    @Inject
    public void init() {
        undoHistory.addOperationHistoryListener(this);
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        int type = event.getEventType();
        boolean refresh = false;
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            ++undoRedoCounter;
            refresh = true;
            break;
        case OperationHistoryEvent.UNDONE:
            --undoRedoCounter;
            refresh = true;
            break;
        }

        if (refresh) {
            boolean dirty = isDirty();
            view.setDirty(dirty);
            refresh();
        }

    }

    private void refresh() {
        view.setComponents(model.getComponents());
    }

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
            AddComponentOperation operation = new AddComponentOperation(new ResourceComponent(resourceTypeRegistry, resource), model);
            executeOperation(operation);
        }
    }

    @Override
    public void onAddEmbeddedComponent() {

        IResourceType[] resourceTypes = resourceTypeRegistry.getResourceTypes();
        List<IResourceType> embedabbleTypes = new ArrayList<IResourceType>();
        for (IResourceType t : resourceTypes) {
            if (t.isEmbeddable()) {
                embedabbleTypes.add(t);
            }
        }

        IResourceType resourceType = view.openAddEmbeddedComponentDialog(embedabbleTypes.toArray(new IResourceType[embedabbleTypes.size()]));
        if (resourceType != null) {
            AddComponentOperation operation = new AddComponentOperation(new EmbeddedComponent(resourceTypeRegistry, resourceType.createTemplateMessage(), resourceType.getFileExtension()), model);
            executeOperation(operation);
        }
    }

    @Override
    public void onRemoveComponent(Component component) {
        RemoveComponentOperation operation = new RemoveComponentOperation(component, model);
        executeOperation(operation);
    }

    @Override
    public void onSetComponentId(Component component, String id) {
        SetComponentIdOperation operation = new SetComponentIdOperation(component, id, model);
        executeOperation(operation);
    }

    @Override
    public boolean isDirty() {
        return undoRedoCounter != 0;
    }

    @Override
    public void onSave(OutputStream output) throws IOException {
        model.save(output);
        undoRedoCounter = 0;
        view.setDirty(isDirty());
    }

    @Override
    public void onLoad(InputStream input) throws IOException {
        model.load(input);
        refresh();
    }

}
