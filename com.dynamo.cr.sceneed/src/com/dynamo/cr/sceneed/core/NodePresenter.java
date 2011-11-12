package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeView.Presenter;
import com.google.inject.Inject;

public abstract class NodePresenter implements Presenter {

    @Inject protected NodeModel model;
    @Inject protected INodeView view;
    @Inject protected NodeLoader loader;
    @Inject private ILogger logger;

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
        this.model.setRoot(this.loader.load(type, contents));
    }

    public abstract Node load(String type, InputStream contents) throws IOException, CoreException;
    public abstract Node create(String type) throws IOException, CoreException;

    protected final void logException(Throwable e) {
        this.logger.logException(e);
    }

}
