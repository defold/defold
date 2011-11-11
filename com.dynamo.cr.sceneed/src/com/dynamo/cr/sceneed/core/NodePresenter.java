package com.dynamo.cr.sceneed.core;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeView.Presenter;
import com.google.inject.Inject;

public abstract class NodePresenter implements Presenter {

    final protected NodeModel model;
    final protected INodeView view;

    @Inject
    public NodePresenter(NodeModel model, INodeView view) {
        this.model = model;
        this.view = view;
    }

    @Override
    public void onSelect(IStructuredSelection selection) {
        this.model.setSelection(selection);
    }

    @Override
    public void onRefresh() {
        this.view.setRoot(this.model.getRoot());
        this.view.updateSelection(this.model.getSelection());
    }

}
