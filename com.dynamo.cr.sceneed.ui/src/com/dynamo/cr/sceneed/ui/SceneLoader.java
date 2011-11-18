package com.dynamo.cr.sceneed.ui;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.sceneed.core.ISceneView;

class SceneLoader implements IRunnableWithProgress {

    IFile file;
    ISceneView.Presenter presenter;
    Throwable exception;

    public SceneLoader(IFile file, ISceneView.Presenter presenter) {
        this.file = file;
        this.presenter = presenter;
    }

    @Override
    public void run(IProgressMonitor monitor) {
        final int totalWork = 5;
        monitor.beginTask("Loading scene...", totalWork);
        try {
            this.presenter.onLoad(this.file.getFileExtension(), this.file.getContents());
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
