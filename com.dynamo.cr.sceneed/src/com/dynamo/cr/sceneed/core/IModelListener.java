package com.dynamo.cr.sceneed.core;

import org.eclipse.jface.viewers.IStructuredSelection;

public interface IModelListener {

    void rootChanged(Node root);

    void selectionChanged(IStructuredSelection selection);

    void nodeChanged(Node node);

    void dirtyChanged(boolean dirty);

}
