package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.ILabelProvider;
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
        // Model interface
        IStructuredSelection getSelection();
        void setSelection(IStructuredSelection selection);
        void executeOperation(IUndoableOperation operation);

        // View interface
        void refreshView();
        void refreshRenderView();
        void asyncExec(Runnable runnable);
        String selectFromList(String title, String message, String... lst);
        Object selectFromArray(String title, String message, Object[] input, ILabelProvider labelProvider);
        String selectFile(String title);
    }

    public interface INodePresenter<T extends Node> {}

    void setRoot(Node root);
    void refresh(IStructuredSelection selection, boolean dirty);
    void refreshRenderView(IStructuredSelection selection);
    void asyncExec(Runnable runnable);

    String selectFromList(String title, String message, String... lst);
    Object selectFromArray(String title, String message, Object[] input, ILabelProvider labelProvider);
    String selectFile(String title);
}
