package com.dynamo.cr.goprot.core;

import java.util.HashMap;
import java.util.Map;

public class NodeManager {
    @SuppressWarnings("rawtypes")
    private Map<Class, INodeView.Presenter> presenters;

    @SuppressWarnings("rawtypes")
    public NodeManager() {
        this.presenters = new HashMap<Class, INodeView.Presenter>();
    }

    @SuppressWarnings("rawtypes")
    public void registerPresenter(Class c, INodeView.Presenter presenter) {
        this.presenters.put(c, presenter);
    }

    @SuppressWarnings("rawtypes")
    public INodeView.Presenter getPresenter(Class c) {
        return this.presenters.get(c);
    }
}
