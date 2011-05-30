package com.dynamo.cr.guieditor.operations;

import java.util.Arrays;
import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class PasteGuiNodesOperation extends AbstractOperation {


    private IGuiEditor editor;
    private List<GuiNode> nodes;

    public PasteGuiNodesOperation(IGuiEditor editor, List<GuiNode> nodes) {
        super("Paste Node(s)");
        this.editor = editor;
        this.nodes = nodes;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (GuiNode node : nodes) {
            scene.addNodes(Arrays.asList(node));
        }

        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (GuiNode node : nodes) {
            scene.addNodes(Arrays.asList(node));
        }

        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (GuiNode node : nodes) {
            scene.removeNodes(Arrays.asList(node));
        }

        return Status.OK_STATUS;
    }

}
