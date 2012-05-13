package com.dynamo.cr.tileeditor;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.tileeditor.core.IGridView;

class GridLoader implements IRunnableWithProgress {

    IFile file;
    IGridView.Presenter presenter;
    Throwable exception;

    public GridLoader(IFile file, IGridView.Presenter presenter) {
        this.file = file;
        this.presenter = presenter;
    }

    @Override
    public void run(IProgressMonitor monitor) {
        final int totalWork = 5;
        monitor.beginTask("Loading Tile Grid...", totalWork);
        try {
            this.presenter.onLoad(this.file.getContents());
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
