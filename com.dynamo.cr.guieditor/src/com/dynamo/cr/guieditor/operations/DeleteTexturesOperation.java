package com.dynamo.cr.guieditor.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.scene.EditorTextureDesc;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class DeleteTexturesOperation extends AbstractOperation {


    private IGuiEditor editor;
    private List<EditorTextureDesc> textures;

    public DeleteTexturesOperation(IGuiEditor editor, List<EditorTextureDesc> textures) {
        super("Delete Node(s)");
        this.editor = editor;
        this.textures = textures;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (EditorTextureDesc textureDesc : textures) {
            scene.removeTexture(textureDesc.getName());
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (EditorTextureDesc textureDesc : textures) {
            scene.removeTexture(textureDesc.getName());
        }
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        for (EditorTextureDesc textureDesc : textures) {
            scene.addTexture(textureDesc.getName(), textureDesc.getTexture());
        }
        return Status.OK_STATUS;
    }

}
