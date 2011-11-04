package com.dynamo.cr.goprot.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;

import com.dynamo.cr.goprot.core.INodeView.Presenter;
import com.google.inject.Inject;

public abstract class NodePresenter implements Presenter, PropertyChangeListener {

    final protected NodeModel model;
    final private INodeView view;

    @Inject
    public NodePresenter(NodeModel model, INodeView view) {
        this.model = model;
        this.model.addListener(this);
        this.view = view;
    }

    @Override
    public void onSelect(Node node) {
        node.setSelected(true);
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        this.view.updateNode((Node)event.getSource());
    }

}
