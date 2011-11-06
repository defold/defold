package com.dynamo.cr.goprot.core;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.goprot.INodeRenderer;

public class NodeManager {
    private Map<Class<? extends Node>, INodeView.Presenter> presenters;
    private INodeView.Presenter defaultPresenter;
    private Map<Class<? extends Node>, INodeRenderer> renderers;

    public NodeManager() {
        this.presenters = new HashMap<Class<? extends Node>, INodeView.Presenter>();
        this.renderers = new HashMap<Class<? extends Node>, INodeRenderer>();
    }

    public void registerNodeType(Class<? extends Node> c, INodeView.Presenter presenter, INodeRenderer renderer) {
        this.presenters.put(c, presenter);
        this.renderers.put(c, renderer);
    }

    public INodeView.Presenter getPresenter(Class<? extends Node> c) {
        return this.presenters.get(c);
    }

    public INodeView.Presenter getDefaultPresenter() {
        return this.defaultPresenter;
    }

    public void setDefaultPresenter(INodeView.Presenter defaultPresenter) {
        this.defaultPresenter = defaultPresenter;
    }

    public INodeRenderer getRenderer(Class<? extends Node> c) {
        return this.renderers.get(c);
    }
}
