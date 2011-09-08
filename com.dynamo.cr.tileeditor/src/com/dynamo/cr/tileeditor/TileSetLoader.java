package com.dynamo.cr.tileeditor;

import java.io.InputStreamReader;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;
import com.dynamo.tile.proto.Tile.TileSet;
import com.google.protobuf.TextFormat;

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
            InputStreamReader reader = new InputStreamReader(this.file.getContents());
            TileSet.Builder builder = TileSet.newBuilder();

            try {
                TextFormat.merge(reader, builder);
                monitor.worked(1);
                TileSet tileSet = builder.build();
                this.presenter.load(tileSet);
            } finally {
                reader.close();
            }
        } catch (Throwable e) {
            this.exception = e;
        }
    }
}
