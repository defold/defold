package com.dynamo.cr.guieditor.operations;

import java.util.List;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.scene.EditorFontDesc;
import com.dynamo.cr.guieditor.scene.GuiScene;

public class DeleteFontsOperation extends AbstractOperation {


    private IGuiEditor editor;
    private List<EditorFontDesc> fonts;

    public DeleteFontsOperation(IGuiEditor editor, List<EditorFontDesc> fonts) {
        super("Delete Font(s)");
        this.editor = editor;
        this.fonts = fonts;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        scene.removeFonts(fonts);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        scene.removeFonts(fonts);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        GuiScene scene = editor.getScene();
        scene.addFonts(fonts);
        return Status.OK_STATUS;
    }

}
