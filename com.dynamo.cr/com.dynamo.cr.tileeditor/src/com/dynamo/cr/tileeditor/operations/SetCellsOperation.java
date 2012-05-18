package com.dynamo.cr.tileeditor.operations;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.tileeditor.core.GridModel;
import com.dynamo.cr.tileeditor.core.Layer.Cell;

public class SetCellsOperation extends AbstractOperation {

    private final GridModel model;
    private final Map<Long, Cell> oldCells;
    private final Map<Long, Cell> cells;

    public SetCellsOperation(GridModel model, Map<Long, Cell> oldCells) {
        super("Paint Cells");
        this.model = model;
        this.cells = new HashMap<Long, Cell>(this.model.getCells());
        this.oldCells = new HashMap<Long, Cell>(oldCells);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setCells(this.cells);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setCells(this.cells);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        this.model.setCells(this.oldCells);
        return Status.OK_STATUS;
    }

}
