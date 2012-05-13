package com.dynamo.cr.guieditor.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class BringForwardOperation extends AbstractOperation {

    private List<GuiNode> nodes;
    private GuiScene scene;
    private List<GuiNode> undoNodes;

    public BringForwardOperation(GuiScene scene, List<GuiNode> nodes) {
        super("Bring Forward");
        this.scene = scene;
        this.nodes = nodes;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        undoNodes = scene.bringForward(nodes);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        undoNodes = scene.bringForward(nodes);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        scene.sendBackward(undoNodes);
        return Status.OK_STATUS;
    }

}
