package com.dynamo.cr.guieditor.operations;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.GuiSelectionProvider;
import com.dynamo.cr.guieditor.scene.GuiNode;

public class SelectOperation extends AbstractOperation {

    private GuiSelectionProvider selectionProvider;
    private ArrayList<GuiNode> newSelection;
    private List<GuiNode> originalSelection;

    public SelectOperation(GuiSelectionProvider selectionProvider, List<GuiNode> originalSelection, List<GuiNode> newSelection) {
        super("Select");
        this.selectionProvider = selectionProvider;
        this.originalSelection = originalSelection;
        this.newSelection = new ArrayList<GuiNode>(newSelection);
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        selectionProvider.setSelection(newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        selectionProvider.setSelection(newSelection);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        selectionProvider.setSelection(originalSelection);
        return Status.OK_STATUS;
    }

}
