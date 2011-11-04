package com.dynamo.cr.goprot.core;

import com.dynamo.cr.goprot.core.INodeView.Presenter;
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
    public void onSelect(Node[] nodes) {
        // TODO: Selection will probably be handled in another way
        for (Node node : nodes) {
            node.setSelected(true);
        }
    }

    @Override
    public void onRefresh() {
        this.view.updateNode(this.model.getRoot());
    }

}
