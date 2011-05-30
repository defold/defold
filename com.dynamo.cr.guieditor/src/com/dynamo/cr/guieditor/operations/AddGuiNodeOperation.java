package com.dynamo.cr.guieditor.operations;

import java.util.Arrays;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.scene.GuiNode;

public class AddGuiNodeOperation extends AbstractOperation {


    private IGuiEditor editor;
    private GuiNode node;

    public AddGuiNodeOperation(IGuiEditor editor, GuiNode node) {
        super("Add Box Node");
        this.editor = editor;
        this.node = node;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        editor.getScene().addNodes(Arrays.asList(node));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        editor.getScene().addNodes(Arrays.asList(node));
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        editor.getScene().removeNodes(Arrays.asList(node));
        return Status.OK_STATUS;
    }

}
