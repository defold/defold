package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;

public interface INodeView {

    public interface Presenter {
        void onSelect(IStructuredSelection selection);
        void onRefresh();

        void onLoad(String type, InputStream contents) throws IOException, CoreException;
    }

    void setRoot(Node root);
    void updateNode(Node node);

    void updateSelection(IStructuredSelection selection);

    String selectComponentType();
    String selectComponentFromFile();
}
