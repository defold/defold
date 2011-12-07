package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.editor.core.ILogger;

public interface ISceneView {

    public interface IPresenter {
        void onSelect(IStructuredSelection selection);
        void onRefresh();

        void onLoad(String type, InputStream contents) throws IOException, CoreException;

        void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException;
        void onResourceChanged(IResourceChangeEvent event) throws CoreException;
    }

    public interface IPresenterContext extends ILogger {
        ISceneView getView();
        IStructuredSelection getSelection();
        void executeOperation(IUndoableOperation operation);
    }

    public interface INodePresenter<T extends Node> {}

    void setRoot(Node root);
    void updateNode(Node node);

    void updateSelection(IStructuredSelection selection);

    void setDirty(boolean dirty);

    String selectFromList(String title, String message, String... lst);

    // TODO: Game object specific methods, how to extract into proper package?
    String selectComponentType();
    String selectComponentFromFile();
}
