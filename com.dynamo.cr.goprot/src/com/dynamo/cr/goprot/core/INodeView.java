package com.dynamo.cr.goprot.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.jface.viewers.IStructuredSelection;

public interface INodeView {

    public interface Presenter {
        void onSelect(IStructuredSelection selection);
        void onRefresh();

        void onLoad(InputStream contents) throws IOException;
    }

    void setRoot(Node root);
    void updateNode(Node node);

    void updateSelection(IStructuredSelection selection);
}
