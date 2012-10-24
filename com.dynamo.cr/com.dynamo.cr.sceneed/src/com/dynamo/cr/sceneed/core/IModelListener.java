package com.dynamo.cr.sceneed.core;

import org.eclipse.jface.viewers.IStructuredSelection;

public interface IModelListener {

    void rootChanged(Node root);

    void stateChanging(IStructuredSelection selection);
    void stateChanged(IStructuredSelection selection, boolean dirty);

}
