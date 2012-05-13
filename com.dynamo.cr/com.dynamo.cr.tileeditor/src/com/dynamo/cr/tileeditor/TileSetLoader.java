package com.dynamo.cr.tileeditor;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;

class TileSetLoader implements IRunnableWithProgress {

    IFile file;
    TileSetPresenter presenter;
    Throwable exception;

    public TileSetLoader(IFile file, TileSetPresenter presenter) {
        this.file = file;
        this.presenter = presenter;
    }

    @Override
    public void run(IProgressMonitor monitor) {
        final int totalWork = 5;
        monitor.beginTask("Loading Tile Set...", totalWork);
        try {
            this.presenter.load(this.file.getContents());
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
