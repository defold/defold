package com.dynamo.cr.sceneed.ui;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.ILogger;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.google.inject.Inject;

public class PresenterContext implements
com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext {

    private final ISceneModel model;
    private final ISceneView view;
    private final ILogger logger;

    @Inject
    public PresenterContext(ISceneModel model, ISceneView view, ILogger logger) {
        this.model = model;
        this.view = view;
        this.logger = logger;
    }

    @Override
    public void logException(Throwable exception) {
        this.logger.logException(exception);
    }

    @Override
    public ISceneView getView() {
        return this.view;
    }

    @Override
    public IStructuredSelection getSelection() {
        return this.model.getSelection();
    }

    @Override
    public void executeOperation(IUndoableOperation operation) {
        this.model.executeOperation(operation);
    }

}
