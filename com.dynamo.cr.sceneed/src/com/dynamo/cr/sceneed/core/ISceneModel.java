package com.dynamo.cr.sceneed.core;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;

public interface ISceneModel extends ISceneWorld {

    Node getRoot();

    void setRoot(Node root);

    IStructuredSelection getSelection();

    void setSelection(IStructuredSelection selection);

    void executeOperation(IUndoableOperation operation);

    @Override
    IContainer getContentRoot();

    void handleResourceChanged(IResourceChangeEvent event)
            throws CoreException;

    IFile getFile(String path);

    Node loadNode(String path);

    void setUndoRedoCounter(int undoRedoCounter);

    void notifyChange(Node node);

}