package com.dynamo.cr.sceneed.ui;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.editor.core.ILogger;
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
    public void refreshView() {
        this.view.refresh(this.model.getSelection(), this.model.isDirty());
    }

    @Override
    public IStructuredSelection getSelection() {
        return this.model.getSelection();
    }

    @Override
    public void setSelection(IStructuredSelection selection) {
        this.model.setSelection(selection);
    }

    @Override
    public void executeOperation(IUndoableOperation operation) {
        this.model.executeOperation(operation);
    }

    @Override
    public String selectFromList(String title, String message, String... lst) {
        return this.view.selectFromList(title, message, lst);
    }

    @Override
    public Object selectFromArray(String title, String message, Object[] input, ILabelProvider labelProvider) {
        return this.view.selectFromArray(title, message, input, labelProvider);
    }

    @Override
    public String selectFile(String title) {
        return this.view.selectFile(title);
    }
}
