package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.google.protobuf.Message;

public interface ISceneView {

    public interface Context extends ILogger {
        Node loadNode(String path) throws IOException, CoreException;
        Node loadNode(String type, InputStream contents) throws IOException, CoreException;
        Message buildMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException;
        ISceneView getView();
        IStructuredSelection getSelection();
        void executeOperation(IUndoableOperation operation);
        NodePresenter getPresenter(String nodeType);
    }

    public interface Presenter {
        void onSelect(IStructuredSelection selection);
        void onRefresh();

        void onLoad(String type, InputStream contents) throws IOException, CoreException;

        void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException;
        void onResourceChanged(IResourceChangeEvent event) throws CoreException;

        Context getContext();
    }

    public interface NodePresenter {
        Node onLoad(Context context, String type, InputStream contents) throws IOException, CoreException;
        Message onBuildMessage(Context context, Node node, IProgressMonitor monitor) throws IOException, CoreException;
        void onSaveMessage(Context context, Message message, OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException;
        Node onCreateNode(Context context, String type) throws IOException, CoreException;
    }

    void setRoot(Node root);
    void updateNode(Node node);

    void updateSelection(IStructuredSelection selection);

    void setDirty(boolean dirty);

    // TODO: Game object specific methods, how to extract into proper package?
    String selectComponentType();
    String selectComponentFromFile();
}
