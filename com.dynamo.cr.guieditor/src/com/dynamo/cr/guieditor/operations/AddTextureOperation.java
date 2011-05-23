package com.dynamo.cr.guieditor.operations;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.scene.GuiScene;

public class AddTextureOperation extends AbstractOperation {

    private GuiScene scene;
    private String name;
    private String texture;

    public AddTextureOperation(GuiScene scene, String name, String texture) {
        super("Add Texture");
        this.scene = scene;
        this.name = name;
        this.texture = texture;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        scene.addTexture(name, texture);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        scene.addTexture(name, texture);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        scene.removeTexture(name);
        return Status.OK_STATUS;
    }

}
