package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
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
        this.model.setRoot(this.manager.getPresenter(type).load(type, contents));
    }

    protected void doSave(Message message, OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
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

    @Override
    public void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException {
        Node node = this.model.getRoot();
        Message message = this.manager.getPresenter(node.getClass()).save(node, monitor);
        doSave(message, contents, monitor);
        this.model.setUndoRedoCounter(0);
    }

    public abstract Node load(String type, InputStream contents) throws IOException, CoreException;
    public abstract Message save(Node node, IProgressMonitor monitor) throws IOException, CoreException;
    public abstract Node create(String type) throws IOException, CoreException;

    protected final void logException(Throwable e) {
        this.logger.logException(e);
    }

    protected static Node load(NodeManager manager, IContainer contentRoot, String path) throws IOException, CoreException {
        return load(manager, contentRoot.getFile(new Path(path)));
    }

    protected static Node load(NodeManager manager, IFile f) throws IOException, CoreException {
        return load(manager, f.getFileExtension(), f.getContents());
    }

    protected static Node load(NodeManager manager, String type, InputStream is) throws IOException, CoreException {
        return manager.getPresenter(type).load(type, is);
    }
}
