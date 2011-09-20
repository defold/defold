package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;

import javax.inject.Inject;

import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.runtime.IStatus;

public class GridPresenter implements IGridView.Presenter, PropertyChangeListener {

    private final GridModel model;
    private final IGridView view;

    private boolean loading = false;
    private int undoRedoCounter = 0;

    @Inject
    public GridPresenter(GridModel model, IGridView view, IOperationHistory undoHistory, IUndoContext undoContext, ILogger logger) {
        this.model = model;
        this.view = view;

        this.model.addTaggedPropertyListener(this);
    }

    public void refresh() {
        this.view.setTileSetProperty(this.model.getTileSet());
        this.view.setCellWidthProperty(this.model.getCellWidth());
        this.view.setCellHeightProperty(this.model.getCellHeight());
        this.view.setLayers(this.model.getLayers());
        this.view.refreshProperties();
    }

    private void setUndoRedoCounter(int undoRedoCounter) {
        boolean prevDirty = this.undoRedoCounter != 0;
        boolean dirty = undoRedoCounter != 0;
        if (prevDirty != dirty) {
            this.view.setDirty(dirty);
        }
        this.undoRedoCounter = undoRedoCounter;
    }

    @SuppressWarnings("unchecked")
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        if (loading)
            return;

        this.view.setValid(this.model.isOk());
        this.view.refreshProperties();
        if (evt.getNewValue() instanceof IStatus) {
        }
        else {
            if (evt.getSource() instanceof GridModel) {
                if (evt.getPropertyName().equals("tileSet")) {
                    view.setTileSetProperty((String)evt.getNewValue());
                } else if (evt.getPropertyName().equals("cellWidth")) {
                    view.setCellWidthProperty((Float)evt.getNewValue());
                } else if (evt.getPropertyName().equals("cellHeight")) {
                    view.setCellHeightProperty((Float)evt.getNewValue());
                } else if (evt.getPropertyName().equals("layers")) {
                    view.setLayers((List<GridModel.Layer>)evt.getNewValue());
                }
            }
        }
    }

    @Override
    public void onTileSelected(int tileIndex) {
        // TODO Auto-generated method stub

    }

    @Override
    public void onLoad(InputStream is) {
        this.loading = true;
        this.model.load(is);
        this.loading = false;
        refresh();
        setUndoRedoCounter(0);
    }

    @Override
    public void onSave(OutputStream os) {
        // TODO Auto-generated method stub

    }

}
