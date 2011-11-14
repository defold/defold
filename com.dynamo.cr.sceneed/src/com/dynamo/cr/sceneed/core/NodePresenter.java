package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeView.Presenter;
import com.google.inject.Inject;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public abstract class NodePresenter implements Presenter {

    @Inject protected NodeModel model;
    @Inject protected INodeView view;
    @Inject protected IContainer contentRoot;
    @Inject private ILogger logger;
    @Inject private NodeManager manager;

    @Override
    public void onSelect(IStructuredSelection selection) {
        this.model.setSelection(selection);
    }

    @Override
    public void onRefresh() {
        this.view.setRoot(this.model.getRoot());
        this.view.updateSelection(this.model.getSelection());
    }

    @Override
    public final void onLoad(String type, InputStream contents) throws IOException, CoreException {
        this.model.setRoot(this.manager.getPresenter(type).doLoad(type, contents));
    }

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        Message message = this.manager.getPresenter(node.getClass()).buildMessage(node, monitor);
        saveMessage(message, contents, monitor);
        this.model.setUndoRedoCounter(0);
    }

    @Override
    public void onResourceChanged(IResourceChangeEvent event) throws CoreException {
        this.model.handleResourceChanged(event);
    }

    public abstract Node doLoad(String type, InputStream contents) throws IOException, CoreException;
    public abstract Message buildMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException;
    public abstract Node createNode(String type) throws IOException, CoreException;

    public Node loadNode(String path) throws IOException, CoreException {
        return loadNode(this.contentRoot.getFile(new Path(path)));
    }

    public Node loadNode(IFile f) throws IOException, CoreException {
        return loadNode(f.getFileExtension(), f.getContents());
    }

    public Node loadNode(String type, InputStream is) throws IOException, CoreException {
        NodePresenter presenter = this.manager.getPresenter(type);
        if (presenter != null) {
            return presenter.doLoad(type, is);
        } else {
            return null;
        }
    }

    protected void saveMessage(Message message, OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        try {
            OutputStreamWriter writer = new OutputStreamWriter(contents);
            try {
                TextFormat.print(message, writer);
                writer.flush();
            } finally {
                writer.close();
            }
        } catch (IOException e) {
            throw e;
        }
    }

    protected final void logException(Throwable e) {
        this.logger.logException(e);
    }

}
