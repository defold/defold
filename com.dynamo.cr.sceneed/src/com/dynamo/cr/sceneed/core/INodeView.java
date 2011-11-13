package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

public interface INodeView {

    public interface Presenter {
        void onSelect(IStructuredSelection selection);
        void onRefresh();

        void onLoad(String type, InputStream contents) throws IOException, CoreException;
        void onSave(OutputStream contents, IProgressMonitor monitor) throws IOException, CoreException;
    }

    void setRoot(Node root);
    void updateNode(Node node);

    void updateSelection(IStructuredSelection selection);

    void setDirty(boolean dirty);

    String selectComponentType();
    String selectComponentFromFile();
}
