package com.dynamo.cr.sceneed.core;

import java.io.IOException;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.IPropertyObjectWorld;

public interface ISceneModel extends IPropertyObjectWorld {

    Node getRoot();

    void setRoot(Node root);

    IStructuredSelection getSelection();

    void setSelection(IStructuredSelection selection);

    void executeOperation(IUndoableOperation operation);

    void handleResourceChanged(IResourceChangeEvent event)
            throws CoreException;

    IFile getFile(String path);

    Node loadNode(String path) throws IOException, CoreException;

    void setUndoRedoCounter(int undoRedoCounter);

    void notifyChange(Node node);

    Image getImage(Class<? extends Node> nodeClass);

}