package com.dynamo.cr.sceneed;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.sceneed.core.INodeView;

class NodeLoaderRunnable implements IRunnableWithProgress {

    IFile file;
    INodeView.Presenter presenter;
    Throwable exception;
    String type;

    public NodeLoaderRunnable(IFile file, INodeView.Presenter presenter, String type) {
        this.file = file;
        this.presenter = presenter;
        this.type = type;
    }

    @Override
    public void run(IProgressMonitor monitor) {
        final int totalWork = 5;
        monitor.beginTask("Loading Game Object...", totalWork);
        try {
            this.presenter.onLoad(this.file.getContents());
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
