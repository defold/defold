package com.dynamo.cr.tileeditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.Layer;

public class AddLayerOperation extends AbstractOperation {

    private final GridModel model;
    private final Layer layer;
    private final Layer prevSelectedLayer;

    public AddLayerOperation(GridModel model) {
        super("Add Layer");
        this.model = model;
        this.layer = model.createLayer();
        this.prevSelectedLayer = model.getSelectedLayer();
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.addLayer(this.layer);
        this.model.setSelectedLayer(this.layer);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.addLayer(this.layer);
        this.model.setSelectedLayer(this.layer);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.removeLayer(this.layer);
        this.model.setSelectedLayer(this.prevSelectedLayer);
        return Status.OK_STATUS;
    }

}
