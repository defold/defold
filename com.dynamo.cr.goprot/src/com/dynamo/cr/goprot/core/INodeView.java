package com.dynamo.cr.goprot.core;

public interface INodeView {

    public interface Presenter {
        void onSelect(Node node);
    }

    void updateNode(Node node);
}
