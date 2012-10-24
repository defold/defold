package com.dynamo.cr.sceneed.ui;

import javax.vecmath.Point3d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class PresenterContext implements
com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext {

    private final ISceneModel model;
    private final ISceneView view;

    @Inject
    public PresenterContext(ISceneModel model, ISceneView view) {
        this.model = model;
        this.view = view;
    }

    @Override
    public void refreshRenderView() {
        this.view.refreshRenderView();
    }

    @Override
    public void asyncExec(Runnable runnable) {
        this.view.asyncExec(runnable);
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
    public String selectFile(String title, String[] extensions) {
        return this.view.selectFile(title, extensions);
    }

    @Override
    public void getCameraFocusPoint(Point3d focusPoint) {
        this.view.getCameraFocusPoint(focusPoint);
    }

    @Override
    public INodeType getNodeType(Class<? extends Node> nodeClass) {
        return this.model.getNodeType(nodeClass);
    }
}
