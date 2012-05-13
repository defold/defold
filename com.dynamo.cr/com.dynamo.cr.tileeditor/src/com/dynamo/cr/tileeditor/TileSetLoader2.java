package com.dynamo.cr.tileeditor;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.sceneed.core.ISceneView;

class TileSetLoader2 implements IRunnableWithProgress {

    IFile file;
    ISceneView.IPresenter presenter;
    Throwable exception;

    public TileSetLoader2(IFile file, ISceneView.IPresenter presenter) {
        this.file = file;
        this.presenter = presenter;
    }

    @Override
    public void run(IProgressMonitor monitor) {
        final int totalWork = 5;
        monitor.beginTask("Loading Tile Set...", totalWork);
        try {
            this.presenter.onLoad(this.file.getFileExtension(), this.file.getContents());
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
